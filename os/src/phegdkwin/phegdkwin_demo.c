/* phegdkwin_demo.c -- gdk-symbian skeleton demo: an expose handler (gradient, a card with Pango text, a gdk-pixbuf PNG, a live status box, pointer dots),
 * a g_timeout that bumps a counter, and key/pointer events from WSERV; every change invalidates only its own rectangle and process_updates presents only that. */
#include <stdio.h>
#include <string.h>
#include "phegdk.h"
#include "phe_png_data.h"

typedef struct { PheGdkDisplay *disp; guint last_iter; PheGdkWindow *win; int ticks, keys, pointers; double dots[16][2]; cairo_surface_t *png; int box_x, box_y, box_w, box_h; guint timeout_id; } Demo;

static void round_rect (cairo_t *cr, double x, double y, double w, double h, double r)
{
  cairo_new_sub_path (cr);
  cairo_arc (cr, x + w - r, y + r, r, -1.5708, 0); cairo_arc (cr, x + w - r, y + h - r, r, 0, 1.5708);
  cairo_arc (cr, x + r, y + h - r, r, 1.5708, 3.14159); cairo_arc (cr, x + r, y + r, r, 3.14159, 4.71239); cairo_close_path (cr);
}
static void text (cairo_t *cr, const char *font, const char *str, double x, double y, double a)
{
  PangoLayout *lay = pango_cairo_create_layout (cr); PangoFontDescription *fd = pango_font_description_from_string (font);
  pango_layout_set_font_description (lay, fd); pango_font_description_free (fd); pango_layout_set_text (lay, str, -1);
  cairo_set_source_rgba (cr, 1, 1, 1, a); cairo_move_to (cr, x, y); pango_cairo_show_layout (cr, lay); g_object_unref (lay);
}

static void expose (PheGdkWindow *win, cairo_t *cr, const cairo_rectangle_int_t *area, gpointer data)
{
  Demo *d = data; double w = win->width, h = win->height; char buf[96]; int i;
  double mg = w * 0.07, cw = w - 2 * mg, ch = h * 0.42, cy = h * 0.08; cairo_pattern_t *g;
  (void) area;
  g = cairo_pattern_create_linear (0, 0, 0, h);
  cairo_pattern_add_color_stop_rgb (g, 0, 0.10, 0.16, 0.30); cairo_pattern_add_color_stop_rgb (g, 1, 0.02, 0.05, 0.10); cairo_set_source (cr, g); cairo_paint (cr); cairo_pattern_destroy (g);
  round_rect (cr, mg, cy, cw, ch, w * 0.04); cairo_set_source_rgba (cr, 1, 1, 1, 0.16); cairo_fill_preserve (cr); cairo_set_source_rgba (cr, 1, 1, 1, 0.55); cairo_set_line_width (cr, 2); cairo_stroke (cr);
  text (cr, "Sans 30", "Hildon", mg + w * 0.05, cy + ch * 0.07, 1.0);
  text (cr, "Sans 17", "gdk-symbian skeleton", mg + w * 0.05, cy + ch * 0.34, 0.9);
  text (cr, "Sans 17", "Pango + gdk-pixbuf", mg + w * 0.05, cy + ch * 0.50, 0.9);
  text (cr, "Sans 12", "GLib main loop on WSERV", mg + w * 0.05, cy + ch * 0.70, 0.7);
  if (d->png) { cairo_set_source_surface (cr, d->png, w * 0.2, h * 0.70); cairo_paint (cr); }
  /* the live status box: only this rectangle is redrawn on a tick or a key */
  round_rect (cr, d->box_x, d->box_y, d->box_w, d->box_h, 8); cairo_set_source_rgba (cr, 0.05, 0.4, 0.2, 0.55); cairo_fill (cr);
  snprintf (buf, sizeof buf, "ticks %d   keys %d   taps %d", d->ticks, d->keys, d->pointers);
  text (cr, "Sans 14", buf, d->box_x + 14, d->box_y + 8, 1.0);
  for (i = 0; i < d->pointers && i < 16; i++) { cairo_set_source_rgba (cr, 0.95, 0.75, 0.15, 0.9); cairo_arc (cr, d->dots[i][0], d->dots[i][1], 14, 0, 6.28318); cairo_fill (cr); }
}

static void invalidate_box (Demo *d) { phe_gdk_window_invalidate_rect (d->win, d->box_x, d->box_y, d->box_w, d->box_h); }

static gboolean on_tick (gpointer data)
{
  Demo *d = data; char m[100]; d->ticks++; invalidate_box (d);
  snprintf (m, sizeof m, "[phegdkwin] tick %d", d->ticks); phe_log (m);
  return d->ticks < 5 ? G_SOURCE_CONTINUE : G_SOURCE_REMOVE;          /* five ticks, then the demo goes idle: nothing wakes the loop but WSERV and the stats timer */
}
static gboolean on_stats (gpointer data)
{
  Demo *d = data; char m[140]; guint n = d->disp->iterations - d->last_iter; d->last_iter = d->disp->iterations;
  snprintf (m, sizeof m, "[phegdkwin] loop iterations in the last 2 s: %u", n); phe_log (m); return G_SOURCE_CONTINUE;
}

static void on_event (const PheGdkEvent *ev, gpointer data)
{
  Demo *d = data; char m[160];
  if (ev->type == PHE_KEY_PRESS || ev->type == PHE_KEY_RELEASE) {
    if (ev->type == PHE_KEY_PRESS) { d->keys++; invalidate_box (d); }
    snprintf (m, sizeof m, "[phegdkwin] GdkEvent KEY_%s keyval=0x%x hardware_keycode=0x%x (keys=%d)", ev->type == PHE_KEY_PRESS ? "PRESS" : "RELEASE", ev->keyval, ev->hardware_keycode, d->keys); phe_log (m);
  } else {
    snprintf (m, sizeof m, "[phegdkwin] GdkEvent %s at %.0f,%.0f state=0x%x button=%u", ev->type == PHE_BUTTON_PRESS ? "BUTTON_PRESS" : ev->type == PHE_BUTTON_RELEASE ? "BUTTON_RELEASE" : "MOTION_NOTIFY", ev->x, ev->y, ev->state, ev->button); phe_log (m);
    if (ev->type == PHE_BUTTON_PRESS && d->pointers < 16) { d->dots[d->pointers][0] = ev->x; d->dots[d->pointers][1] = ev->y; d->pointers++;
      phe_gdk_window_invalidate_rect (d->win, (int) ev->x - 16, (int) ev->y - 16, 32, 32); invalidate_box (d); }
  }
}

void phe_gdk_demo_run (PheGdkDisplay *disp, PheGdkWindow *win)
{
  static Demo demo; Demo *d = &demo; GMainLoop *loop; GdkPixbufLoader *ld; GError *err = NULL; char m[120];
  d->win = win; d->disp = disp; d->box_x = (int) (win->width * 0.07); d->box_w = (int) (win->width * 0.86); d->box_h = 40; d->box_y = (int) (win->height * 0.54);
  ld = gdk_pixbuf_loader_new ();
  if (gdk_pixbuf_loader_write (ld, phe_png, sizeof phe_png, &err) && gdk_pixbuf_loader_close (ld, &err)) {
    GdkPixbuf *sc = gdk_pixbuf_scale_simple (gdk_pixbuf_loader_get_pixbuf (ld), (int) (win->width * 0.6), (int) (win->width * 0.2), GDK_INTERP_NEAREST);
    d->png = phe_gdk_pixbuf_surface (sc); g_object_unref (sc);
  }
  g_clear_error (&err); g_object_unref (ld);
  phe_gdk_window_set_expose_func (win, expose, d);
  phe_gdk_display_set_event_func (disp, on_event, d);
  phe_gdk_window_invalidate_rect (win, 0, 0, win->width, win->height);
  phe_gdk_window_process_updates (win);                                    /* the first paint: the whole window */
  d->timeout_id = g_timeout_add (400, on_tick, d); g_timeout_add (2000, on_stats, d);
  loop = g_main_loop_new (NULL, FALSE);
  snprintf (m, sizeof m, "[phegdkwin] loop running (%dx%d, status box %d,%d %dx%d)", win->width, win->height, d->box_x, d->box_y, d->box_w, d->box_h); phe_log (m);
  g_main_loop_run (loop);
}
