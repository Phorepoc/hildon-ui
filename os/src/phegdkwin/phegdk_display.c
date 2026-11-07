/* phegdk_display.c -- gdk-symbian skeleton: PheGdkDisplay / PheGdkScreen / PheGdkWindow, the GLib main-loop source for WSERV, process_updates as a
 * dirty-rectangle BitBlt. BSD-2-Clause (Phorepoc).
 *
 * The main-loop source: WSERV completes TRequestStatus objects (RWsSession::EventReady / RedrawReady); the C++ half keeps them armed and phe_ws_poll()
 * fetches whatever completed WITHOUT blocking. The source asks the loop to wake every 10 ms (prepare timeout) and drains the queue in dispatch: a polling
 * source. The proper wake-up (a helper thread that waits on the statuses and writes a byte into a pipe the loop select()s on, as section 3 describes) replaces
 * only prepare/check. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "phegdk.h"

G_DEFINE_TYPE (PheGdkScreen, phe_gdk_screen, G_TYPE_OBJECT)
static void phe_gdk_screen_init (PheGdkScreen *s) { (void) s; }
static void phe_gdk_screen_class_init (PheGdkScreenClass *c) { (void) c; }
G_DEFINE_TYPE (PheGdkDisplay, phe_gdk_display, G_TYPE_OBJECT)
static void phe_gdk_display_init (PheGdkDisplay *d) { (void) d; }
static void phe_gdk_display_class_init (PheGdkDisplayClass *c) { (void) c; }
G_DEFINE_TYPE (PheGdkWindow, phe_gdk_window, G_TYPE_OBJECT)
static void phe_gdk_window_init (PheGdkWindow *w) { (void) w; }
static void phe_gdk_window_finalize (GObject *o)
{
  PheGdkWindow *w = (PheGdkWindow *) o;
  if (w->cr) cairo_destroy (w->cr);
  if (w->surface) cairo_surface_destroy (w->surface);
  G_OBJECT_CLASS (phe_gdk_window_parent_class)->finalize (o);
}
static void phe_gdk_window_class_init (PheGdkWindowClass *c) { G_OBJECT_CLASS (c)->finalize = phe_gdk_window_finalize; }

/* ---- the main-loop source: sleeps until WSERV (or any request) completes ---- */
typedef struct { GSource base; PheGdkDisplay *display; } PheWsSource;

/* WSERV key codes (e32keys.h) -> X keysyms (what GDK's keyval is); anything printable is the same in both */
static unsigned keysym_of (int code, int scancode)
{
  switch (code) {
    case 0x0e: return 0xff0d;   /* EKeyEnter -> GDK_KEY_Return */   case 0x01: return 0xff08;  /* EKeyBackspace */
    case 0x0b: return 0xff1b;   /* EKeyEscape */                     case 0x09: return 0xff09;  /* EKeyTab */
    case 0xf807: return 0xff51; case 0xf808: return 0xff53; case 0xf809: return 0xff52; case 0xf80a: return 0xff54;   /* arrows: Left Right Up Down */
    case 0xf845: return 0xffc2; /* EKeyDevice0 (left softkey)  -> GDK_KEY_F5 */  case 0xf846: return 0xffc3;  /* EKeyDevice1 -> F6 */  case 0xf847: return 0xffc4;  /* EKeyDevice2 -> F7 */
  }
  if (code >= 0x20 && code < 0x7f) return (unsigned) code;
  return 0x1008ff00u + (unsigned) (scancode & 0xff);   /* vendor key space: unknown keys stay distinguishable */
}

/* WSERV delivers EEventKeyDown(scancode), EEventKey(code), EEventKeyUp(scancode) for one key press. A GdkEventKey needs the keyval from the middle one, so the
 * key-down is held back until the typed event (or the key-up, for keys that never type: modifiers) arrives. */
static struct { gboolean down_pending; int scancode; unsigned last_keyval; } kstate;

/* returns how many GdkEvents were produced into out[0..1] */
static int translate (const PheWsEvent *e, PheGdkEvent out[2])
{
  PheGdkEvent *g = &out[0]; int n = 0;
  memset (out, 0, 2 * sizeof *out); out[0].time = out[1].time = (unsigned) (g_get_monotonic_time () / 1000);
  if (e->type == PHE_EV_KEY) {
    if (e->ptype == 1) { kstate.down_pending = TRUE; kstate.scancode = e->scancode; return 0; }
    if (e->ptype == 0) { if (!kstate.down_pending) return 0; kstate.down_pending = FALSE;
      g->type = PHE_KEY_PRESS; g->hardware_keycode = (unsigned) kstate.scancode; g->keyval = keysym_of (e->code, kstate.scancode); kstate.last_keyval = g->keyval; return 1; }
    /* key up */
    if (kstate.down_pending) { kstate.down_pending = FALSE; g->type = PHE_KEY_PRESS; g->hardware_keycode = (unsigned) kstate.scancode; g->keyval = keysym_of (0, kstate.scancode); kstate.last_keyval = g->keyval; n = 1; }
    out[n].type = PHE_KEY_RELEASE; out[n].hardware_keycode = (unsigned) e->scancode; out[n].keyval = kstate.last_keyval ? kstate.last_keyval : keysym_of (0, e->scancode); kstate.last_keyval = 0; return n + 1;
  }
  if (e->type == PHE_EV_POINTER) {
    g->x = e->x; g->y = e->y;
    switch (e->ptype) {
      case 0: g->type = PHE_BUTTON_PRESS; g->button = 1; g->state = 0; return 1;
      case 1: g->type = PHE_BUTTON_RELEASE; g->button = 1; g->state = 0x100; return 1;
      case 6: case 7: g->type = PHE_MOTION_NOTIFY; g->state = e->ptype == 6 ? 0x100 : 0; return 1;
      default: return 0;
    }
  }
  return 0;
}

static gboolean ws_prepare (GSource *s, gint *timeout) { ((PheWsSource *) s)->display->iterations++; *timeout = -1; return phe_ws_pending () != 0; }
static gboolean ws_check (GSource *s) { PheGdkDisplay *d = ((PheWsSource *) s)->display; return (d->wake.revents & G_IO_IN) != 0 || phe_ws_pending () != 0; }
static gboolean ws_dispatch (GSource *s, GSourceFunc cb, gpointer data)
{
  PheGdkDisplay *d = ((PheWsSource *) s)->display; PheWsEvent ev; PheGdkEvent ge[2]; GList *l; int n = 0, k, nev; char sink[64]; (void) cb; (void) data;
  while (read (d->wake.fd, sink, sizeof sink) > 0) ;                          /* empty the wake-up pipe (non-blocking), then the queue */
  while (n < 64 && phe_ws_poll (&ev)) {
    n++;
    if (ev.type == PHE_EV_REDRAW) {            /* the window server lost part of the screen: re-present that rectangle from the backing bitmap */
      if (ev.w > 0 && ev.h > 0) { char m[120]; snprintf (m, sizeof m, "[phegdkwin] redraw event %d,%d %dx%d", ev.x, ev.y, ev.w, ev.h); phe_log (m); phe_ws_present (ev.x, ev.y, ev.w, ev.h); }
    } else if ((nev = translate (&ev, ge)) > 0 && d->event_func) for (k = 0; k < nev; k++) d->event_func (&ge[k], d->event_data);
  }
  for (l = d->windows; l; l = l->next) phe_gdk_window_process_updates (l->data);   /* anything the handlers invalidated is exposed in the same iteration */
  return G_SOURCE_CONTINUE;
}
static GSourceFuncs ws_funcs = { ws_prepare, ws_check, ws_dispatch, NULL, NULL, NULL };

PheGdkDisplay *phe_gdk_display_open (void)
{
  PheGdkDisplay *d = g_object_new (PHE_TYPE_GDK_DISPLAY, NULL); PheGdkScreen *s = g_object_new (PHE_TYPE_GDK_SCREEN, NULL);
  int w = 0, h = 0, st = 0; unsigned char *px = NULL; char m[120];
  if (phe_ws_init (&px, &w, &h, &st) != 0) { phe_log ("[phegdkwin] phe_ws_init failed"); return NULL; }
  d->screen = s; s->display = d; s->width = w; s->height = h; d->pixels = px; d->stride = st;
  d->source = g_source_new (&ws_funcs, sizeof (PheWsSource)); ((PheWsSource *) d->source)->display = d;
  d->wake.fd = phe_ws_wake_fd (); d->wake.events = G_IO_IN; d->wake.revents = 0; g_source_add_poll (d->source, &d->wake);
  g_source_attach (d->source, NULL);
  snprintf (m, sizeof m, "[phegdkwin] display open: screen %dx%d, WSERV source attached (helper thread + wake-up pipe fd %d)", w, h, d->wake.fd); phe_log (m);
  return d;
}
PheGdkScreen *phe_gdk_display_get_screen (PheGdkDisplay *d) { return d->screen; }
void phe_gdk_display_set_event_func (PheGdkDisplay *d, PheGdkEventFunc f, gpointer data) { d->event_func = f; d->event_data = data; }

/* ---- windows ---- */
PheGdkWindow *phe_gdk_window_new_toplevel (PheGdkDisplay *d)
{
  PheGdkWindow *w = g_object_new (PHE_TYPE_GDK_WINDOW, NULL);
  w->screen = d->screen; w->x = 0; w->y = 0; w->width = d->screen->width; w->height = d->screen->height;
  w->surface = cairo_image_surface_create_for_data (d->pixels, CAIRO_FORMAT_RGB24, w->width, w->height, d->stride);
  d->windows = g_list_append (d->windows, w);
  return w;
}
void phe_gdk_window_set_expose_func (PheGdkWindow *w, PheGdkExposeFunc f, gpointer data) { w->expose_func = f; w->expose_data = data; }

void phe_gdk_window_invalidate_rect (PheGdkWindow *w, int x, int y, int width, int height)
{
  int x2 = x + width, y2 = y + height;
  if (x < 0) x = 0; if (y < 0) y = 0; if (x2 > w->width) x2 = w->width; if (y2 > w->height) y2 = w->height;
  if (x2 <= x || y2 <= y) return;
  if (!w->dirty) { w->dirty = TRUE; w->dirty_rect.x = x; w->dirty_rect.y = y; w->dirty_rect.width = x2 - x; w->dirty_rect.height = y2 - y; }
  else {
    int ox2 = w->dirty_rect.x + w->dirty_rect.width, oy2 = w->dirty_rect.y + w->dirty_rect.height;
    if (x < w->dirty_rect.x) w->dirty_rect.x = x; if (y < w->dirty_rect.y) w->dirty_rect.y = y;
    if (x2 < ox2) x2 = ox2; if (y2 < oy2) y2 = oy2;
    w->dirty_rect.width = x2 - w->dirty_rect.x; w->dirty_rect.height = y2 - w->dirty_rect.y;
  }
}

void phe_gdk_window_process_updates (PheGdkWindow *w)
{
  cairo_rectangle_int_t r; char m[160];
  if (!w->dirty) return;
  r = w->dirty_rect; w->dirty = FALSE;
  if (w->expose_func) {
    cairo_t *cr = cairo_create (w->surface);
    cairo_rectangle (cr, r.x, r.y, r.width, r.height); cairo_clip (cr);
    w->expose_func (w, cr, &r, w->expose_data);
    cairo_destroy (cr);
  }
  cairo_surface_flush (w->surface);
  phe_ws_present (r.x, r.y, r.width, r.height);
  w->updates++; w->last_present_w = r.width; w->last_present_h = r.height;
  snprintf (m, sizeof m, "[phegdkwin] process_updates #%d: presented %d,%d %dx%d of %dx%d", w->updates, r.x, r.y, r.width, r.height, w->width, w->height); phe_log (m);
}

cairo_surface_t *phe_gdk_pixbuf_surface (GdkPixbuf *pb)
{
  int w = gdk_pixbuf_get_width (pb), h = gdk_pixbuf_get_height (pb), nc = gdk_pixbuf_get_n_channels (pb), rs = gdk_pixbuf_get_rowstride (pb), x, y, ds;
  const guchar *src = gdk_pixbuf_get_pixels (pb); cairo_surface_t *s = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h); unsigned char *d;
  cairo_surface_flush (s); d = cairo_image_surface_get_data (s); ds = cairo_image_surface_get_stride (s);
  for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
    const guchar *p = src + y * rs + x * nc; unsigned a = nc == 4 ? p[3] : 255; unsigned r = p[0] * a / 255, g = p[1] * a / 255, b = p[2] * a / 255;   /* premultiplied */
    *(unsigned *) (d + y * ds + 4 * x) = (a << 24) | (r << 16) | (g << 8) | b; }
  cairo_surface_mark_dirty (s); return s;
}
