/* gdkvisual-symbian.c / colormap -- the one visual of the Symbian backend (24 bit TrueColor, 0x00RRGGBB, the pixel layout of CFbsBitmap EColor16MU = cairo RGB24) and the
 * colormap for it. A TrueColor colormap has no entries to allocate: a pixel is computed from the colour with the visual's masks. BSD-2-Clause (Phorepoc). */
#include "config.h"
#include <string.h>
#include "gdksymbian.h"
#include "gdkvisual.h"
#include "gdkcolor.h"
#include "gdkprivate.h"
#include "gdkinternals.h"

/* ---- GdkVisual ---- */
struct _GdkVisualClass { GObjectClass parent_class; };
G_DEFINE_TYPE (GdkVisual, gdk_visual, G_TYPE_OBJECT)
static void gdk_visual_init (GdkVisual *v) { (void) v; }
static void gdk_visual_class_init (GdkVisualClass *c) { (void) c; }

void
_gdk_visual_init (GdkScreen *screen)
{
  GdkScreenSymbian *s = GDK_SCREEN_SYMBIAN (screen);
  GdkVisual *v = g_object_new (GDK_TYPE_VISUAL, NULL);
  v->type = GDK_VISUAL_TRUE_COLOR; v->depth = 24; v->byte_order = GDK_LSB_FIRST;   /* x86: 0x00RRGGBB is stored B G R 0 */
  v->colormap_size = 256; v->bits_per_rgb = 8;
  v->red_mask = 0x00ff0000; v->red_shift = 16; v->red_prec = 8;
  v->green_mask = 0x0000ff00; v->green_shift = 8; v->green_prec = 8;
  v->blue_mask = 0x000000ff; v->blue_shift = 0; v->blue_prec = 8;
  s->visual = v; _gdk_visual = v;
  s->colormap = gdk_colormap_new (v, FALSE);
}

GdkVisual *_gdk_visual = NULL;

GdkScreen *gdk_visual_get_screen (GdkVisual *visual) { g_return_val_if_fail (GDK_IS_VISUAL (visual), NULL); return _gdk_screen; }
GdkVisual *gdk_visual_get_best (void) { return _gdk_visual; }
gint gdk_visual_get_best_depth (void) { return 24; }
GdkVisualType gdk_visual_get_best_type (void) { return GDK_VISUAL_TRUE_COLOR; }
GdkVisual *gdk_visual_get_best_with_depth (gint depth) { return depth == 24 ? _gdk_visual : NULL; }
GdkVisual *gdk_visual_get_best_with_type (GdkVisualType visual_type) { return visual_type == GDK_VISUAL_TRUE_COLOR ? _gdk_visual : NULL; }
GdkVisual *gdk_visual_get_best_with_both (gint depth, GdkVisualType visual_type) { return depth == 24 && visual_type == GDK_VISUAL_TRUE_COLOR ? _gdk_visual : NULL; }
void gdk_query_depths (gint **depths, gint *count) { static gint d[1] = { 24 }; *count = 1; *depths = d; }
void gdk_query_visual_types (GdkVisualType **visual_types, gint *count) { static GdkVisualType t[1] = { GDK_VISUAL_TRUE_COLOR }; *count = 1; *visual_types = t; }

/* ---- GdkColormap ---- */
G_DEFINE_TYPE (GdkColormap, gdk_colormap, G_TYPE_OBJECT)
static void gdk_colormap_init (GdkColormap *c) { c->size = 0; c->colors = NULL; c->visual = NULL; c->windowing_data = NULL; }
static void gdk_colormap_finalize (GObject *object)
{
  GdkColormap *c = GDK_COLORMAP (object);
  g_free (c->colors);
  if (c->visual) g_object_unref (c->visual);
  G_OBJECT_CLASS (gdk_colormap_parent_class)->finalize (object);
}
static void gdk_colormap_class_init (GdkColormapClass *c) { G_OBJECT_CLASS (c)->finalize = gdk_colormap_finalize; }

GdkColormap *
gdk_colormap_new (GdkVisual *visual, gboolean private_cmap)
{
  GdkColormap *c;
  g_return_val_if_fail (GDK_IS_VISUAL (visual), NULL);
  (void) private_cmap;
  c = g_object_new (GDK_TYPE_COLORMAP, NULL);
  c->visual = g_object_ref (visual);
  c->size = visual->colormap_size;
  c->colors = g_new0 (GdkColor, c->size);
  return c;
}

GdkScreen *gdk_colormap_get_screen (GdkColormap *cmap) { g_return_val_if_fail (GDK_IS_COLORMAP (cmap), NULL); return _gdk_screen; }

static guint32
pixel_of (const GdkVisual *v, const GdkColor *c)
{
  return (((guint32) c->red   >> (16 - v->red_prec))   << v->red_shift)   |
         (((guint32) c->green >> (16 - v->green_prec)) << v->green_shift) |
         (((guint32) c->blue  >> (16 - v->blue_prec))  << v->blue_shift);
}

gint
gdk_colormap_alloc_colors (GdkColormap *colormap, GdkColor *colors, gint n_colors, gboolean writeable, gboolean best_match, gboolean *success)
{
  gint i;
  g_return_val_if_fail (GDK_IS_COLORMAP (colormap), n_colors);
  g_return_val_if_fail (colors != NULL, n_colors);
  g_return_val_if_fail (success != NULL, n_colors);
  (void) best_match;
  for (i = 0; i < n_colors; i++) {
    if (writeable) { success[i] = FALSE; continue; }              /* a TrueColor map has no writable cells */
    colors[i].pixel = pixel_of (colormap->visual, &colors[i]); success[i] = TRUE;
  }
  for (i = 0; i < n_colors; i++) if (!success[i]) return n_colors - i;
  return 0;
}

void gdk_colormap_free_colors (GdkColormap *colormap, const GdkColor *colors, gint n_colors) { (void) colormap; (void) colors; (void) n_colors; }   /* nothing was allocated */

void
gdk_colormap_query_color (GdkColormap *colormap, gulong pixel, GdkColor *result)
{
  const GdkVisual *v;
  g_return_if_fail (GDK_IS_COLORMAP (colormap)); g_return_if_fail (result != NULL);
  v = colormap->visual; result->pixel = pixel;
  result->red   = (guint16) ((((pixel & v->red_mask)   >> v->red_shift)   * 65535) / ((1u << v->red_prec)   - 1));
  result->green = (guint16) ((((pixel & v->green_mask) >> v->green_shift) * 65535) / ((1u << v->green_prec) - 1));
  result->blue  = (guint16) ((((pixel & v->blue_mask)  >> v->blue_shift)  * 65535) / ((1u << v->blue_prec)  - 1));
}

/* change entries of a writable map: there are none */
void gdk_colormap_change (GdkColormap *colormap, gint ncolors) { g_return_if_fail (GDK_IS_COLORMAP (colormap)); (void) ncolors; }
