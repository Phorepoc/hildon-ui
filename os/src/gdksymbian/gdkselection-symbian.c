/* gdkselection-symbian.c -- selections of the Symbian backend: an owner table in this process (selection atom -> owning window) and the X selection protocol carried by GDK events between
 * windows of this process: SELECTION_REQUEST to the owner, the owner answers with gdk_property_change on the requestor's "GDK_SELECTION" property and gdk_selection_send_notify, which
 * delivers SELECTION_NOTIFY to the requestor; ownership changes send SELECTION_CLEAR to the previous owner. (Other Symbian processes would need a server for it; GtkEntry/GtkTextView copy
 * and paste inside one application are this.) BSD-2-Clause (Phorepoc). */
#include "config.h"
#include <string.h>
#include "gdksymbian.h"
#include "gdkselection.h"
#include "gdkproperty.h"
#include "gdkprivate.h"
#include "gdkinternals.h"

typedef struct { GdkAtom selection; GdkWindow *owner; } OwnerInfo;
static GSList *owner_list = NULL;

static void
send_selection_event (GdkWindow *win, GdkEventType type, GdkAtom selection, GdkAtom target, GdkAtom property, GdkNativeWindow requestor, guint32 time)
{
  GdkEvent *ev = gdk_event_new (type);
  ev->selection.window = g_object_ref (win); ev->selection.send_event = FALSE;
  ev->selection.selection = selection; ev->selection.target = target; ev->selection.property = property; ev->selection.requestor = requestor; ev->selection.time = time;
  gdk_event_put (ev); gdk_event_free (ev);
}

/* called when a window is destroyed: it no longer owns anything */
void
_gdk_selection_window_destroyed (GdkWindow *window)
{
  GSList *l = owner_list;
  while (l) { OwnerInfo *info = l->data; l = l->next; if (info->owner == window) { owner_list = g_slist_remove (owner_list, info); g_free (info); } }
}

gboolean
gdk_selection_owner_set_for_display (GdkDisplay *display, GdkWindow *owner, GdkAtom selection, guint32 time, gboolean send_event)
{
  GSList *l; OwnerInfo *info; GdkWindow *old = NULL;
  (void) display; (void) send_event;
  g_return_val_if_fail (selection != GDK_NONE, FALSE);
  for (l = owner_list; l; l = l->next) {
    info = l->data;
    if (info->selection == selection) { old = info->owner; owner_list = g_slist_remove (owner_list, info); g_free (info); break; }
  }
  if (owner) { info = g_new (OwnerInfo, 1); info->owner = owner; info->selection = selection; owner_list = g_slist_prepend (owner_list, info); }
  if (old && old != owner && !GDK_WINDOW_DESTROYED (old)) send_selection_event (old, GDK_SELECTION_CLEAR, selection, GDK_NONE, GDK_NONE, 0, time);
  return TRUE;
}

GdkWindow *
gdk_selection_owner_get_for_display (GdkDisplay *display, GdkAtom selection)
{
  GSList *l; (void) display;
  for (l = owner_list; l; l = l->next) { OwnerInfo *info = l->data; if (info->selection == selection) return info->owner; }
  return NULL;
}

/* the property of the requestor that the owner writes the converted value to */
static GdkAtom selection_property (void) { return gdk_atom_intern_static_string ("GDK_SELECTION"); }

void
gdk_selection_convert (GdkWindow *requestor, GdkAtom selection, GdkAtom target, guint32 time)
{
  GdkWindow *owner;
  g_return_if_fail (GDK_IS_WINDOW (requestor));
  owner = gdk_selection_owner_get_for_display (_gdk_display, selection);
  if (owner && !GDK_WINDOW_DESTROYED (owner))
    send_selection_event (owner, GDK_SELECTION_REQUEST, selection, target, selection_property (), (GdkNativeWindow) (gsize) requestor, time);
  else                                                                          /* nobody owns it: the conversion failed (property NONE) */
    send_selection_event (requestor, GDK_SELECTION_NOTIFY, selection, target, GDK_NONE, 0, time);
}

gint
gdk_selection_property_get (GdkWindow *requestor, guchar **data, GdkAtom *ret_type, gint *ret_format)
{
  guchar *t = NULL; GdkAtom prop_type; gint prop_format, prop_len = 0;
  g_return_val_if_fail (requestor == NULL || GDK_IS_WINDOW (requestor), 0);
  if (!requestor) requestor = _gdk_parent_root;
  if (GDK_WINDOW_DESTROYED (requestor) || !gdk_property_get (requestor, selection_property (), GDK_NONE, 0, 0x1fffffff, FALSE, &prop_type, &prop_format, &prop_len, &t)) {
    if (ret_type) *ret_type = GDK_NONE;
    if (ret_format) *ret_format = 0;
    if (data) *data = NULL;
    return 0;
  }
  if (ret_type) *ret_type = prop_type;
  if (ret_format) *ret_format = prop_format;
  if (data) *data = t; else g_free (t);
  return prop_len;
}

void
gdk_selection_send_notify_for_display (GdkDisplay *display, GdkNativeWindow requestor, GdkAtom selection, GdkAtom target, GdkAtom property, guint32 time)
{
  GdkWindow *win = gdk_window_lookup_for_display (display, requestor);
  if (win && !GDK_WINDOW_DESTROYED (win)) send_selection_event (win, GDK_SELECTION_NOTIFY, selection, target, property, requestor, time);
}
