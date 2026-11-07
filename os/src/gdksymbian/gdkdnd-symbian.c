/* gdkdnd-symbian.c -- drag and drop of the Symbian backend: the local protocol (GDK_DRAG_PROTO_LOCAL), a drag between windows of this process, carried by GDK events:
 * the source context announces the destination with GDK_DRAG_ENTER / MOTION / LEAVE / DROP_START (events to the destination window, which holds a mirror context), the destination answers
 * with gdk_drag_status (GDK_DRAG_STATUS to the source) and gdk_drop_finish (GDK_DROP_FINISHED); the data travels through the "LocalDndSelection" selection of gtkselection. Destinations are the
 * toplevels that registered with gdk_window_register_dnd. (Other Symbian processes would need a server for it.) BSD-2-Clause (Phorepoc). */
#include "config.h"
#include <string.h>
#include "gdksymbian.h"
#include "gdkdnd.h"
#include "gdkproperty.h"
#include "gdkprivate.h"
#include "gdkinternals.h"

typedef enum { DRAG_STATUS_DRAG, DRAG_STATUS_MOTION_WAIT, DRAG_STATUS_ACTION_WAIT, DRAG_STATUS_DROP } DragStatus;

typedef struct {
  GdkAtom local_selection;
  gint last_x, last_y;               /* root coordinates of the last motion */
  guint drag_status : 4;
  gboolean drop_ok;                  /* the destination finished the drop successfully (source side, gdk_drag_drop_succeeded) */
} GdkDragContextPrivate;

static GList *contexts = NULL;
static GdkDragContext *current_dest_drag = NULL;       /* the mirror context the destination currently holds */

#define PRIV(ctx) ((GdkDragContextPrivate *) GDK_DRAG_CONTEXT (ctx)->windowing_data)

G_DEFINE_TYPE (GdkDragContext, gdk_drag_context, G_TYPE_OBJECT)

static void
gdk_drag_context_init (GdkDragContext *dragcontext)
{
  dragcontext->windowing_data = g_new0 (GdkDragContextPrivate, 1);
  contexts = g_list_prepend (contexts, dragcontext);
}

static void
gdk_drag_context_finalize (GObject *object)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (object);
  g_list_free (context->targets);
  if (context->source_window) g_object_unref (context->source_window);
  if (context->dest_window) g_object_unref (context->dest_window);
  g_free (context->windowing_data);
  contexts = g_list_remove (contexts, context);
  G_OBJECT_CLASS (gdk_drag_context_parent_class)->finalize (object);
}
static void gdk_drag_context_class_init (GdkDragContextClass *klass) { G_OBJECT_CLASS (klass)->finalize = gdk_drag_context_finalize; }

GdkDragContext *gdk_drag_context_new (void) { return g_object_new (gdk_drag_context_get_type (), NULL); }
void gdk_drag_context_ref (GdkDragContext *context) { g_object_ref (context); }
void gdk_drag_context_unref (GdkDragContext *context) { g_object_unref (context); }

/* the source context (or destination mirror) of a drag between @source and @dest */
static GdkDragContext *
context_find (gboolean is_source, GdkWindow *source, GdkWindow *dest)
{
  GList *l;
  for (l = contexts; l; l = l->next) {
    GdkDragContext *c = l->data;
    if ((!c->is_source == !is_source) && (source == NULL || (c->source_window && c->source_window == source)) && (dest == NULL || (c->dest_window && c->dest_window == dest))) return c;
  }
  return NULL;
}

static void
put_dnd_event (GdkEventType type, GdkWindow *window, GdkDragContext *context, guint32 time, gboolean send_event, gint x_root, gint y_root)
{
  GdkEvent *ev = gdk_event_new (type);
  ev->dnd.window = g_object_ref (window); ev->dnd.send_event = send_event; ev->dnd.context = g_object_ref (context); ev->dnd.time = time; ev->dnd.x_root = (gshort) x_root; ev->dnd.y_root = (gshort) y_root;
  gdk_event_put (ev); gdk_event_free (ev);
}

static void
local_send_leave (GdkDragContext *context, guint32 time)
{
  if (current_dest_drag != NULL && current_dest_drag->protocol == GDK_DRAG_PROTO_LOCAL && current_dest_drag->source_window == context->source_window) {
    GdkDragContext *dest = current_dest_drag; current_dest_drag = NULL;
    put_dnd_event (GDK_DRAG_LEAVE, context->dest_window, dest, time, FALSE, 0, 0);
    g_object_unref (dest);                                               /* the event holds its own reference */
  }
}

static void
local_send_enter (GdkDragContext *context, guint32 time)
{
  GdkDragContext *nc;
  if (!PRIV (context)->local_selection) PRIV (context)->local_selection = gdk_atom_intern_static_string ("LocalDndSelection");
  if (current_dest_drag != NULL) { g_object_unref (current_dest_drag); current_dest_drag = NULL; }
  nc = gdk_drag_context_new ();
  nc->protocol = GDK_DRAG_PROTO_LOCAL; nc->is_source = FALSE;
  nc->source_window = g_object_ref (context->source_window); nc->dest_window = g_object_ref (context->dest_window);
  nc->targets = g_list_copy (context->targets); nc->actions = context->actions;
  gdk_window_set_events (nc->source_window, gdk_window_get_events (nc->source_window) | GDK_PROPERTY_CHANGE_MASK);
  PRIV (nc)->local_selection = PRIV (context)->local_selection;
  current_dest_drag = nc;
  put_dnd_event (GDK_DRAG_ENTER, context->dest_window, nc, time, FALSE, 0, 0);
}

static void
local_send_motion (GdkDragContext *context, gint x_root, gint y_root, GdkDragAction action, guint32 time)
{
  if (current_dest_drag != NULL && current_dest_drag->protocol == GDK_DRAG_PROTO_LOCAL && current_dest_drag->source_window == context->source_window) {
    current_dest_drag->suggested_action = action; current_dest_drag->actions = action;
    PRIV (current_dest_drag)->last_x = x_root; PRIV (current_dest_drag)->last_y = y_root;
    PRIV (context)->drag_status = DRAG_STATUS_MOTION_WAIT;
    put_dnd_event (GDK_DRAG_MOTION, current_dest_drag->dest_window, current_dest_drag, time, FALSE, x_root, y_root);
  }
}

static void
local_send_drop (GdkDragContext *context, guint32 time)
{
  if (current_dest_drag != NULL && current_dest_drag->protocol == GDK_DRAG_PROTO_LOCAL && current_dest_drag->source_window == context->source_window)
    put_dnd_event (GDK_DROP_START, current_dest_drag->dest_window, current_dest_drag, time, FALSE, PRIV (current_dest_drag)->last_x, PRIV (current_dest_drag)->last_y);
}

static void
do_leave (GdkDragContext *context, guint32 time)
{
  if (context->dest_window) {
    if (context->protocol == GDK_DRAG_PROTO_LOCAL) local_send_leave (context, time);
    g_object_unref (context->dest_window); context->dest_window = NULL;
  }
}

GdkDragContext *
gdk_drag_begin (GdkWindow *window, GList *targets)
{
  GdkDragContext *nc; GList *l;
  g_return_val_if_fail (window != NULL, NULL);
  nc = gdk_drag_context_new ();
  nc->is_source = TRUE; nc->source_window = g_object_ref (window); nc->targets = NULL; nc->actions = 0;
  for (l = targets; l; l = l->next) nc->targets = g_list_append (nc->targets, l->data);
  return nc;
}

/* a window is a drop destination when it registered for it (gdk_window_register_dnd); its protocol is the local one */
guint32
gdk_drag_get_protocol_for_display (GdkDisplay *display, guint32 xid, GdkDragProtocol *protocol)
{
  GdkWindow *window = gdk_window_lookup_for_display (display, (GdkNativeWindow) xid);
  if (window && GPOINTER_TO_INT (gdk_drawable_get_data (GDK_DRAWABLE (window), "gdk-dnd-registered"))) { *protocol = GDK_DRAG_PROTO_LOCAL; return xid; }
  *protocol = GDK_DRAG_PROTO_NONE;
  return 0;
}

void
gdk_window_register_dnd (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  if (GPOINTER_TO_INT (gdk_drawable_get_data (GDK_DRAWABLE (window), "gdk-dnd-registered"))) return;
  gdk_drawable_set_data (GDK_DRAWABLE (window), "gdk-dnd-registered", GINT_TO_POINTER (TRUE), NULL);
}

/* the topmost registered toplevel under (x_root, y_root), not the drag icon window */
void
gdk_drag_find_window_for_screen (GdkDragContext *context, GdkWindow *drag_window, GdkScreen *screen, gint x_root, gint y_root, GdkWindow **dest_window, GdkDragProtocol *protocol)
{
  GList *l; GdkWindow *found = NULL;
  g_return_if_fail (context != NULL); (void) screen;
  *dest_window = NULL; *protocol = GDK_DRAG_PROTO_NONE;
  for (l = g_list_last (_gdk_symbian_toplevel_stack ()); l; l = l->prev) {
    GdkWindow *w = l->data; gint ox, oy, ww, wh;
    if (w == drag_window) continue;
    gdk_window_get_origin (w, &ox, &oy); gdk_drawable_get_size (GDK_DRAWABLE (w), &ww, &wh);
    if (x_root >= ox && y_root >= oy && x_root < ox + ww && y_root < oy + wh) { found = w; break; }          /* the window on top at that point decides */
  }
  if (found && gdk_drag_get_protocol_for_display (_gdk_display, (guint32) (gsize) found, protocol)) { *dest_window = g_object_ref (found); return; }
  *protocol = GDK_DRAG_PROTO_NONE;
}

gboolean
gdk_drag_motion (GdkDragContext *context, GdkWindow *dest_window, GdkDragProtocol protocol, gint x_root, gint y_root, GdkDragAction suggested_action, GdkDragAction possible_actions, guint32 time)
{
  GdkDragContextPrivate *private;
  g_return_val_if_fail (context != NULL, FALSE);
  (void) possible_actions;
  private = PRIV (context);
  if (context->dest_window != dest_window) {
    do_leave (context, time);                                              /* a leave to the last destination */
    private->drag_status = DRAG_STATUS_DRAG;
    if (dest_window) {
      context->dest_window = g_object_ref (dest_window); context->protocol = protocol;
      if (protocol == GDK_DRAG_PROTO_LOCAL) local_send_enter (context, time);
      context->suggested_action = suggested_action;
    } else { context->dest_window = NULL; context->action = 0; }
    put_dnd_event (GDK_DRAG_STATUS, context->source_window, context, time, TRUE, 0, 0);       /* a synthetic status: the destination changed */
  } else context->suggested_action = suggested_action;
  private->last_x = x_root; private->last_y = y_root;
  if (context->dest_window) {
    if (private->drag_status == DRAG_STATUS_DRAG) {
      if (context->protocol == GDK_DRAG_PROTO_LOCAL) local_send_motion (context, x_root, y_root, suggested_action, time);
      else if (context->protocol == GDK_DRAG_PROTO_NONE) g_warning ("GDK_DRAG_PROTO_NONE is not valid in gdk_drag_motion()");
    } else return TRUE;                                                    /* still waiting for the status of the last motion */
  }
  return FALSE;
}

void
gdk_drag_drop (GdkDragContext *context, guint32 time)
{
  g_return_if_fail (context != NULL);
  if (!context->dest_window) return;
  if (context->protocol == GDK_DRAG_PROTO_LOCAL) local_send_drop (context, time);
  else if (context->protocol == GDK_DRAG_PROTO_NONE) g_warning ("GDK_DRAG_PROTO_NONE is not valid in gdk_drag_drop()");
}

void gdk_drag_abort (GdkDragContext *context, guint32 time) { g_return_if_fail (context != NULL); do_leave (context, time); }

void
gdk_drag_status (GdkDragContext *context, GdkDragAction action, guint32 time)
{
  GdkDragContext *src;
  g_return_if_fail (context != NULL);
  src = context_find (TRUE, context->source_window, context->dest_window);
  if (!src) return;
  if (PRIV (src)->drag_status == DRAG_STATUS_MOTION_WAIT) PRIV (src)->drag_status = DRAG_STATUS_DRAG;
  src->action = action;
  put_dnd_event (GDK_DRAG_STATUS, src->source_window, src, time, FALSE, 0, 0);
}

/* the reply of a destination to a drop-start is part of the XDND handshake only; the local protocol answers with gdk_drop_finish */
void gdk_drop_reply (GdkDragContext *context, gboolean ok, guint32 time) { g_return_if_fail (context != NULL); (void) ok; (void) time; }

void
gdk_drop_finish (GdkDragContext *context, gboolean success, guint32 time)
{
  GdkDragContext *src;
  g_return_if_fail (context != NULL);
  src = context_find (TRUE, context->source_window, context->dest_window);
  if (!src) return;
  PRIV (src)->drop_ok = success;
  put_dnd_event (GDK_DROP_FINISHED, src->source_window, src, time, FALSE, 0, 0);
}

gboolean gdk_drag_drop_succeeded (GdkDragContext *context) { g_return_val_if_fail (context != NULL, FALSE); return PRIV (context)->drop_ok; }

GdkAtom
gdk_drag_get_selection (GdkDragContext *context)
{
  g_return_val_if_fail (context != NULL, GDK_NONE);
  return context->protocol == GDK_DRAG_PROTO_LOCAL ? PRIV (context)->local_selection : GDK_NONE;
}
