/* phegdkwin_paint.c -- gdk-symbian skeleton + the demo scene (Pango layouts, a gdk-pixbuf PNG) drawn into a PheGdkWindow. BSD-2-Clause (Phorepoc). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "phegdk.h"
#include "phe_png_data.h"
extern void phe_log (const char *msg);

PheGdkWindow *phe_gdk_window_new (unsigned char *pixels, int width, int height, int stride)
{
  PheGdkWindow *w = g_new0 (PheGdkWindow, 1);
  w->width = width; w->height = height; w->stride = stride; w->pixels = pixels;
  w->surface = cairo_image_surface_create_for_data (pixels, CAIRO_FORMAT_RGB24, width, height, stride);
  return w;
}
cairo_t *phe_gdk_window_begin_paint (PheGdkWindow *w) { if (!w->cr) w->cr = cairo_create (w->surface); return w->cr; }
void phe_gdk_window_end_paint (PheGdkWindow *w) { if (w->cr) { cairo_destroy (w->cr); w->cr = NULL; } cairo_surface_flush (w->surface); }
void phe_gdk_window_free (PheGdkWindow *w) { phe_gdk_window_end_paint (w); cairo_surface_destroy (w->surface); g_free (w); }

cairo_surface_t *phe_gdk_pixbuf_surface (GdkPixbuf *pb)
{
  int w = gdk_pixbuf_get_width (pb), h = gdk_pixbuf_get_height (pb), nc = gdk_pixbuf_get_n_channels (pb), rs = gdk_pixbuf_get_rowstride (pb), x, y;
  const guchar *src = gdk_pixbuf_get_pixels (pb); cairo_surface_t *s = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h); unsigned char *d;
  int ds; cairo_surface_flush (s); d = cairo_image_surface_get_data (s); ds = cairo_image_surface_get_stride (s);
  for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
    const guchar *p = src + y * rs + x * nc; unsigned a = nc == 4 ? p[3] : 255; unsigned r = p[0] * a / 255, g = p[1] * a / 255, b = p[2] * a / 255;   /* premultiplied */
    *(unsigned *) (d + y * ds + 4 * x) = (a << 24) | (r << 16) | (g << 8) | b; }
  cairo_surface_mark_dirty (s); return s;
}

static void round_rect (cairo_t *cr, double x, double y, double w, double h, double r)
{
  cairo_new_sub_path (cr);
  cairo_arc (cr, x + w - r, y + r, r, -1.5708, 0); cairo_arc (cr, x + w - r, y + h - r, r, 0, 1.5708);
  cairo_arc (cr, x + r, y + h - r, r, 1.5708, 3.14159); cairo_arc (cr, x + r, y + r, r, 3.14159, 4.71239); cairo_close_path (cr);
}

static void text (cairo_t *cr, const char *font, const char *str, double x, double y, double a, int *w_out)
{
  PangoLayout *lay = pango_cairo_create_layout (cr); PangoFontDescription *fd = pango_font_description_from_string (font); int tw = 0, th = 0;
  pango_layout_set_font_description (lay, fd); pango_font_description_free (fd); pango_layout_set_text (lay, str, -1); pango_layout_get_pixel_size (lay, &tw, &th);
  cairo_set_source_rgba (cr, 1, 1, 1, a); cairo_move_to (cr, x, y); pango_cairo_show_layout (cr, lay); g_object_unref (lay); if (w_out) *w_out = tw;
}

int phe_gdk_demo_paint (PheGdkWindow *win)
{
  char m[200]; double w = win->width, h = win->height; int rc = 0, tw = 0;
  cairo_t *cr = phe_gdk_window_begin_paint (win);
  cairo_pattern_t *g = cairo_pattern_create_linear (0, 0, 0, h);
  cairo_pattern_add_color_stop_rgb (g, 0, 0.10, 0.16, 0.30); cairo_pattern_add_color_stop_rgb (g, 1, 0.02, 0.05, 0.10); cairo_set_source (cr, g); cairo_paint (cr); cairo_pattern_destroy (g);
  { double mg = w * 0.07, cw = w - 2 * mg, ch = h * 0.42, cy = h * 0.08;
    round_rect (cr, mg, cy, cw, ch, w * 0.04); cairo_set_source_rgba (cr, 1, 1, 1, 0.16); cairo_fill_preserve (cr); cairo_set_source_rgba (cr, 1, 1, 1, 0.55); cairo_set_line_width (cr, 2); cairo_stroke (cr);
    text (cr, "Sans 30", "Hildon", mg + w * 0.05, cy + ch * 0.07, 1.0, &tw);
    text (cr, "Sans 17", "Pango 1.40 + HarfBuzz", mg + w * 0.05, cy + ch * 0.34, 0.9, NULL);
    text (cr, "Sans 17", "gdk-pixbuf + libpng", mg + w * 0.05, cy + ch * 0.50, 0.9, NULL);
    text (cr, "Sans 12", "GLib, cairo, Pango on Phorepoc", mg + w * 0.05, cy + ch * 0.70, 0.7, NULL);
    snprintf (m, sizeof m, "[phegdkwin] pango text drawn, first layout %d px wide", tw); phe_log (m); if (tw < 40) rc |= 1;
    /* a PNG decoded by gdk-pixbuf, scaled, drawn under the card */
    { GError *err = NULL; GdkPixbufLoader *ld = gdk_pixbuf_loader_new (); GdkPixbuf *pb; gboolean ok = gdk_pixbuf_loader_write (ld, phe_png, sizeof phe_png, &err) && gdk_pixbuf_loader_close (ld, &err);
      pb = ok ? gdk_pixbuf_loader_get_pixbuf (ld) : NULL;
      if (pb) { GdkPixbuf *sc = gdk_pixbuf_scale_simple (pb, (int) (w * 0.6), (int) (w * 0.3), GDK_INTERP_NEAREST); cairo_surface_t *ps = phe_gdk_pixbuf_surface (sc);
        cairo_set_source_surface (cr, ps, w * 0.2, h * 0.62); cairo_paint (cr); cairo_surface_destroy (ps); g_object_unref (sc);
        snprintf (m, sizeof m, "[phegdkwin] png decoded %dx%d and drawn", gdk_pixbuf_get_width (pb), gdk_pixbuf_get_height (pb)); phe_log (m); }
      else { rc |= 2; snprintf (m, sizeof m, "[phegdkwin] png decode failed: %s", err ? err->message : "-"); phe_log (m); }
      g_clear_error (&err); g_object_unref (ld); }
    text (cr, "Sans 12", "8x4 PNG via gdk-pixbuf, scaled", w * 0.2, h * 0.62 + w * 0.3 + 8, 0.8, NULL); }
  phe_gdk_window_end_paint (win);
  return rc;
}
