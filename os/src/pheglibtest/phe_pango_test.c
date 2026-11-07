/* phe_pango_test.c -- HarfBuzz 15.1.0 shaping straight from the ROM font, then Pango 1.40.14 (PangoCairo over fontconfig + FreeType) laying out and drawing text into an ARGB32 cairo surface. BSD-2-Clause (Phorepoc). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hb.h>
#include <cairo.h>
#include <pango/pangocairo.h>
extern void phe_log (const char *msg);
#define T(c, what) do { checks++; if (!(c)) { fails++; snprintf (m, sizeof m, "pango FAIL: %s (line %d)", what, __LINE__); phe_log (m); } } while (0)

int phe_pango_test (void)
{
  int checks = 0, fails = 0; char m[260];
  /* 1. HarfBuzz alone: a face from the font file's bytes, shape "Hello" */
  { FILE *f = fopen ("z:/resource/fonts/s60sans_r.ttf", "rb"); long n = 0; char *data = NULL;
    if (f) { fseek (f, 0, SEEK_END); n = ftell (f); fseek (f, 0, SEEK_SET); data = (char *) malloc (n); if (data && fread (data, 1, n, f) != (size_t) n) { free (data); data = NULL; } fclose (f); }
    T (data != NULL, "read ROM font");
    if (data) {
      hb_blob_t *blob = hb_blob_create (data, (unsigned) n, HB_MEMORY_MODE_READONLY, NULL, NULL); hb_face_t *face = hb_face_create (blob, 0); hb_font_t *font = hb_font_create (face);
      hb_buffer_t *buf = hb_buffer_create (); unsigned cnt = 0, i; long adv = 0; hb_glyph_info_t *gi; hb_glyph_position_t *gp;
      T (hb_face_get_glyph_count (face) > 100, "face glyph count"); T (hb_face_get_upem (face) > 0, "upem");
      hb_buffer_add_utf8 (buf, "Hello", -1, 0, -1); hb_buffer_guess_segment_properties (buf); hb_shape (font, buf, NULL, 0);
      gi = hb_buffer_get_glyph_infos (buf, &cnt); gp = hb_buffer_get_glyph_positions (buf, &cnt);
      for (i = 0; i < cnt; i++) adv += gp[i].x_advance;
      snprintf (m, sizeof m, "harfbuzz %s: %u glyphs, advance %ld, upem %u, first glyph id %u", hb_version_string (), cnt, adv, hb_face_get_upem (face), cnt ? gi[0].codepoint : 0); phe_log (m);
      T (cnt == 5 && adv > 0 && gi[0].codepoint != 0, "shape Hello");
      hb_buffer_destroy (buf); hb_font_destroy (font); hb_face_destroy (face); hb_blob_destroy (blob);
      free (data);
    } }
  /* 2. Pango: a layout through PangoCairo, drawn black on white */
  { cairo_surface_t *s = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 220, 48); cairo_t *cr = cairo_create (s); PangoLayout *lay; PangoFontDescription *fd; int w = 0, h = 0, x, y; long dark = 0; unsigned char *d; int st;
    cairo_set_source_rgb (cr, 1, 1, 1); cairo_paint (cr); cairo_set_source_rgb (cr, 0, 0, 0);
    lay = pango_cairo_create_layout (cr); fd = pango_font_description_from_string ("Sans 20"); pango_layout_set_font_description (lay, fd);
    pango_layout_set_text (lay, "Hildon Pango", -1); pango_layout_get_pixel_size (lay, &w, &h);
    snprintf (m, sizeof m, "pango %s: layout \"Hildon Pango\" %dx%d px, lines %d", pango_version_string (), w, h, pango_layout_get_line_count (lay)); phe_log (m);
    T (w > 60 && w < 220 && h > 10 && h < 48, "layout size plausible"); T (pango_layout_get_line_count (lay) == 1, "one line");
    { PangoFontMap *fm = pango_cairo_font_map_get_default (); PangoContext *ctx = pango_layout_get_context (lay); PangoFont *font = pango_font_map_load_font (fm, ctx, fd);
      snprintf (m, sizeof m, "pango: font map %s, loaded font %p", G_OBJECT_TYPE_NAME (fm), (void *) font); phe_log (m); T (font != NULL, "font loaded through fontconfig"); if (font) g_object_unref (font); }
    cairo_move_to (cr, 4, 4); pango_cairo_show_layout (cr, lay); cairo_surface_flush (s);
    d = cairo_image_surface_get_data (s); st = cairo_image_surface_get_stride (s);
    for (y = 0; y < 48; y++) for (x = 0; x < 220; x++) if ((*(unsigned *) (d + y * st + 4 * x) & 0xff) < 128) dark++;
    snprintf (m, sizeof m, "pango: %ld dark pixels", dark); phe_log (m); T (dark > 150 && dark < 5000, "text drawn");
    pango_font_description_free (fd); g_object_unref (lay); cairo_destroy (cr); cairo_surface_destroy (s); }
  snprintf (m, sizeof m, "pango/harfbuzz: %d checks, %d failures", checks, fails); phe_log (m);
  return fails;
}
