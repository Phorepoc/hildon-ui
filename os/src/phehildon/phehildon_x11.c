/* phehildon_x11.c -- Xlib / gdkx.h for libhildon over the Symbian GDK backend (see shims/phe_xlib.h). One process owns every window and the backend is the window manager, so the
 * "server" operations are direct operations on GDK windows, atoms and properties. BSD-2-Clause (Phorepoc). */
#include <string.h>
#include "phe_xlib.h"

GdkFilterReturn gdk_symbian_apply_filters (GdkWindow *window, gpointer xevent, GdkEvent *event);
gboolean gdk_symbian_wm_client_message (GdkWindow *target, GdkAtom message_type, const glong *data, gint format);       /* phegdk (gdkevents-symbian.c) */

static GdkWindow *win_of (Window w) { return gdk_window_lookup ((GdkNativeWindow) w); }
static GdkAtom ga (Atom a) { return GUINT_TO_POINTER ((guint) a); }

Atom
XInternAtom (Display *display, const char *atom_name, Bool only_if_exists)
{
  (void) display;
  return (Atom) GPOINTER_TO_UINT (gdk_atom_intern (atom_name, only_if_exists));
}

Atom gdk_x11_get_xatom_by_name_for_display (GdkDisplay *display, const gchar *atom_name) { (void) display; return XInternAtom (NULL, atom_name, False); }
Atom gdk_x11_get_xatom_by_name (const gchar *atom_name) { return XInternAtom (NULL, atom_name, False); }
GdkAtom gdk_x11_xatom_to_atom (Atom xatom) { return ga (xatom); }
Atom gdk_x11_atom_to_xatom (GdkAtom atom) { return (Atom) GPOINTER_TO_UINT (atom); }
/* the backend is its own window manager and supports every hint it is asked about (it keeps them itself) */
gboolean gdk_x11_screen_supports_net_wm_hint (GdkScreen *screen, GdkAtom property) { (void) screen; (void) property; return TRUE; }

int
XChangeProperty (Display *display, Window w, Atom property, Atom type, int format, int mode, const unsigned char *data, int nelements)
{
  GdkWindow *win = win_of (w);
  (void) display;
  if (!win) return BadWindow;
  gdk_property_change (win, ga (property), ga (type), format, mode == PropModeAppend ? GDK_PROP_MODE_APPEND : mode == PropModePrepend ? GDK_PROP_MODE_PREPEND : GDK_PROP_MODE_REPLACE, data, nelements);
  return Success;
}

int
XGetWindowProperty (Display *display, Window w, Atom property, glong long_offset, glong long_length, Bool delete_prop, Atom req_type,
                    Atom *actual_type_return, int *actual_format_return, gulong *nitems_return, gulong *bytes_after_return, unsigned char **prop_return)
{
  GdkWindow *win = win_of (w); GdkAtom type; gint fmt = 0, len = 0; guchar *data = NULL;
  (void) display;
  *actual_type_return = None; *actual_format_return = 0; *nitems_return = 0; *bytes_after_return = 0; *prop_return = NULL;
  if (!win) return BadWindow;
  if (!gdk_property_get (win, ga (property), ga (req_type), long_offset, long_length * 4, delete_prop, &type, &fmt, &len, &data)) return Success;     /* no such property: type None */
  *actual_type_return = (Atom) GPOINTER_TO_UINT (type); *actual_format_return = fmt; *nitems_return = fmt ? (gulong) len / (fmt / 8) : 0; *prop_return = data;
  return Success;
}

int
XDeleteProperty (Display *display, Window w, Atom property)
{
  GdkWindow *win = win_of (w);
  (void) display;
  if (!win) return BadWindow;
  gdk_property_delete (win, ga (property));
  return Success;
}

/* as Xlib: the WM_TRANSIENT_FOR property (a window id); a window transient for the root window is one that belongs to the desktop layer (Home applets) */
int
XSetTransientForHint (Display *display, Window w, Window prop_window)
{
  unsigned long id = (unsigned long) prop_window;
  return XChangeProperty (display, w, XA_WM_TRANSIENT_FOR, XA_WINDOW, 32, PropModeReplace, (const unsigned char *) &id, 1);
}

Status
XSetWMProtocols (Display *display, Window w, Atom *protocols, int count)
{
  GdkWindow *win = win_of (w);
  (void) display;
  if (!win) return 0;
  gdk_property_change (win, gdk_atom_intern_static_string ("WM_PROTOCOLS"), ga (XA_ATOM), 32, GDK_PROP_MODE_REPLACE, (const guchar *) protocols, count);
  return 1;
}

Status
XGetWMProtocols (Display *display, Window w, Atom **protocols_return, int *count_return)
{
  Atom type; int fmt; gulong n, after; unsigned char *data;
  if (XGetWindowProperty (display, w, XInternAtom (display, "WM_PROTOCOLS", False), 0, 1000000, False, XA_ATOM, &type, &fmt, &n, &after, &data) != Success || type != XA_ATOM) return 0;
  *protocols_return = (Atom *) data; *count_return = (int) n;
  return 1;
}

/* the window group is the one hint of WM_HINTS the backend keeps (gdk_window_set_group) */
XWMHints *
XGetWMHints (Display *display, Window w)
{
  GdkWindow *win = win_of (w), *group; XWMHints *h;
  (void) display;
  if (!win) return NULL;
  group = gdk_window_get_group (win);
  if (!group) return NULL;
  h = g_new0 (XWMHints, 1); h->flags = WindowGroupHint; h->window_group = GDK_WINDOW_XID (group);
  return h;
}

/* a client message sent to a window of this process is delivered to it; one sent to the root window has no window manager to receive it but the backend (the root's event mask is the WM's) */
Status
XSendEvent (Display *display, Window w, Bool propagate, glong event_mask, XEvent *event_send)
{
  GdkEvent *ev; GdkWindow *win = win_of (w); gint i;
  (void) display; (void) propagate; (void) event_mask;
  if (!win || event_send->type != ClientMessage) return 0;
  /* a message to the root window is a request to the window manager (the backend) */
  if (gdk_symbian_wm_client_message (win, ga (event_send->xclient.message_type), event_send->xclient.data.l, event_send->xclient.format)) return 1;
  ev = gdk_event_new (GDK_CLIENT_EVENT);
  ev->client.window = g_object_ref (win); ev->client.send_event = TRUE; ev->client.message_type = ga (event_send->xclient.message_type); ev->client.data_format = event_send->xclient.format;
  for (i = 0; i < 5; i++) ev->client.data.l[i] = event_send->xclient.data.l[i];
  /* the filters of the target window see the message first, as the native event it is */
  if (gdk_symbian_apply_filters (win, event_send, ev) == GDK_FILTER_REMOVE) { gdk_event_free (ev); return 1; }
  gdk_event_put (ev); gdk_event_free (ev);
  return 1;
}

/* blocks (running the main loop) until a client message the predicate accepts has arrived; the message is taken out of the queue */
int
XIfEvent (Display *display, XEvent *event_return, Bool (*predicate) (Display *, XEvent *, char *), char *arg)
{
  GList *keep = NULL, *l;
  for (;;) {
    GdkEvent *ev;
    while ((ev = gdk_event_get ()) != NULL) {
      if (ev->type == GDK_CLIENT_EVENT) {
        XEvent xe; gint i; memset (&xe, 0, sizeof xe);
        xe.type = ClientMessage; xe.xclient.display = display; xe.xclient.window = GDK_WINDOW_XID (ev->client.window); xe.xclient.message_type = (Atom) GPOINTER_TO_UINT (ev->client.message_type);
        xe.xclient.format = ev->client.data_format; for (i = 0; i < 5; i++) xe.xclient.data.l[i] = ev->client.data.l[i];
        if (predicate (display, &xe, arg)) { *event_return = xe; gdk_event_free (ev); for (l = keep; l; l = l->next) { gdk_event_put (l->data); gdk_event_free (l->data); } g_list_free (keep); return Success; }
      }
      keep = g_list_append (keep, ev);                                   /* every other event goes back for the toolkit */
    }
    for (l = keep; l; l = l->next) { gdk_event_put (l->data); gdk_event_free (l->data); }
    g_list_free (keep); keep = NULL;
    g_main_context_iteration (NULL, TRUE);
  }
}

int XSync (Display *display, Bool discard) { (void) discard; gdk_display_sync ((GdkDisplay *) display); return 0; }
int XFlush (Display *display) { gdk_display_flush ((GdkDisplay *) display); return 0; }
int XFree (void *data) { g_free (data); return 1; }
Window XDefaultRootWindow (Display *display) { (void) display; return GDK_ROOT_WINDOW (); }
