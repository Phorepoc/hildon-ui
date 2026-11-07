/* gdkwinhints-symbian.c -- the window manager facing part of the GDK window API (type hints, transients, states, decorations, geometry hints, drags) and the remaining public GDK entry points
 * that have no window-system content (devices, error traps, wide characters, spawning). There is no window manager on this system, so the backend is the window manager: it acts on every hint
 * itself, with the stacking layers and the focus choice of gdkwindow-symbian.c / gdkevents-symbian.c. BSD-2-Clause (Phorepoc). */
#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include "gdksymbian.h"
#include "gdkinput.h"
#include "gdkprivate.h"
#include "gdkinternals.h"
#include "gdkscreen.h"
#include "gdkspawn.h"
#include "gdkdisplay.h"
#include "gdkkeysyms.h"
#include <glib/gstdio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#define WI(w) ((GdkWindowImplSymbian *) GDK_WINDOW_OBJECT (w)->impl)
#define TOPLEVEL_P(w) (GDK_WINDOW_OBJECT (w)->parent == NULL || GDK_WINDOW_OBJECT (GDK_WINDOW_OBJECT (w)->parent)->window_type == GDK_WINDOW_ROOT)

static void
transient_gone (gpointer data, GObject *where)
{
  GdkWindowImplSymbian *impl = data;
  if (impl->transient_for == (GdkWindow *) where) impl->transient_for = NULL;
}

/* ---- type hint: the layer and the focus behaviour that go with it ---- */
void
gdk_window_set_type_hint (GdkWindow *window, GdkWindowTypeHint hint)
{
  GdkWindowImplSymbian *impl;
  g_return_if_fail (GDK_IS_WINDOW (window));
  if (GDK_WINDOW_DESTROYED (window)) return;
  impl = WI (window); impl->type_hint = hint;
  switch (hint) {
    case GDK_WINDOW_TYPE_HINT_POPUP_MENU: case GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU: case GDK_WINDOW_TYPE_HINT_COMBO: case GDK_WINDOW_TYPE_HINT_TOOLTIP:
    case GDK_WINDOW_TYPE_HINT_NOTIFICATION: case GDK_WINDOW_TYPE_HINT_DND:
      impl->accept_focus = FALSE; impl->layer = 1; break;                         /* transient pop-ups: above the application windows, no keyboard focus */
    case GDK_WINDOW_TYPE_HINT_SPLASHSCREEN: case GDK_WINDOW_TYPE_HINT_DOCK:
      impl->layer = 1; break;
    default: break;
  }
  if (impl->mapped && TOPLEVEL_P (window)) _gdk_symbian_stack_raise (window);
}
GdkWindowTypeHint gdk_window_get_type_hint (GdkWindow *window) { g_return_val_if_fail (GDK_IS_WINDOW (window), GDK_WINDOW_TYPE_HINT_NORMAL); return WI (window)->type_hint; }

/* ---- transient-for and modal: a transient window is kept above the window it belongs to ---- */
void
gdk_window_set_transient_for (GdkWindow *window, GdkWindow *parent)
{
  GdkWindowImplSymbian *impl;
  g_return_if_fail (GDK_IS_WINDOW (window)); g_return_if_fail (GDK_IS_WINDOW (parent));
  if (GDK_WINDOW_DESTROYED (window) || GDK_WINDOW_DESTROYED (parent)) return;
  impl = WI (window);
  if (impl->transient_for) g_object_weak_unref (G_OBJECT (impl->transient_for), transient_gone, impl);
  impl->transient_for = gdk_window_get_toplevel (parent);
  g_object_weak_ref (G_OBJECT (impl->transient_for), transient_gone, impl);
  if (impl->mapped && TOPLEVEL_P (window)) _gdk_symbian_stack_raise (impl->transient_for), _gdk_symbian_stack_raise (window);
}

void
gdk_window_set_modal_hint (GdkWindow *window, gboolean modal)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  if (GDK_WINDOW_DESTROYED (window)) return;
  WI (window)->modal = modal;
  if (modal && WI (window)->mapped && TOPLEVEL_P (window)) _gdk_symbian_stack_raise (window);
}

void
gdk_window_set_keep_above (GdkWindow *window, gboolean setting)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  if (GDK_WINDOW_DESTROYED (window)) return;
  WI (window)->layer = setting ? 1 : 0;
  gdk_synthesize_window_state (window, setting ? GDK_WINDOW_STATE_BELOW : GDK_WINDOW_STATE_ABOVE, setting ? GDK_WINDOW_STATE_ABOVE : 0);
  if (WI (window)->mapped && TOPLEVEL_P (window)) _gdk_symbian_stack_raise (window);
}

void
gdk_window_set_keep_below (GdkWindow *window, gboolean setting)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  if (GDK_WINDOW_DESTROYED (window)) return;
  WI (window)->layer = setting ? -1 : 0;
  gdk_synthesize_window_state (window, setting ? GDK_WINDOW_STATE_ABOVE : GDK_WINDOW_STATE_BELOW, setting ? GDK_WINDOW_STATE_BELOW : 0);
  if (WI (window)->mapped && TOPLEVEL_P (window)) { if (setting) _gdk_symbian_stack_lower (window); else _gdk_symbian_stack_raise (window); gdk_window_invalidate_rect (window, NULL, TRUE); }
}

/* ---- plain attributes the backend keeps for the application and for its own focus / stacking choices ---- */
void gdk_window_set_accept_focus (GdkWindow *window, gboolean accept_focus) { g_return_if_fail (GDK_IS_WINDOW (window)); WI (window)->accept_focus = accept_focus; }
void gdk_window_set_focus_on_map (GdkWindow *window, gboolean focus_on_map) { g_return_if_fail (GDK_IS_WINDOW (window)); WI (window)->focus_on_map = focus_on_map; }
void gdk_window_set_override_redirect (GdkWindow *window, gboolean override_redirect) { g_return_if_fail (GDK_IS_WINDOW (window)); WI (window)->override_redirect = override_redirect; }
void gdk_window_set_skip_taskbar_hint (GdkWindow *window, gboolean skips_taskbar) { g_return_if_fail (GDK_IS_WINDOW (window)); WI (window)->skip_taskbar = skips_taskbar; }
void gdk_window_set_skip_pager_hint (GdkWindow *window, gboolean skips_pager) { g_return_if_fail (GDK_IS_WINDOW (window)); WI (window)->skip_pager = skips_pager; }
void gdk_window_set_urgency_hint (GdkWindow *window, gboolean urgent) { g_return_if_fail (GDK_IS_WINDOW (window)); WI (window)->urgent = urgent; }
void gdk_window_set_role (GdkWindow *window, const gchar *role) { g_return_if_fail (GDK_IS_WINDOW (window)); g_free (WI (window)->role); WI (window)->role = g_strdup (role); }
void gdk_window_set_startup_id (GdkWindow *window, const gchar *startup_id) { g_return_if_fail (GDK_IS_WINDOW (window)); g_free (WI (window)->startup_id); WI (window)->startup_id = g_strdup (startup_id); }

void
gdk_window_set_icon_list (GdkWindow *window, GList *pixbufs)
{
  GdkWindowImplSymbian *impl; GList *l, *copy = NULL;
  g_return_if_fail (GDK_IS_WINDOW (window));
  impl = WI (window);
  for (l = pixbufs; l; l = l->next) copy = g_list_append (copy, g_object_ref (l->data));
  g_list_foreach (impl->icon_list, (GFunc) g_object_unref, NULL); g_list_free (impl->icon_list);
  impl->icon_list = copy;
}

void
gdk_window_set_decorations (GdkWindow *window, GdkWMDecoration decorations)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  WI (window)->decorations = decorations; WI (window)->has_decorations = TRUE;
}

gboolean
gdk_window_get_decorations (GdkWindow *window, GdkWMDecoration *decorations)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);
  if (!WI (window)->has_decorations) return FALSE;
  if (decorations) *decorations = WI (window)->decorations;
  return TRUE;
}

void
gdk_window_set_functions (GdkWindow *window, GdkWMFunction functions)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  WI (window)->functions = functions; WI (window)->has_functions = TRUE;
}

/* the title and icon name are kept in the window and published as the properties a window manager would read (WM_NAME / _NET_WM_NAME, WM_ICON_NAME / _NET_WM_ICON_NAME) */
static void
set_text_property (GdkWindow *window, const gchar *name_utf8, const gchar *value)
{
  gdk_property_change (window, gdk_atom_intern_static_string (name_utf8), gdk_atom_intern_static_string ("UTF8_STRING"), 8, GDK_PROP_MODE_REPLACE, (const guchar *) value, strlen (value));
}

void
gdk_window_set_title (GdkWindow *window, const gchar *title)
{
  g_return_if_fail (GDK_IS_WINDOW (window)); g_return_if_fail (title != NULL);
  if (GDK_WINDOW_DESTROYED (window)) return;
  g_free (WI (window)->title); WI (window)->title = g_strdup (title);
  set_text_property (window, "_NET_WM_NAME", title);
  if (!WI (window)->icon_name) set_text_property (window, "_NET_WM_ICON_NAME", title);
}

void
gdk_window_set_icon_name (GdkWindow *window, const gchar *name)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  if (GDK_WINDOW_DESTROYED (window)) return;
  g_free (WI (window)->icon_name); WI (window)->icon_name = g_strdup (name);
  if (name) set_text_property (window, "_NET_WM_ICON_NAME", name);
}

void
gdk_window_set_icon (GdkWindow *window, GdkWindow *icon_window, GdkPixmap *pixmap, GdkBitmap *mask)
{
  GdkWindowImplSymbian *impl;
  g_return_if_fail (GDK_IS_WINDOW (window)); g_return_if_fail (icon_window == NULL || GDK_IS_WINDOW (icon_window));
  g_return_if_fail (pixmap == NULL || GDK_IS_PIXMAP (pixmap)); g_return_if_fail (mask == NULL || GDK_IS_PIXMAP (mask));
  if (GDK_WINDOW_DESTROYED (window)) return;
  impl = WI (window);
  if (pixmap) g_object_ref (pixmap); if (mask) g_object_ref (mask);
  if (impl->icon_pixmap) g_object_unref (impl->icon_pixmap); if (impl->icon_mask) g_object_unref (impl->icon_mask);
  impl->icon_pixmap = pixmap; impl->icon_mask = mask; impl->icon_window = icon_window;
}

static void group_gone (gpointer data, GObject *where) { GdkWindowImplSymbian *impl = data; if (impl->group == (GdkWindow *) where) impl->group = NULL; }

void
gdk_window_set_group (GdkWindow *window, GdkWindow *leader)
{
  GdkWindowImplSymbian *impl;
  g_return_if_fail (GDK_IS_WINDOW (window)); g_return_if_fail (leader == NULL || GDK_IS_WINDOW (leader));
  if (GDK_WINDOW_DESTROYED (window) || (leader && GDK_WINDOW_DESTROYED (leader))) return;
  impl = WI (window);
  if (impl->group) g_object_weak_unref (G_OBJECT (impl->group), group_gone, impl);
  impl->group = leader;
  if (leader) g_object_weak_ref (G_OBJECT (leader), group_gone, impl);
}

/* every toplevel belongs to the display's default window group (X11: the display's leader window, set in WM_HINTS at creation) unless gdk_window_set_group () said otherwise */
static GdkWindow *default_leader = NULL;
static GdkWindow *
get_default_leader (void)
{
  if (!default_leader) {
    GdkWindowAttr a; memset (&a, 0, sizeof a);
    a.window_type = GDK_WINDOW_TOPLEVEL; a.wclass = GDK_INPUT_OUTPUT; a.width = a.height = 1; a.x = a.y = -10;
    default_leader = gdk_window_new (NULL, &a, GDK_WA_X | GDK_WA_Y);                    /* never mapped */
  }
  return default_leader;
}

GdkWindow *
gdk_window_get_group (GdkWindow *window)
{
  GdkWindowImplSymbian *impl;
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);
  impl = WI (window);
  if (impl->group) return impl->group;
  if (GDK_WINDOW_OBJECT (window)->window_type == GDK_WINDOW_TOPLEVEL && !(default_leader && window == default_leader)) return get_default_leader ();
  return NULL;
}

/* the old WM hints call: position / size hints in one, applied as geometry hints */
void
gdk_window_set_hints (GdkWindow *window, gint x, gint y, gint min_width, gint min_height, gint max_width, gint max_height, gint flags)
{
  GdkGeometry g; GdkWindowHints mask = 0;
  g_return_if_fail (GDK_IS_WINDOW (window)); (void) x; (void) y;
  memset (&g, 0, sizeof g);
  if (flags & GDK_HINT_MIN_SIZE) { g.min_width = min_width; g.min_height = min_height; mask |= GDK_HINT_MIN_SIZE; }
  if (flags & GDK_HINT_MAX_SIZE) { g.max_width = max_width; g.max_height = max_height; mask |= GDK_HINT_MAX_SIZE; }
  gdk_window_set_geometry_hints (window, &g, mask);
}

/* the window has no frame of its own: the rectangle of the frame is the one of the window */
void
gdk_window_get_root_origin (GdkWindow *window, gint *x, gint *y)
{
  GdkRectangle r;
  g_return_if_fail (GDK_IS_WINDOW (window));
  gdk_window_get_frame_extents (window, &r);
  if (x) *x = r.x;
  if (y) *y = r.y;
}

void
gdk_window_get_frame_extents (GdkWindow *window, GdkRectangle *rect)
{
  GdkWindow *top;
  g_return_if_fail (GDK_IS_WINDOW (window)); g_return_if_fail (rect != NULL);
  top = gdk_window_get_toplevel (window);
  gdk_window_get_origin (top, &rect->x, &rect->y);
  gdk_drawable_get_size (GDK_DRAWABLE (top), &rect->width, &rect->height);
}

/* the limits of the geometry hints are enforced in move_resize (gdkwindow-symbian.c); setting them applies them to the present size at once */
void
gdk_window_set_geometry_hints (GdkWindow *window, const GdkGeometry *geometry, GdkWindowHints geom_mask)
{
  GdkWindowImplSymbian *impl;
  g_return_if_fail (GDK_IS_WINDOW (window)); g_return_if_fail (geometry != NULL || geom_mask == 0);
  if (GDK_WINDOW_DESTROYED (window)) return;
  impl = WI (window);
  if (geometry) impl->geometry = *geometry;
  impl->geom_mask = geom_mask;
  if (TOPLEVEL_P (window) && (geom_mask & (GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE | GDK_HINT_RESIZE_INC))) {
    gint w, h; gdk_drawable_get_size (GDK_DRAWABLE (window), &w, &h);
    gdk_window_resize (window, w, h);
  }
}

/* there is no configure handshake with a window manager, so the resize that was announced is final the moment it is made */
void gdk_window_enable_synchronized_configure (GdkWindow *window) { g_return_if_fail (GDK_IS_WINDOW (window)); }
void gdk_window_configure_finished (GdkWindow *window) { g_return_if_fail (GDK_IS_WINDOW (window)); }

/* opacity has no window server meaning here (gdk_display_supports_composite is FALSE); like GDK's own X11 backend it records the request as the property a compositing manager would read */
void
gdk_window_set_opacity (GdkWindow *window, gdouble opacity)
{
  guint32 cardinal;
  g_return_if_fail (GDK_IS_WINDOW (window));
  if (opacity < 0) opacity = 0; else if (opacity > 1) opacity = 1;
  cardinal = (guint32) (opacity * 0xffffffffu);
  gdk_property_change (window, gdk_atom_intern_static_string ("_NET_WM_WINDOW_OPACITY"), gdk_atom_intern_static_string ("CARDINAL"), 32, GDK_PROP_MODE_REPLACE, (guchar *) &cardinal, 1);
}

/* ---- focus, states ---- */
void
gdk_window_focus (GdkWindow *window, guint32 timestamp)
{
  GdkWindow *top;
  g_return_if_fail (GDK_IS_WINDOW (window));
  if (GDK_WINDOW_DESTROYED (window)) return;
  top = gdk_window_get_toplevel (window); (void) timestamp;
  WI (top)->skip_focus_once = FALSE;
  if (!WI (top)->mapped) return;
  _gdk_symbian_stack_raise (top); gdk_window_invalidate_rect (top, NULL, TRUE);
  _gdk_symbian_focus_update ();
}

void
gdk_window_iconify (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  if (GDK_WINDOW_DESTROYED (window)) return;
  if (!TOPLEVEL_P (window)) return;
  gdk_window_hide (window);
  gdk_synthesize_window_state (window, GDK_WINDOW_STATE_WITHDRAWN, GDK_WINDOW_STATE_ICONIFIED);
}

void
gdk_window_deiconify (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  if (GDK_WINDOW_DESTROYED (window)) return;
  gdk_window_show (window);
  gdk_synthesize_window_state (window, GDK_WINDOW_STATE_ICONIFIED, 0);
}

void gdk_window_stick (GdkWindow *window) { g_return_if_fail (GDK_IS_WINDOW (window)); gdk_synthesize_window_state (window, 0, GDK_WINDOW_STATE_STICKY); }
void gdk_window_unstick (GdkWindow *window) { g_return_if_fail (GDK_IS_WINDOW (window)); gdk_synthesize_window_state (window, GDK_WINDOW_STATE_STICKY, 0); }

static void
to_screen_size (GdkWindow *window, GdkWindowState state)
{
  GdkWindowImplSymbian *impl = WI (window); gint x, y, w, h;
  if (GDK_WINDOW_DESTROYED (window) || !TOPLEVEL_P (window)) return;
  if (!(gdk_window_get_state (window) & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN))) {
    gdk_window_get_origin (window, &x, &y); gdk_drawable_get_size (GDK_DRAWABLE (window), &w, &h);
    impl->save_x = x; impl->save_y = y; impl->save_w = w; impl->save_h = h; impl->saved_valid = TRUE;
  }
  gdk_window_move_resize (window, 0, 0, gdk_screen_get_width (gdk_screen_get_default ()), gdk_screen_get_height (gdk_screen_get_default ()));
  gdk_synthesize_window_state (window, 0, state);
}

static void
from_screen_size (GdkWindow *window, GdkWindowState state)
{
  GdkWindowImplSymbian *impl = WI (window);
  if (GDK_WINDOW_DESTROYED (window) || !(gdk_window_get_state (window) & state)) return;
  gdk_synthesize_window_state (window, state, 0);
  if (impl->saved_valid && !(gdk_window_get_state (window) & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN))) {
    gdk_window_move_resize (window, impl->save_x, impl->save_y, impl->save_w, impl->save_h); impl->saved_valid = FALSE;
  }
}

void gdk_window_maximize (GdkWindow *window) { g_return_if_fail (GDK_IS_WINDOW (window)); to_screen_size (window, GDK_WINDOW_STATE_MAXIMIZED); }
void gdk_window_unmaximize (GdkWindow *window) { g_return_if_fail (GDK_IS_WINDOW (window)); from_screen_size (window, GDK_WINDOW_STATE_MAXIMIZED); }
void gdk_window_fullscreen (GdkWindow *window) { g_return_if_fail (GDK_IS_WINDOW (window)); to_screen_size (window, GDK_WINDOW_STATE_FULLSCREEN); }
void gdk_window_unfullscreen (GdkWindow *window) { g_return_if_fail (GDK_IS_WINDOW (window)); from_screen_size (window, GDK_WINDOW_STATE_FULLSCREEN); }

/* ---- interactive move and resize: the backend follows the pointer itself until the button goes up (gdkevents-symbian.c) ---- */
void
gdk_window_begin_resize_drag (GdkWindow *window, GdkWindowEdge edge, gint button, gint root_x, gint root_y, guint32 timestamp)
{
  g_return_if_fail (GDK_IS_WINDOW (window)); (void) button; (void) timestamp;
  if (GDK_WINDOW_DESTROYED (window)) return;
  _gdk_symbian_drag_begin (gdk_window_get_toplevel (window), (gint) edge, root_x, root_y);
}

void
gdk_window_begin_move_drag (GdkWindow *window, gint button, gint root_x, gint root_y, guint32 timestamp)
{
  g_return_if_fail (GDK_IS_WINDOW (window)); (void) button; (void) timestamp;
  if (GDK_WINDOW_DESTROYED (window)) return;
  _gdk_symbian_drag_begin (gdk_window_get_toplevel (window), -1, root_x, root_y);
}

/* ---- screen: stacking order and active window ---- */
GList *
gdk_screen_get_window_stack (GdkScreen *screen)
{
  GList *l, *r = NULL;
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  for (l = _gdk_symbian_toplevel_stack (); l; l = l->next) r = g_list_append (r, g_object_ref (l->data));         /* bottom to top, each referenced */
  return r;
}

GdkWindow *
gdk_screen_get_active_window (GdkScreen *screen)
{
  GdkWindow *w;
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  w = _gdk_symbian_focus_window ();
  return w ? g_object_ref (w) : NULL;
}

/* ---- keyboard grab: the window server grabs nothing, so a grab is GDK's bookkeeping, honoured by the key translation ---- */
GdkGrabStatus
gdk_keyboard_grab (GdkWindow *window, gboolean owner_events, guint32 time)
{
  GdkDisplay *display; GdkDisplaySymbian *d;
  g_return_val_if_fail (GDK_IS_WINDOW (window), GDK_GRAB_NOT_VIEWABLE);
  if (!gdk_window_is_viewable (window)) return GDK_GRAB_NOT_VIEWABLE;
  display = gdk_window_get_display (window); d = GDK_DISPLAY_SYMBIAN (display);
  _gdk_display_set_has_keyboard_grab (display, window, gdk_window_get_toplevel (window), owner_events, ++d->serial, time);
  return GDK_GRAB_SUCCESS;
}

/* ---- devices: the core pointer is the only one ---- */
static GList *device_list = NULL;
GList *
gdk_display_list_devices (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  if (device_list == NULL) device_list = g_list_append (NULL, display->core_pointer);
  return device_list;
}
GList *gdk_devices_list (void) { return gdk_display_list_devices (gdk_display_get_default ()); }

const gchar *gdk_device_get_name (GdkDevice *device) { g_return_val_if_fail (GDK_IS_DEVICE (device), NULL); return device->name; }
GdkInputSource gdk_device_get_source (GdkDevice *device) { g_return_val_if_fail (GDK_IS_DEVICE (device), GDK_SOURCE_MOUSE); return device->source; }
GdkInputMode gdk_device_get_mode (GdkDevice *device) { g_return_val_if_fail (GDK_IS_DEVICE (device), GDK_MODE_DISABLED); return device->mode; }
gboolean gdk_device_get_has_cursor (GdkDevice *device) { g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE); return device->has_cursor; }
gint gdk_device_get_n_axes (GdkDevice *device) { g_return_val_if_fail (GDK_IS_DEVICE (device), 0); return device->num_axes; }
gint gdk_device_get_n_keys (GdkDevice *device) { g_return_val_if_fail (GDK_IS_DEVICE (device), 0); return device->num_keys; }
void gdk_device_set_source (GdkDevice *device, GdkInputSource source) { g_return_if_fail (GDK_IS_DEVICE (device)); device->source = source; }

/* a device that moves the pointer cannot be put in window mode or disabled: only the extension devices of a window system that has them can */
gboolean
gdk_device_set_mode (GdkDevice *device, GdkInputMode mode)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);
  return device->mode == mode;
}

GdkAxisUse
gdk_device_get_axis_use (GdkDevice *device, guint index_)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), GDK_AXIS_IGNORE); g_return_val_if_fail (index_ < (guint) device->num_axes, GDK_AXIS_IGNORE);
  return device->axes[index_].use;
}

void
gdk_device_set_axis_use (GdkDevice *device, guint index_, GdkAxisUse use)
{
  g_return_if_fail (GDK_IS_DEVICE (device)); g_return_if_fail (index_ < (guint) device->num_axes);
  device->axes[index_].use = use;
}

void
gdk_device_get_key (GdkDevice *device, guint index_, guint *keyval, GdkModifierType *modifiers)
{
  g_return_if_fail (GDK_IS_DEVICE (device)); g_return_if_fail (index_ < (guint) device->num_keys);
  if (keyval) *keyval = device->keys[index_].keyval;
  if (modifiers) *modifiers = device->keys[index_].modifiers;
}

void
gdk_device_set_key (GdkDevice *device, guint index_, guint keyval, GdkModifierType modifiers)
{
  g_return_if_fail (GDK_IS_DEVICE (device)); g_return_if_fail (index_ < (guint) device->num_keys);
  device->keys[index_].keyval = keyval; device->keys[index_].modifiers = modifiers;
}

/* the core pointer keeps no motion history (the window server reports every move as an event) */
gboolean
gdk_device_get_history (GdkDevice *device, GdkWindow *window, guint32 start, guint32 stop, GdkTimeCoord ***events, gint *n_events)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE); g_return_val_if_fail (events != NULL, FALSE); g_return_val_if_fail (n_events != NULL, FALSE);
  (void) device; (void) start; (void) stop;
  *events = NULL; *n_events = 0;
  return FALSE;
}
void gdk_device_free_history (GdkTimeCoord **events, gint n_events) { gint i; for (i = 0; i < n_events; i++) g_free (events[i]); g_free (events); }

/* ---- the display ---- */
gboolean gdk_display_supports_selection_notification (GdkDisplay *display) { g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE); return FALSE; }
gboolean gdk_display_request_selection_notification (GdkDisplay *display, GdkAtom selection) { g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE); (void) selection; return FALSE; }
gboolean gdk_display_supports_clipboard_persistence (GdkDisplay *display) { g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE); return FALSE; }
void gdk_display_store_clipboard (GdkDisplay *display, GdkWindow *clipboard_window, guint32 time_, const GdkAtom *targets, gint n_targets)
{ g_return_if_fail (GDK_IS_DISPLAY (display)); (void) clipboard_window; (void) time_; (void) targets; (void) n_targets; }       /* there is no clipboard manager to hand the contents to */

void
gdk_display_warp_pointer (GdkDisplay *display, GdkScreen *screen, gint x, gint y)
{
  g_return_if_fail (GDK_IS_DISPLAY (display)); (void) screen;
  _gdk_symbian_pointer_warp (display, x, y);
}

/* ---- error traps: the window server reports no asynchronous errors, so there is nothing to catch; the nesting is still checked ---- */
static gint trap_depth = 0;
void gdk_error_trap_push (void) { trap_depth++; }
gint gdk_error_trap_pop (void) { g_return_val_if_fail (trap_depth > 0, 0); trap_depth--; return 0; }

/* ---- visuals, colormaps ---- */
gint gdk_colormap_get_system_size (void) { return gdk_screen_get_system_visual (gdk_screen_get_default ())->colormap_size; }

/* ---- wide characters in the locale encoding ---- */
gchar *
gdk_wcstombs (const GdkWChar *src)
{
  gint n = 0; gchar *utf8, *r; gunichar *ucs;
  while (src[n]) n++;
  ucs = g_new (gunichar, n + 1); { gint i; for (i = 0; i < n; i++) ucs[i] = (gunichar) src[i]; ucs[n] = 0; }
  utf8 = g_ucs4_to_utf8 (ucs, n, NULL, NULL, NULL); g_free (ucs);
  if (!utf8) return NULL;
  r = g_locale_from_utf8 (utf8, -1, NULL, NULL, NULL); g_free (utf8);
  return r;
}

gint
gdk_mbstowcs (GdkWChar *dest, const gchar *src, gint dest_max)
{
  gchar *utf8 = g_locale_to_utf8 (src, -1, NULL, NULL, NULL); glong n = 0, i; gunichar *ucs;
  if (!utf8) return -1;
  ucs = g_utf8_to_ucs4 (utf8, -1, NULL, &n, NULL); g_free (utf8);
  if (!ucs) return -1;
  for (i = 0; i < n && i < dest_max; i++) dest[i] = (GdkWChar) ucs[i];
  g_free (ucs);
  return (gint) i;
}

/* ---- spawning: processes inherit the environment unchanged (there is no DISPLAY to name the screen) ---- */
gboolean
gdk_spawn_on_screen_with_pipes (GdkScreen *screen, const gchar *working_directory, gchar **argv, gchar **envp, GSpawnFlags flags, GSpawnChildSetupFunc child_setup,
                                gpointer user_data, GPid *child_pid, gint *standard_input, gint *standard_output, gint *standard_error, GError **error)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);
  return g_spawn_async_with_pipes (working_directory, argv, envp, flags, child_setup, user_data, child_pid, standard_input, standard_output, standard_error, error);
}

gboolean
gdk_spawn_on_screen (GdkScreen *screen, const gchar *working_directory, gchar **argv, gchar **envp, GSpawnFlags flags, GSpawnChildSetupFunc child_setup,
                     gpointer user_data, gint *child_pid, GError **error)
{
  GPid pid; gboolean ok;
  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);
  ok = g_spawn_async (working_directory, argv, envp, flags, child_setup, user_data, &pid, error);
  if (child_pid) *child_pid = (gint) pid;
  return ok;
}

gboolean
gdk_spawn_command_line_on_screen (GdkScreen *screen, const gchar *command_line, GError **error)
{
  gchar **argv = NULL; gboolean ok;
  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);
  if (!g_shell_parse_argv (command_line, NULL, &argv, error)) return FALSE;
  ok = g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, error);
  g_strfreev (argv);
  return ok;
}

/* ---- fonts: one display ---- */
GdkDisplay *gdk_font_get_display (GdkFont *font) { g_return_val_if_fail (font != NULL, NULL); return gdk_display_get_default (); }
GdkFont *gdk_fontset_load (const gchar *fontset_name) { return gdk_fontset_load_for_display (gdk_display_get_default (), fontset_name); }

/* graphics-expose events belong to copy operations from windows that are partly covered; every GDK window of this backend is drawn client side, so none is ever generated */
GdkEvent *gdk_event_get_graphics_expose (GdkWindow *window) { g_return_val_if_fail (GDK_IS_WINDOW (window), NULL); return NULL; }

/* sets the locale from the environment; without the Xlib locale support of the X11 backend the C library's answer is final */
gchar *
gdk_set_locale (void)
{
  if (!setlocale (LC_ALL, "")) g_warning ("locale not supported by C library");
  return g_strdup (setlocale (LC_ALL, NULL));
}

/* ---- pending events, the display name, shared memory images, colour cells of a visual that has none ---- */
gboolean
gdk_events_pending (void)
{
  return _gdk_event_queue_find_first (gdk_display_get_default ()) != NULL || phe_ws_pending () != 0;
}

gchar *gdk_get_display (void) { return g_strdup (gdk_display_get_name (gdk_display_get_default ())); }
GdkScreen *gdk_gc_get_screen (GdkGC *gc) { g_return_val_if_fail (GDK_IS_GC (gc), NULL); return gdk_screen_get_default (); }
/* shared memory images are an X protocol extension; every image here is plain client memory */
static gboolean use_xshm = FALSE;
gboolean gdk_get_use_xshm (void) { return use_xshm; }
void gdk_set_use_xshm (gboolean value) { use_xshm = value; }

/* the system visual is TrueColor: its colormap has no writable cells, so an allocation of private cells cannot succeed and a store into a cell cannot change anything */
gint gdk_colors_alloc (GdkColormap *colormap, gboolean contiguous, gulong *planes, gint nplanes, gulong *pixels, gint npixels) { g_return_val_if_fail (GDK_IS_COLORMAP (colormap), 0); return 0; }
void gdk_colors_free (GdkColormap *colormap, gulong *pixels, gint npixels, gulong planes) { g_return_if_fail (GDK_IS_COLORMAP (colormap)); }
gint gdk_color_change (GdkColormap *colormap, GdkColor *color) { g_return_val_if_fail (GDK_IS_COLORMAP (colormap), 0); g_return_val_if_fail (color != NULL, 0); return 0; }

/* ---- the window manager's own client messages (messages sent to the root window) ----
 * _HILDON_LOADING_SCREENSHOT (what hildon-desktop answers): data.l[0] 0 = take a screenshot of the window data.l[1] and keep it as the picture shown while the application starts again, 1 = remove
 * the kept one; the manager answers with a client message of the same type to that window when it is done. The picture is the window's pixels as PNG under $PHE_LAUNCH_CACHE
 * (default c:/data/launch), named after the window's title. Returns TRUE when the manager took the message. */
static gchar *
launch_picture_path (GdkWindow *win)
{
  gchar *name, *dir, *path, *p;
  name = g_strdup (WI (win)->title && *WI (win)->title ? WI (win)->title : "application");               /* the title gdk_window_set_title () kept */
  for (p = name; *p; p++) if (!g_ascii_isalnum (*p) && *p != '-' && *p != '_') *p = '_';
  dir = g_strdup (g_getenv ("PHE_LAUNCH_CACHE") ? g_getenv ("PHE_LAUNCH_CACHE") : "c:/data/launch");
  g_mkdir_with_parents (dir, 0755);
  path = g_strdup_printf ("%s/%s.png", dir, name); g_free (dir); g_free (name);
  return path;
}

gboolean
gdk_symbian_wm_client_message (GdkWindow *target, GdkAtom message_type, const glong *data, gint format)
{
  GdkWindow *win; gchar *path; gboolean ok = TRUE;
  if (target != gdk_screen_get_root_window (gdk_screen_get_default ()) || format != 32) return FALSE;
  if (message_type != gdk_atom_intern_static_string ("_HILDON_LOADING_SCREENSHOT")) return FALSE;
  win = gdk_window_lookup ((GdkNativeWindow) data[1]);
  if (win && !GDK_WINDOW_DESTROYED (win)) {
    path = launch_picture_path (win);
    if (data[0] == 0) {
      gint w, h; GdkPixbuf *pb;
      gdk_drawable_get_size (GDK_DRAWABLE (win), &w, &h);
      gdk_window_process_all_updates (); gdk_display_sync (gdk_window_get_display (win));            /* what is on the screen, not what is still queued */
      pb = gdk_pixbuf_get_from_drawable (NULL, GDK_DRAWABLE (win), NULL, 0, 0, 0, 0, w, h);
      if (pb) { GError *err = NULL; ok = gdk_pixbuf_save (pb, path, "png", &err, NULL); if (!ok) { gchar *m = g_strdup_printf ("[phegdk] launch picture %s: %s", path, err ? err->message : "?"); phe_log (m); g_free (m); } g_clear_error (&err); g_object_unref (pb); }
      else { gchar *m = g_strdup_printf ("[phegdk] launch picture: no pixbuf for window %dx%d", w, h); phe_log (m); g_free (m); ok = FALSE; }
    } else g_unlink (path);
    { gchar *m = g_strdup_printf ("[phegdk] launch picture %s %s: %s, file there: %d", data[0] == 0 ? "take" : "remove", path, ok ? "ok" : "failed", g_file_test (path, G_FILE_TEST_EXISTS)); phe_log (m); g_free (m); }
    g_free (path);
    { GdkEvent *ev = gdk_event_new (GDK_CLIENT_EVENT);                                             /* the reply: same type, to the window, l[0] = 0 on success */
      ev->client.window = g_object_ref (win); ev->client.send_event = TRUE; ev->client.message_type = message_type; ev->client.data_format = 32;
      ev->client.data.l[0] = ok ? 0 : 1; ev->client.data.l[1] = data[1];
      gdk_event_put (ev); gdk_event_free (ev); }
  }
  return TRUE;
}
