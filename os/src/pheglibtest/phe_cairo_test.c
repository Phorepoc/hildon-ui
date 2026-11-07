/* phe_cairo_test.c -- cairo 1.16.0 (image backend) on the x32 image: fill, stroke, a gradient, a transform and a clip into an ARGB32 surface, checked pixel by pixel. BSD-2-Clause (Phorepoc). */
#include <cairo.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <cairo-ft.h>
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
#ifdef CAIRO_HAS_FT_FONT
  { /* text through cairo-ft: a ROM font (Ubuntu-based "Series 60 Sans") opened by FreeType, glyphs rendered by cairo into black-on-white */
    FT_Library lib; FT_Face face; int err = FT_Init_FreeType (&lib), dark = 0, x, y;
    snprintf (m, sizeof m, "freetype: FT_Init_FreeType=%d", err); phe_log (m); T (err == 0, "FT_Init_FreeType");
    if (err == 0)
      {
        err = FT_New_Face (lib, "z:/resource/fonts/s60sans_r.ttf", 0, &face);
        snprintf (m, sizeof m, "freetype: FT_New_Face(z:/resource/fonts/s60sans_r.ttf)=%d", err); phe_log (m); T (err == 0, "FT_New_Face");
        if (err == 0)
          {
            cairo_surface_t *ts = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 200, 48); cairo_t *tc = cairo_create (ts); cairo_text_extents_t ex; cairo_font_face_t *ff;
            snprintf (m, sizeof m, "freetype: family [%s] style [%s] glyphs %ld units/em %d", face->family_name ? face->family_name : "-", face->style_name ? face->style_name : "-", (long) face->num_glyphs, (int) face->units_per_EM); phe_log (m);
            cairo_set_source_rgb (tc, 1, 1, 1); cairo_paint (tc);
            ff = cairo_ft_font_face_create_for_ft_face (face, 0); cairo_set_font_face (tc, ff); cairo_set_font_size (tc, 28); cairo_set_source_rgb (tc, 0, 0, 0);
            cairo_text_extents (tc, "Hildon", &ex); cairo_move_to (tc, 6, 34); cairo_show_text (tc, "Hildon"); cairo_surface_flush (ts);
            for (y = 0; y < 48; y++) for (x = 0; x < 200; x++) if ((px (ts, x, y) & 0xff) < 128) dark++;
            snprintf (m, sizeof m, "cairo text \"Hildon\": advance %.1f x %.1f, %d dark pixels, status %s", ex.x_advance, ex.height, dark, cairo_status_to_string (cairo_status (tc))); phe_log (m);
            T (cairo_status (tc) == CAIRO_STATUS_SUCCESS && ex.x_advance > 40 && dark > 150 && dark < 3000, "cairo-ft text renders glyphs");
            cairo_font_face_destroy (ff); cairo_destroy (tc); cairo_surface_destroy (ts);
            FT_Done_Face (face);
          }
        FT_Done_FreeType (lib);
      }
  }
#endif
  snprintf (m, sizeof m, "cairo %s: %d checks, %d failures", cairo_version_string (), checks, fails); phe_log (m);
  cairo_destroy (cr); cairo_surface_destroy (s);
  return fails;
}
