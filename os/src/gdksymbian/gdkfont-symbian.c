/* gdkfont-symbian.c -- the deprecated GdkFont API of the Symbian backend, over Pango (fontconfig + FreeType + HarfBuzz): a GdkFont is a Pango font description with its metrics; widths and extents
 * come from a PangoLayout, drawing goes through the cairo path of the drawable (gdkdrawable-symbian.c calls _gdk_symbian_font_show). Font names are Pango descriptions ("Sans 12") or X logical font
 * descriptions ("-*-sans-bold-r-normal-*-14-*-*-*-*-*-*-*": family, weight, slant and pixel size are used). Text is UTF-8; a string that is not valid UTF-8 is read as Latin-1,
 * the character set of X core fonts. BSD-2-Clause (Phorepoc). */
#include "config.h"
#include <string.h>
#include <pango/pangocairo.h>
#include "gdksymbian.h"
#include "gdkfont.h"
#include "gdkprivate.h"
#include "gdkinternals.h"

typedef struct { GdkFontPrivate base; PangoFontDescription *desc; PangoContext *context; } GdkFontSymbian;
#define FONT(f) ((GdkFontSymbian *) (f))

static PangoContext *
make_context (void)
{
  PangoFontMap *fm = pango_cairo_font_map_get_default ();
  return pango_font_map_create_context (fm);
}

static GdkFont *
font_from_description (PangoFontDescription *desc)
{
  GdkFontSymbian *f = g_new0 (GdkFontSymbian, 1); PangoFontMetrics *m;
  f->base.ref_count = 1; f->desc = pango_font_description_copy (desc); f->context = make_context ();
  pango_context_set_font_description (f->context, f->desc);
  m = pango_context_get_metrics (f->context, f->desc, NULL);
  f->base.font.type = GDK_FONT_FONT;
  f->base.font.ascent = PANGO_PIXELS (pango_font_metrics_get_ascent (m)); f->base.font.descent = PANGO_PIXELS (pango_font_metrics_get_descent (m));
  pango_font_metrics_unref (m);
  return (GdkFont *) f;
}

GdkFont *
gdk_font_from_description_for_display (GdkDisplay *display, PangoFontDescription *font_desc)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL); g_return_val_if_fail (font_desc != NULL, NULL);
  return font_from_description (font_desc);
}

/* an XLFD name: -foundry-family-weight-slant-width-style-pixels-points-resx-resy-spacing-avgwidth-registry-encoding */
static PangoFontDescription *
description_of_name (const gchar *name)
{
  PangoFontDescription *d;
  if (name[0] == '-') {
    gchar **f = g_strsplit (name, "-", 16); guint n = g_strv_length (f); gint px = 0;
    d = pango_font_description_new ();
    if (n > 2 && f[2][0] && strcmp (f[2], "*") != 0) pango_font_description_set_family (d, f[2]); else pango_font_description_set_family (d, "sans");
    if (n > 3 && !g_ascii_strcasecmp (f[3], "bold")) pango_font_description_set_weight (d, PANGO_WEIGHT_BOLD);
    if (n > 3 && !g_ascii_strcasecmp (f[3], "light")) pango_font_description_set_weight (d, PANGO_WEIGHT_LIGHT);
    if (n > 4 && !g_ascii_strcasecmp (f[4], "i")) pango_font_description_set_style (d, PANGO_STYLE_ITALIC);
    if (n > 4 && !g_ascii_strcasecmp (f[4], "o")) pango_font_description_set_style (d, PANGO_STYLE_OBLIQUE);
    if (n > 7) px = atoi (f[7]);
    if (px > 0) pango_font_description_set_absolute_size (d, px * PANGO_SCALE); else pango_font_description_set_size (d, 12 * PANGO_SCALE);
    g_strfreev (f);
    return d;
  }
  return pango_font_description_from_string (name);
}

GdkFont *
gdk_font_load_for_display (GdkDisplay *display, const gchar *font_name)
{
  PangoFontDescription *d; GdkFont *f;
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL); g_return_val_if_fail (font_name != NULL, NULL);
  d = description_of_name (font_name); f = font_from_description (d); pango_font_description_free (d);
  return f;
}

/* a font set is a font that has every character: Pango falls back per character, so one description serves; the name may list several, the first is used */
GdkFont *
gdk_fontset_load_for_display (GdkDisplay *display, const gchar *fontset_name)
{
  gchar **parts; GdkFont *f;
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL); g_return_val_if_fail (fontset_name != NULL, NULL);
  parts = g_strsplit (fontset_name, ",", 2); f = gdk_font_load_for_display (display, parts[0]); g_strfreev (parts);
  return f;
}

void
_gdk_font_destroy (GdkFont *font)
{
  GdkFontSymbian *f = FONT (font);
  pango_font_description_free (f->desc); g_object_unref (f->context); g_free (f);
}

gint _gdk_font_strlen (GdkFont *font, const gchar *str) { g_return_val_if_fail (font != NULL && str != NULL, -1); return (gint) strlen (str); }
gint gdk_font_id (const GdkFont *font) { g_return_val_if_fail (font != NULL, 0); return (gint) (gsize) font; }
gboolean gdk_font_equal (const GdkFont *fonta, const GdkFont *fontb) { g_return_val_if_fail (fonta != NULL && fontb != NULL, FALSE); return pango_font_description_equal (FONT (fonta)->desc, FONT (fontb)->desc); }

/* the text as UTF-8: as is when valid, else Latin-1 */
static gchar *
utf8_of (const gchar *text, gint length)
{
  gchar *s = g_strndup (text, length);
  if (!g_utf8_validate (s, -1, NULL)) { gchar *u = g_convert (s, -1, "UTF-8", "ISO-8859-1", NULL, NULL, NULL); g_free (s); s = u ? u : g_strdup (""); }
  return s;
}
static gchar *utf8_of_wc (const GdkWChar *text, gint length) { gchar *s = g_ucs4_to_utf8 ((const gunichar *) text, length, NULL, NULL, NULL); return s ? s : g_strdup (""); }

static PangoLayout *
layout_of (GdkFont *font, const gchar *utf8)
{
  PangoLayout *l = pango_layout_new (FONT (font)->context);
  pango_layout_set_text (l, utf8, -1);
  return l;
}

static void
extents_utf8 (GdkFont *font, const gchar *utf8, gint *lbearing, gint *rbearing, gint *width, gint *ascent, gint *descent)
{
  PangoLayout *l = layout_of (font, utf8); PangoRectangle ink, logical; gint baseline;
  pango_layout_get_pixel_extents (l, &ink, &logical); baseline = pango_layout_get_baseline (l) / PANGO_SCALE;
  if (lbearing) *lbearing = ink.x;
  if (rbearing) *rbearing = ink.x + ink.width;
  if (width) *width = logical.width;
  if (ascent) *ascent = baseline - ink.y;
  if (descent) *descent = ink.y + ink.height - baseline;
  g_object_unref (l);
}

void
gdk_text_extents (GdkFont *font, const gchar *text, gint text_length, gint *lbearing, gint *rbearing, gint *width, gint *ascent, gint *descent)
{
  gchar *u;
  g_return_if_fail (font != NULL); g_return_if_fail (text != NULL);
  u = utf8_of (text, text_length); extents_utf8 (font, u, lbearing, rbearing, width, ascent, descent); g_free (u);
}
void
gdk_text_extents_wc (GdkFont *font, const GdkWChar *text, gint text_length, gint *lbearing, gint *rbearing, gint *width, gint *ascent, gint *descent)
{
  gchar *u;
  g_return_if_fail (font != NULL); g_return_if_fail (text != NULL);
  u = utf8_of_wc (text, text_length); extents_utf8 (font, u, lbearing, rbearing, width, ascent, descent); g_free (u);
}
gint gdk_text_width (GdkFont *font, const gchar *text, gint text_length) { gint w; gdk_text_extents (font, text, text_length, NULL, NULL, &w, NULL, NULL); return w; }
gint gdk_text_width_wc (GdkFont *font, const GdkWChar *text, gint text_length) { gint w; gdk_text_extents_wc (font, text, text_length, NULL, NULL, &w, NULL, NULL); return w; }

/* draw text with its baseline origin at (x, y) on a cairo context whose source and state the GC has set */
void
_gdk_symbian_font_show (cairo_t *cr, GdkFont *font, gint x, gint y, const gchar *utf8)
{
  PangoLayout *l = layout_of (font, utf8);
  cairo_move_to (cr, x, y - pango_layout_get_baseline (l) / PANGO_SCALE);
  pango_cairo_show_layout (cr, l);
  g_object_unref (l);
}
gchar *_gdk_symbian_font_utf8 (const gchar *text, gint length) { return utf8_of (text, length); }
gchar *_gdk_symbian_font_utf8_wc (const GdkWChar *text, gint length) { return utf8_of_wc (text, length); }
