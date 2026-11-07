#define PANGO_ENABLE_BACKEND 1
/* phe_pango_test.c -- HarfBuzz 15.1.0 shaping straight from the ROM font, then Pango 1.40.14 (PangoCairo over fontconfig + FreeType) laying out and drawing text into an ARGB32 cairo surface. BSD-2-Clause (Phorepoc). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hb.h>
#include <cairo.h>
#include <pango/pangocairo.h>
#include <fontconfig/fontconfig.h>
#include <pango/pangofc-font.h>
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
  /* 1a. type layout: the instance sizes GObject registered vs the structs this compile sees (a mismatch = the qdata overwrite) */
  { const char *names[] = { "PangoFont", "PangoFcFont", "PangoCairoFcFont", "PangoContext", "PangoFontMap", NULL }; int i; GTypeQuery q;
    for (i = 0; names[i]; i++) { GType t = g_type_from_name (names[i]); if (!t) { snprintf (m, sizeof m, "pango type %s: not registered yet", names[i]); phe_log (m); continue; }
      g_type_query (t, &q); snprintf (m, sizeof m, "pango type %s: instance_size=%u class_size=%u", names[i], q.instance_size, q.class_size); phe_log (m); }
    snprintf (m, sizeof m, "pango sizeof: GObject=%u PangoFont=%u PangoFcFont=%u PangoContext?=- gsize=%u glong=%u gpointer=%u", (unsigned) sizeof (GObject), (unsigned) sizeof (PangoFont), (unsigned) sizeof (PangoFcFont), (unsigned) sizeof (gsize), (unsigned) sizeof (glong), (unsigned) sizeof (gpointer)); phe_log (m); }
  /* 1b. fontconfig alone: which config file, which directories, how many fonts */
  { FcConfig *cfg; FcStrList *dl; FcChar8 *dir; FcFontSet *fs; int nd = 0, nf = 0; char dn[200] = "";
    T (FcInit (), "FcInit"); cfg = FcConfigGetCurrent (); dl = FcConfigGetFontDirs (cfg); while (dl && (dir = FcStrListNext (dl))) { nd++; if (strlen (dn) < 150) { strcat (dn, (char *) dir); strcat (dn, " "); } }
    fs = FcConfigGetFonts (cfg, FcSetSystem); nf = fs ? fs->nfont : -1;
    snprintf (m, sizeof m, "fontconfig %d: dirs=%d [%s] fonts=%d, config files: %s", FC_VERSION, nd, dn, nf, FcConfigGetFilename (cfg, (const FcChar8 *) "z:/resource/pheglibtest/fonts.conf") ? "found" : "NOT found"); phe_log (m);
    { FcPattern *pat = FcPatternCreate (), *mt; FcResult r; FcChar8 *fam = NULL, *file = NULL; FcPatternAddString (pat, FC_FAMILY, (const FcChar8 *) "Sans"); FcConfigSubstitute (cfg, pat, FcMatchPattern); FcDefaultSubstitute (pat);
      mt = FcFontMatch (cfg, pat, &r); if (mt) { FcPatternGetString (mt, FC_FAMILY, 0, &fam); FcPatternGetString (mt, FC_FILE, 0, &file); }
      snprintf (m, sizeof m, "fontconfig: match for Sans = %s file %s", fam ? (char *) fam : "(none)", file ? (char *) file : "(none)"); phe_log (m); T (mt != NULL, "FcFontMatch"); if (mt) FcPatternDestroy (mt); FcPatternDestroy (pat); }
    T (nf > 0, "fontconfig sees fonts"); }
  /* 2. Pango: a layout through PangoCairo, drawn black on white */
  { cairo_surface_t *s = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 220, 48); cairo_t *cr = cairo_create (s); PangoLayout *lay; PangoFontDescription *fd; int w = 0, h = 0, x, y; long dark = 0; unsigned char *d; int st;
    cairo_set_source_rgb (cr, 1, 1, 1); cairo_paint (cr); cairo_set_source_rgb (cr, 0, 0, 0);
    lay = pango_cairo_create_layout (cr); fd = pango_font_description_from_string ("Sans 20"); pango_layout_set_font_description (lay, fd);
    pango_layout_set_text (lay, "Hildon Pango", -1); pango_layout_get_pixel_size (lay, &w, &h);
    snprintf (m, sizeof m, "pango %s: layout \"Hildon Pango\" %dx%d px, lines %d", pango_version_string (), w, h, pango_layout_get_line_count (lay)); phe_log (m);
    T (w > 60 && w < 220 && h > 10 && h < 48, "layout size plausible"); T (pango_layout_get_line_count (lay) == 1, "one line");
    { PangoLayoutLine *ln = pango_layout_get_line_readonly (lay, 0); PangoFont *font = (ln && ln->runs) ? ((PangoLayoutRun *) ln->runs->data)->item->analysis.font : NULL;
      snprintf (m, sizeof m, "pango: font map %s, run font %p (%s)", G_OBJECT_TYPE_NAME (pango_cairo_font_map_get_default ()), (void *) font, font ? G_OBJECT_TYPE_NAME (font) : "-"); phe_log (m); T (font != NULL, "layout run has a fontconfig font"); }
    cairo_move_to (cr, 4, 4); pango_cairo_show_layout (cr, lay); cairo_surface_flush (s);
    d = cairo_image_surface_get_data (s); st = cairo_image_surface_get_stride (s);
    for (y = 0; y < 48; y++) for (x = 0; x < 220; x++) if ((*(unsigned *) (d + y * st + 4 * x) & 0xff) < 128) dark++;
    snprintf (m, sizeof m, "pango: %ld dark pixels", dark); phe_log (m); T (dark > 150 && dark < 5000, "text drawn");
    pango_font_description_free (fd); g_object_unref (lay); cairo_destroy (cr); cairo_surface_destroy (s); }
  snprintf (m, sizeof m, "pango/harfbuzz: %d checks, %d failures", checks, fails); phe_log (m);
  return fails;
}
