/* phe_cairo_test.c -- cairo 1.16.0 (image backend) on the x32 image: fill, stroke, a gradient, a transform and a clip into an ARGB32 surface, checked pixel by pixel. BSD-2-Clause (Phorepoc). */
#include <cairo.h>
#include <stdio.h>
#include <string.h>
extern void phe_log (const char *msg);
static int near_ (unsigned a, unsigned b) { int s; for (s = 0; s < 32; s += 8) { int x = (int) ((a >> s) & 0xff), y = (int) ((b >> s) & 0xff); if (x - y > 3 || y - x > 3) return 0; } return 1; }
#define T(c, what) do { checks++; if (!(c)) { fails++; snprintf (m, sizeof m, "cairo FAIL: %s (line %d)", what, __LINE__); phe_log (m); } } while (0)
static unsigned px (cairo_surface_t *s, int x, int y) { return *(unsigned *) (cairo_image_surface_get_data (s) + y * cairo_image_surface_get_stride (s) + 4 * x); }

int phe_cairo_test (void)
{
  int checks = 0, fails = 0; char m[220];
  cairo_surface_t *s = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 64, 64); cairo_t *cr = cairo_create (s);
  T (cairo_surface_status (s) == CAIRO_STATUS_SUCCESS && cairo_status (cr) == CAIRO_STATUS_SUCCESS, "create");
  cairo_set_source_rgb (cr, 0, 0, 1); cairo_paint (cr);                                             /* blue background */
  T (near_ (px (s, 5, 5), 0xff0000ffu), "paint blue");
  cairo_set_source_rgba (cr, 1, 0, 0, 0.5); cairo_rectangle (cr, 8, 8, 16, 16); cairo_fill (cr);    /* 50% red over blue */
  T (near_ (px (s, 10, 10), 0xff800080u), "fill red 50% over blue");
  T (near_ (px (s, 4, 4), 0xff0000ffu), "outside the rectangle");
  cairo_set_source_rgb (cr, 0, 1, 0); cairo_set_line_width (cr, 4); cairo_move_to (cr, 4, 50); cairo_line_to (cr, 60, 50); cairo_stroke (cr);   /* green line */
  T (near_ (px (s, 30, 50), 0xff00ff00u) && near_ (px (s, 30, 40), 0xff0000ffu), "stroke a horizontal line");
  { cairo_pattern_t *g = cairo_pattern_create_linear (0, 0, 64, 0); cairo_pattern_add_color_stop_rgb (g, 0, 0, 0, 0); cairo_pattern_add_color_stop_rgb (g, 1, 1, 1, 1);
    cairo_save (cr); cairo_set_source (cr, g); cairo_rectangle (cr, 0, 56, 64, 8); cairo_fill (cr); cairo_restore (cr); cairo_pattern_destroy (g);
    { unsigned left = px (s, 2, 60) & 0xff, mid = px (s, 32, 60) & 0xff, right = px (s, 61, 60) & 0xff; T (left < 12 && mid > 116 && mid < 140 && right > 243, "linear gradient left/mid/right"); } }
  cairo_save (cr); cairo_rectangle (cr, 40, 8, 10, 10); cairo_clip (cr); cairo_translate (cr, 40, 8); cairo_scale (cr, 2, 2); cairo_set_source_rgb (cr, 1, 1, 0); cairo_rectangle (cr, 0, 0, 20, 20); cairo_fill (cr); cairo_restore (cr);
  T (near_ (px (s, 45, 12), 0xffffff00u) && near_ (px (s, 52, 12), 0xff0000ffu), "clip + translate + scale");
  snprintf (m, sizeof m, "cairo %s: %d checks, %d failures", cairo_version_string (), checks, fails); phe_log (m);
  cairo_destroy (cr); cairo_surface_destroy (s);
  return fails;
}
