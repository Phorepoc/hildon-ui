/* gdkdisplay-symbian.c -- GdkDisplay / GdkScreen of the Symbian backend: one display, one screen of the WSERV screen size, one 24 bit TrueColor visual (the pixel layout of
 * the CFbsBitmap EColor16MU that every window draws into), the system colormap, the core pointer device. BSD-2-Clause (Phorepoc). */
#include "config.h"
#include <string.h>
#include "gdksymbian.h"
#include "gdkdisplay.h"
#include "gdkscreen.h"
#include "gdkprivate.h"
#include "gdkinternals.h"

GdkDisplay *_gdk_display = NULL;
GdkScreen  *_gdk_screen = NULL;

G_DEFINE_TYPE (GdkDisplaySymbian, gdk_display_symbian, GDK_TYPE_DISPLAY)
G_DEFINE_TYPE (GdkScreenSymbian, gdk_screen_symbian, GDK_TYPE_SCREEN)

static void gdk_display_symbian_init (GdkDisplaySymbian *d) { (void) d; }
static void gdk_display_symbian_class_init (GdkDisplaySymbianClass *c) { (void) c; }
static void gdk_screen_symbian_init (GdkScreenSymbian *s) { (void) s; }

static void gdk_screen_symbian_dispose (GObject *object)
{
  GdkScreenSymbian *s = GDK_SCREEN_SYMBIAN (object);
  if (s->root) { _gdk_window_destroy (s->root, TRUE); s->root = NULL; }
  G_OBJECT_CLASS (gdk_screen_symbian_parent_class)->dispose (object);
}
static void gdk_screen_symbian_class_init (GdkScreenSymbianClass *c) { G_OBJECT_CLASS (c)->dispose = gdk_screen_symbian_dispose; }

/* ---- the input device: the WSERV pointer (touch or mouse), no extension devices ---- */
static GdkDeviceAxis core_axes[] = {
  { GDK_AXIS_X, 0, 0 },
  { GDK_AXIS_Y, 0, 0 }
};

void
_gdk_input_init (void)
{
  GdkDisplay *display = _gdk_display;
  display->core_pointer = g_object_new (GDK_TYPE_DEVICE, NULL);
  display->core_pointer->name = "Core Pointer";
  display->core_pointer->source = GDK_SOURCE_MOUSE;
  display->core_pointer->mode = GDK_MODE_SCREEN;
  display->core_pointer->has_cursor = TRUE;
  display->core_pointer->num_axes = 2;
  display->core_pointer->axes = core_axes;
  display->core_pointer->num_keys = 0;
  display->core_pointer->keys = NULL;
}

/* ---- opening the display ---- */
GdkDisplay *
gdk_display_open (const gchar *display_name)
{
  GdkDisplaySymbian *d; GdkScreenSymbian *s; unsigned char *px = NULL; int w = 0, h = 0, st = 0, err;
  (void) display_name;
  if (_gdk_display) return _gdk_display;                    /* a single display only */

  err = phe_ws_init (&px, &w, &h, &st);
  if (err != 0) { g_warning ("gdk_display_open: the window server could not be opened (%d)", err); return NULL; }

  d = g_object_new (GDK_TYPE_DISPLAY_SYMBIAN, NULL);
  _gdk_display = GDK_DISPLAY_OBJECT (d);
  d->pixels = px; d->stride = st; d->serial = 1;
  s = g_object_new (GDK_TYPE_SCREEN_SYMBIAN, NULL);
  d->screen = GDK_SCREEN (s); _gdk_screen = d->screen;
  s->display = _gdk_display; s->width = w; s->height = h;
  s->width_mm = (w * 254 + 5 * 96) / (10 * 96);             /* 96 dpi */
  s->height_mm = (h * 254 + 5 * 96) / (10 * 96);

  _gdk_visual_init (d->screen);
  _gdk_windowing_window_init (d->screen);
  _gdk_symbian_window_hooks ();
  gdk_screen_set_default_colormap (d->screen, gdk_screen_get_system_colormap (d->screen));
  _gdk_events_init ();
  _gdk_input_init ();

  g_signal_emit_by_name (gdk_display_manager_get (), "display_opened", _gdk_display);
  return _gdk_display;
}

const gchar *
gdk_display_get_name (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  return "symbian:0";
}
gint gdk_display_get_n_screens (GdkDisplay *display) { g_return_val_if_fail (GDK_IS_DISPLAY (display), 0); return 1; }
GdkScreen *gdk_display_get_screen (GdkDisplay *display, gint screen_num)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL); g_return_val_if_fail (screen_num == 0, NULL);
  return GDK_DISPLAY_SYMBIAN (display)->screen;
}
GdkScreen *gdk_display_get_default_screen (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  return GDK_DISPLAY_SYMBIAN (display)->screen;
}

/* the pointer is never grabbed by the window server: a grab is bookkeeping of GDK (the display's grab list), so ungrabbing ends it there */
void
gdk_display_pointer_ungrab (GdkDisplay *display, guint32 time_)
{
  GdkPointerGrabInfo *grab;
  (void) time_;
  g_return_if_fail (GDK_IS_DISPLAY (display));
  grab = _gdk_display_get_last_pointer_grab (display);
  if (grab) grab->serial_end = 0;
  _gdk_display_pointer_grab_update (display, 0);
}

void
gdk_display_keyboard_ungrab (GdkDisplay *display, guint32 time_)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  if (display->keyboard_grab.window != NULL) _gdk_display_unset_has_keyboard_grab (display, FALSE);
  (void) time_;
}

void gdk_display_beep (GdkDisplay *display) { g_return_if_fail (GDK_IS_DISPLAY (display)); phe_ws_beep (); }
void gdk_display_sync (GdkDisplay *display) { g_return_if_fail (GDK_IS_DISPLAY (display)); phe_ws_sync (); }
/* nothing is buffered on the client side except the BitBlt queue, which wakes the WSERV thread the moment a command is queued */
void gdk_display_flush (GdkDisplay *display) { g_return_if_fail (GDK_IS_DISPLAY (display)); }
gboolean gdk_display_supports_composite (GdkDisplay *display) { g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE); return FALSE; }
gboolean gdk_display_supports_cursor_alpha (GdkDisplay *display) { return FALSE; }
gboolean gdk_display_supports_cursor_color (GdkDisplay *display) { return FALSE; }
gboolean gdk_display_supports_input_shapes (GdkDisplay *display) { return FALSE; }
gboolean gdk_display_supports_shapes (GdkDisplay *display) { return FALSE; }
guint gdk_display_get_default_cursor_size (GdkDisplay *display) { return 16; }
void gdk_display_get_maximal_cursor_size (GdkDisplay *display, guint *width, guint *height) { if (width) *width = 16; if (height) *height = 16; }
GdkWindow *gdk_display_get_default_group (GdkDisplay *display) { g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL); return gdk_screen_get_root_window (GDK_DISPLAY_SYMBIAN (display)->screen); }

/* ---- GdkScreen ---- */
GdkDisplay *gdk_screen_get_display (GdkScreen *screen) { g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL); return GDK_SCREEN_SYMBIAN (screen)->display; }
gint gdk_screen_get_number (GdkScreen *screen) { g_return_val_if_fail (GDK_IS_SCREEN (screen), 0); return 0; }
gint gdk_screen_get_width (GdkScreen *screen) { g_return_val_if_fail (GDK_IS_SCREEN (screen), 0); return GDK_SCREEN_SYMBIAN (screen)->width; }
gint gdk_screen_get_height (GdkScreen *screen) { g_return_val_if_fail (GDK_IS_SCREEN (screen), 0); return GDK_SCREEN_SYMBIAN (screen)->height; }
gint gdk_screen_get_width_mm (GdkScreen *screen) { g_return_val_if_fail (GDK_IS_SCREEN (screen), 0); return GDK_SCREEN_SYMBIAN (screen)->width_mm; }
gint gdk_screen_get_height_mm (GdkScreen *screen) { g_return_val_if_fail (GDK_IS_SCREEN (screen), 0); return GDK_SCREEN_SYMBIAN (screen)->height_mm; }
GdkWindow *gdk_screen_get_root_window (GdkScreen *screen) { g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL); return GDK_SCREEN_SYMBIAN (screen)->root; }
GdkColormap *gdk_screen_get_system_colormap (GdkScreen *screen) { g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL); return GDK_SCREEN_SYMBIAN (screen)->colormap; }
GdkVisual *gdk_screen_get_system_visual (GdkScreen *screen) { g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL); return GDK_SCREEN_SYMBIAN (screen)->visual; }
GdkColormap *gdk_screen_get_rgba_colormap (GdkScreen *screen) { return NULL; }      /* no ARGB visual: the bitmap is EColor16MU */
GdkVisual *gdk_screen_get_rgba_visual (GdkScreen *screen) { return NULL; }
gboolean gdk_screen_is_composited (GdkScreen *screen) { return FALSE; }
GList *gdk_screen_list_visuals (GdkScreen *screen) { g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL); return g_list_append (NULL, GDK_SCREEN_SYMBIAN (screen)->visual); }
gchar *gdk_screen_make_display_name (GdkScreen *screen) { g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL); return g_strdup ("symbian:0"); }

/* one monitor: the whole screen (a resolution change is `_gdk_symbian_screen_resized`) */
gint gdk_screen_get_n_monitors (GdkScreen *screen) { g_return_val_if_fail (GDK_IS_SCREEN (screen), 0); return 1; }
gint gdk_screen_get_primary_monitor (GdkScreen *screen) { return 0; }
void gdk_screen_get_monitor_geometry (GdkScreen *screen, gint monitor_num, GdkRectangle *dest)
{
  g_return_if_fail (GDK_IS_SCREEN (screen)); g_return_if_fail (monitor_num == 0);
  dest->x = 0; dest->y = 0; dest->width = GDK_SCREEN_SYMBIAN (screen)->width; dest->height = GDK_SCREEN_SYMBIAN (screen)->height;
}
gint gdk_screen_get_monitor_width_mm (GdkScreen *screen, gint monitor_num) { return gdk_screen_get_width_mm (screen); }
gint gdk_screen_get_monitor_height_mm (GdkScreen *screen, gint monitor_num) { return gdk_screen_get_height_mm (screen); }
gchar *gdk_screen_get_monitor_plug_name (GdkScreen *screen, gint monitor_num) { return g_strdup ("WSERV"); }

void
_gdk_symbian_screen_resized (GdkScreen *screen, int width, int height)
{
  GdkScreenSymbian *s = GDK_SCREEN_SYMBIAN (screen);
  s->width = width; s->height = height;
  s->width_mm = (width * 254 + 5 * 96) / (10 * 96); s->height_mm = (height * 254 + 5 * 96) / (10 * 96);
  g_signal_emit_by_name (screen, "size-changed");
  g_signal_emit_by_name (screen, "monitors-changed");
}

/* there are no other clients to send a client message to: toplevels of this process are reached through the event queue */
void
gdk_screen_broadcast_client_message (GdkScreen *screen, GdkEvent *sev)
{
  GList *l, *tops;
  g_return_if_fail (GDK_IS_SCREEN (screen)); g_return_if_fail (sev != NULL);
  tops = gdk_screen_get_toplevel_windows (screen);
  for (l = tops; l; l = l->next) {
    GdkEvent *ev = gdk_event_copy (sev);
    if (ev->any.window) g_object_unref (ev->any.window);
    ev->any.window = g_object_ref (l->data); ev->any.send_event = TRUE;
    gdk_event_put (ev); gdk_event_free (ev);
  }
  g_list_free (tops);
}

/* GTK settings: no settings daemon on the device; the values of a touch phone, as the DirectFB and Win32 backends fall back to fixed values too */
gboolean
gdk_screen_get_setting (GdkScreen *screen, const gchar *name, GValue *value)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE); g_return_val_if_fail (name != NULL, FALSE);
  if (strcmp (name, "gtk-double-click-time") == 0) { if (!G_VALUE_HOLDS_INT (value)) return FALSE; g_value_set_int (value, 400); return TRUE; }
  if (strcmp (name, "gtk-double-click-distance") == 0) { if (!G_VALUE_HOLDS_INT (value)) return FALSE; g_value_set_int (value, 12); return TRUE; }
  if (strcmp (name, "gtk-dnd-drag-threshold") == 0) { if (!G_VALUE_HOLDS_INT (value)) return FALSE; g_value_set_int (value, 16); return TRUE; }
  if (strcmp (name, "gtk-cursor-blink") == 0) { if (!G_VALUE_HOLDS_BOOLEAN (value)) return FALSE; g_value_set_boolean (value, TRUE); return TRUE; }
  if (strcmp (name, "gtk-cursor-blink-time") == 0) { if (!G_VALUE_HOLDS_INT (value)) return FALSE; g_value_set_int (value, 1200); return TRUE; }
  if (strcmp (name, "gtk-xft-dpi") == 0) { if (!G_VALUE_HOLDS_INT (value)) return FALSE; g_value_set_int (value, 96 * 1024); return TRUE; }
  if (strcmp (name, "gtk-xft-antialias") == 0) { if (!G_VALUE_HOLDS_INT (value)) return FALSE; g_value_set_int (value, 1); return TRUE; }
  if (strcmp (name, "gtk-xft-hinting") == 0) { if (!G_VALUE_HOLDS_INT (value)) return FALSE; g_value_set_int (value, 0); return TRUE; }
  if (strcmp (name, "gtk-xft-rgba") == 0) { if (!G_VALUE_HOLDS_STRING (value)) return FALSE; g_value_set_static_string (value, "none"); return TRUE; }
  if (strcmp (name, "gtk-enable-animations") == 0) { if (!G_VALUE_HOLDS_BOOLEAN (value)) return FALSE; g_value_set_boolean (value, TRUE); return TRUE; }
  return FALSE;
}

/* ---- windowing glue of gdkinternals.h that belongs to the display ---- */
const GOptionEntry _gdk_windowing_args[] = { { NULL } };
void _gdk_windowing_init (void) { }                                   /* nothing to do before the arguments are parsed (as in the DirectFB backend) */
void _gdk_windowing_set_default_display (GdkDisplay *display) { (void) display; }   /* single display: it is the default by construction */
gchar *_gdk_windowing_substitute_screen_number (const gchar *display_name, gint screen_number) { (void) screen_number; return g_strdup (display_name); }
gulong _gdk_windowing_window_get_next_serial (GdkDisplay *display) { return ++GDK_DISPLAY_SYMBIAN (display)->serial; }
void _gdk_windowing_display_set_sm_client_id (GdkDisplay *display, const gchar *sm_client_id) { (void) display; (void) sm_client_id; }   /* no session manager */
gint _gdk_windowing_get_bits_for_depth (GdkDisplay *display, gint depth)
{
  (void) display;
  switch (depth) { case 1: return 1; case 8: return 8; case 15: case 16: return 16; case 24: case 32: return 32; }
  return 0;
}
void _gdk_windowing_window_set_composited (GdkWindow *window, gboolean composited) { (void) window; (void) composited; }
void gdk_flush (void) { if (_gdk_display) gdk_display_flush (_gdk_display); }
void gdk_notify_startup_complete (void) { }
void gdk_notify_startup_complete_with_id (const gchar *startup_id) { (void) startup_id; }
char *_gdk_windowing_get_startup_notify_id (GAppLaunchContext *context, GAppInfo *info, GList *files) { (void) context; (void) info; (void) files; return NULL; }
void _gdk_windowing_launch_failed (GAppLaunchContext *context, const char *startup_notify_id) { (void) context; (void) startup_notify_id; }
void _gdk_windowing_exit (void) { }

