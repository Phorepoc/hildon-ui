/* gdkgc-symbian.c -- GdkGC of the Symbian backend. A GC is a bag of drawing attributes (colours, raster function, fill, clip, line style); there is no server object behind it, so it
 * keeps the attributes itself and _gdk_symbian_gc_apply() turns them into the state of a cairo context for the drawing primitives of gdkdrawable-symbian.c. BSD-2-Clause (Phorepoc). */
#include "config.h"
#include <string.h>
#include "gdksymbian.h"
#include "gdkgc.h"
#include "gdkprivate.h"
#include "gdkinternals.h"
#include "gdkpixmap.h"
#include "gdkregion.h"
#include "gdkraster.h"

static const cairo_user_data_key_t coverage_key;
G_STATIC_ASSERT (GDK_COPY == PHE_FN_COPY && GDK_INVERT == PHE_FN_INVERT && GDK_XOR == PHE_FN_XOR && GDK_CLEAR == PHE_FN_CLEAR && GDK_AND == PHE_FN_AND &&
                 GDK_AND_REVERSE == PHE_FN_AND_REVERSE && GDK_AND_INVERT == PHE_FN_AND_INVERT && GDK_NOOP == PHE_FN_NOOP && GDK_OR == PHE_FN_OR && GDK_EQUIV == PHE_FN_EQUIV &&
                 GDK_OR_REVERSE == PHE_FN_OR_REVERSE && GDK_COPY_INVERT == PHE_FN_COPY_INVERT && GDK_OR_INVERT == PHE_FN_OR_INVERT && GDK_NAND == PHE_FN_NAND &&
                 GDK_NOR == PHE_FN_NOR && GDK_SET == PHE_FN_SET);
G_STATIC_ASSERT (GDK_SOLID == PHE_FILL_SOLID && GDK_TILED == PHE_FILL_TILED && GDK_STIPPLED == PHE_FILL_STIPPLED && GDK_OPAQUE_STIPPLED == PHE_FILL_OPAQUE_STIPPLED);
typedef struct _GdkGCSymbian {
  GdkGC parent_instance;
  GdkColor fg, bg;
  GdkFunction function; GdkFill fill;
  GdkPixmap *tile, *stipple, *clip_mask;
  GdkSubwindowMode subwindow_mode; gint graphics_exposures;
  gint line_width; GdkLineStyle line_style; GdkCapStyle cap_style; GdkJoinStyle join_style;
  GdkRegion *clip_region;
  gint8 *dashes; gint n_dashes, dash_offset;
  gint depth;
} GdkGCSymbian;
typedef struct { GdkGCClass parent_class; } GdkGCSymbianClass;
#define GDK_GC_SYMBIAN(o) ((GdkGCSymbian *) (o))

static void apply_coverage (struct _GdkGCSymbian *g, GdkGC *gc, cairo_t *cr);
G_DEFINE_TYPE (GdkGCSymbian, gdk_gc_symbian, GDK_TYPE_GC)

static void
gdk_gc_symbian_init (GdkGCSymbian *g)
{
  g->function = GDK_COPY; g->fill = GDK_SOLID; g->graphics_exposures = TRUE; g->line_width = 0;
  g->line_style = GDK_LINE_SOLID; g->cap_style = GDK_CAP_BUTT; g->join_style = GDK_JOIN_MITER;
  g->fg.pixel = 0; g->bg.pixel = 0xffffff; g->fg.red = g->fg.green = g->fg.blue = 0; g->bg.red = g->bg.green = g->bg.blue = 0xffff;
}

static void
gdk_gc_symbian_finalize (GObject *object)
{
  GdkGCSymbian *g = GDK_GC_SYMBIAN (object);
  if (g->tile) g_object_unref (g->tile);
  if (g->stipple) g_object_unref (g->stipple);
  if (g->clip_mask) g_object_unref (g->clip_mask);
  if (g->clip_region) gdk_region_destroy (g->clip_region);
  g_free (g->dashes);
  if (GDK_GC (g)->colormap) g_object_unref (GDK_GC (g)->colormap);
  G_OBJECT_CLASS (gdk_gc_symbian_parent_class)->finalize (object);
}

static void
gdk_gc_symbian_get_values (GdkGC *gc, GdkGCValues *values)
{
  GdkGCSymbian *g = GDK_GC_SYMBIAN (gc);
  values->foreground = g->fg; values->background = g->bg; values->font = NULL;
  values->function = g->function; values->fill = g->fill;
  values->tile = g->tile; values->stipple = g->stipple; values->clip_mask = g->clip_mask;
  values->subwindow_mode = g->subwindow_mode;
  values->ts_x_origin = gc->ts_x_origin; values->ts_y_origin = gc->ts_y_origin;
  values->clip_x_origin = gc->clip_x_origin; values->clip_y_origin = gc->clip_y_origin;
  values->graphics_exposures = g->graphics_exposures;
  values->line_width = g->line_width; values->line_style = g->line_style; values->cap_style = g->cap_style; values->join_style = g->join_style;
}

#define REPLACE_OBJ(field, val) do { GdkPixmap *n_ = (val); if (n_) g_object_ref (n_); if (g->field) g_object_unref (g->field); g->field = n_; } while (0)

static void
gdk_gc_symbian_set_values (GdkGC *gc, GdkGCValues *values, GdkGCValuesMask mask)
{
  GdkGCSymbian *g = GDK_GC_SYMBIAN (gc);
  if (mask & GDK_GC_FOREGROUND) g->fg = values->foreground;
  if (mask & GDK_GC_BACKGROUND) g->bg = values->background;
  if (mask & GDK_GC_FUNCTION) g->function = values->function;
  if (mask & GDK_GC_FILL) g->fill = values->fill;
  if (mask & GDK_GC_TILE) REPLACE_OBJ (tile, values->tile);
  if (mask & GDK_GC_STIPPLE) REPLACE_OBJ (stipple, values->stipple);
  if (mask & GDK_GC_CLIP_MASK) { REPLACE_OBJ (clip_mask, values->clip_mask); if (values->clip_mask && g->clip_region) { gdk_region_destroy (g->clip_region); g->clip_region = NULL; } }
  if (mask & GDK_GC_SUBWINDOW) g->subwindow_mode = values->subwindow_mode;
  if (mask & GDK_GC_TS_X_ORIGIN) gc->ts_x_origin = values->ts_x_origin;
  if (mask & GDK_GC_TS_Y_ORIGIN) gc->ts_y_origin = values->ts_y_origin;
  if (mask & GDK_GC_CLIP_X_ORIGIN) gc->clip_x_origin = values->clip_x_origin;
  if (mask & GDK_GC_CLIP_Y_ORIGIN) gc->clip_y_origin = values->clip_y_origin;
  if (mask & GDK_GC_EXPOSURES) g->graphics_exposures = values->graphics_exposures;
  if (mask & GDK_GC_LINE_WIDTH) g->line_width = values->line_width;
  if (mask & GDK_GC_LINE_STYLE) g->line_style = values->line_style;
  if (mask & GDK_GC_CAP_STYLE) g->cap_style = values->cap_style;
  if (mask & GDK_GC_JOIN_STYLE) g->join_style = values->join_style;
}

static void
gdk_gc_symbian_set_dashes (GdkGC *gc, gint dash_offset, gint8 dash_list[], gint n)
{
  GdkGCSymbian *g = GDK_GC_SYMBIAN (gc);
  g_free (g->dashes); g->dashes = g_memdup (dash_list, n); g->n_dashes = n; g->dash_offset = dash_offset;
}

static void
gdk_gc_symbian_class_init (GdkGCSymbianClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass); GdkGCClass *gc_class = GDK_GC_CLASS (klass);
  object_class->finalize = gdk_gc_symbian_finalize;
  gc_class->get_values = gdk_gc_symbian_get_values;
  gc_class->set_values = gdk_gc_symbian_set_values;
  gc_class->set_dashes = gdk_gc_symbian_set_dashes;
}

/* created by the drawable (create_gc vfunc): a GC bound to the drawable's depth and colormap */
GdkGC *
_gdk_symbian_gc_new (GdkDrawable *drawable, GdkGCValues *values, GdkGCValuesMask mask, gint depth)
{
  GdkGC *gc = g_object_new (GDK_TYPE_GC_SYMBIAN, NULL);
  GDK_GC_SYMBIAN (gc)->depth = depth;
  _gdk_gc_init (gc, drawable, values, mask);
  return gc;
}

void
_gdk_windowing_gc_set_clip_region (GdkGC *gc, const GdkRegion *region, gboolean reset_origin)
{
  GdkGCSymbian *g = GDK_GC_SYMBIAN (gc);
  if (g->clip_region) { gdk_region_destroy (g->clip_region); g->clip_region = NULL; }
  if (g->clip_mask) { g_object_unref (g->clip_mask); g->clip_mask = NULL; }
  if (region) g->clip_region = gdk_region_copy (region);
  if (reset_origin) gc->clip_x_origin = gc->clip_y_origin = 0;
}

void
_gdk_windowing_gc_copy (GdkGC *dst_gc, GdkGC *src_gc)
{
  GdkGCSymbian *d = GDK_GC_SYMBIAN (dst_gc), *s = GDK_GC_SYMBIAN (src_gc);
  d->fg = s->fg; d->bg = s->bg; d->function = s->function; d->fill = s->fill;
  { GdkPixmap *n;
    n = s->tile; if (n) g_object_ref (n); if (d->tile) g_object_unref (d->tile); d->tile = n;
    n = s->stipple; if (n) g_object_ref (n); if (d->stipple) g_object_unref (d->stipple); d->stipple = n;
    n = s->clip_mask; if (n) g_object_ref (n); if (d->clip_mask) g_object_unref (d->clip_mask); d->clip_mask = n; }
  d->subwindow_mode = s->subwindow_mode; d->graphics_exposures = s->graphics_exposures;
  d->line_width = s->line_width; d->line_style = s->line_style; d->cap_style = s->cap_style; d->join_style = s->join_style;
  if (d->clip_region) gdk_region_destroy (d->clip_region);
  d->clip_region = s->clip_region ? gdk_region_copy (s->clip_region) : NULL;
  g_free (d->dashes); d->dashes = s->dashes ? g_memdup (s->dashes, s->n_dashes) : NULL; d->n_dashes = s->n_dashes; d->dash_offset = s->dash_offset;
  d->depth = s->depth;
  dst_gc->clip_x_origin = src_gc->clip_x_origin; dst_gc->clip_y_origin = src_gc->clip_y_origin;
  dst_gc->ts_x_origin = src_gc->ts_x_origin; dst_gc->ts_y_origin = src_gc->ts_y_origin;
}

static void
color_of (GdkGCSymbian *g, const GdkColor *c, double *r, double *gg, double *b, double *a)
{
  GdkColor rgb = *c;
  if (GDK_GC (g)->colormap && g->depth > 1) gdk_colormap_query_color (GDK_GC (g)->colormap, c->pixel, &rgb);
  if (g->depth == 1) { *r = *gg = *b = 0; *a = (c->pixel & 1) ? 1.0 : 0.0; return; }   /* a bitmap is an A8 surface: pixel 1 = opaque */
  *r = rgb.red / 65535.0; *gg = rgb.green / 65535.0; *b = rgb.blue / 65535.0; *a = 1.0;
}

/* ---- raster operations ----
 * cairo has no bitwise operators and X semantics of GDK_XOR / AND / ... are defined on pixel values. When the GC asks for one (or for OPAQUE_STIPPLED, which paints two colours),
 * a primitive does not draw into the drawable: it draws its shape, with the GC's line attributes and clip, into an A8 coverage surface without antialiasing, and
 * _gdk_symbian_raster_combine() then goes over the covered pixels and writes  dest = function (source, dest)  on the image data. The source pixel is the foreground
 * (SOLID), the tile pixel (TILED), the foreground or the background by the stipple bit (OPAQUE_STIPPLED) or the foreground where the stipple bit is set (STIPPLED). */
const cairo_user_data_key_t *_gdk_symbian_coverage_key (void) { return &coverage_key; }

gboolean
_gdk_symbian_gc_raster_mode (GdkGC *gc)
{
  GdkGCSymbian *g = GDK_GC_SYMBIAN (gc);
  return g->function != GDK_COPY || g->fill == GDK_OPAQUE_STIPPLED;
}
GdkFunction _gdk_symbian_gc_function (GdkGC *gc) { return GDK_GC_SYMBIAN (gc)->function; }
void _gdk_symbian_gc_pixels (GdkGC *gc, guint32 *fg, guint32 *bg) { *fg = GDK_GC_SYMBIAN (gc)->fg.pixel; *bg = GDK_GC_SYMBIAN (gc)->bg.pixel; }
/* may (x, y) of the drawable be drawn to, with respect to the clip region and clip mask of the GC? */
gboolean
_gdk_symbian_gc_clip_test (GdkGC *gc, gint x, gint y)
{
  GdkGCSymbian *g = GDK_GC_SYMBIAN (gc);
  if (g->clip_region && !gdk_region_point_in (g->clip_region, x - gc->clip_x_origin, y - gc->clip_y_origin)) return FALSE;
  if (g->clip_mask) {
    cairo_surface_t *ms = _gdk_drawable_ref_cairo_surface (g->clip_mask); gboolean ok = FALSE;
    if (ms) {
      gint mx = x - gc->clip_x_origin, my = y - gc->clip_y_origin;
      cairo_surface_flush (ms);
      if (mx >= 0 && my >= 0 && mx < cairo_image_surface_get_width (ms) && my < cairo_image_surface_get_height (ms))
        ok = cairo_image_surface_get_data (ms)[my * cairo_image_surface_get_stride (ms) + mx] != 0;
      cairo_surface_destroy (ms);
    }
    return ok;
  }
  return TRUE;
}
gboolean _gdk_symbian_gc_is_noop (GdkGC *gc) { return GDK_GC_SYMBIAN (gc)->function == GDK_NOOP; }

static void
apply_coverage (GdkGCSymbian *g, GdkGC *gc, cairo_t *cr)
{
  if (g->clip_region) {
    gint i, n; GdkRectangle *rects;
    gdk_region_get_rectangles (g->clip_region, &rects, &n);
    cairo_new_path (cr);
    for (i = 0; i < n; i++) cairo_rectangle (cr, rects[i].x + gc->clip_x_origin, rects[i].y + gc->clip_y_origin, rects[i].width, rects[i].height);
    cairo_clip (cr); g_free (rects);
  }
  cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER); cairo_set_source_rgba (cr, 1, 1, 1, 1);
  cairo_set_line_width (cr, g->line_width > 0 ? g->line_width : 1);
  switch (g->cap_style) { case GDK_CAP_ROUND: cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND); break; case GDK_CAP_PROJECTING: cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE); break; default: cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT); }
  switch (g->join_style) { case GDK_JOIN_ROUND: cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND); break; case GDK_JOIN_BEVEL: cairo_set_line_join (cr, CAIRO_LINE_JOIN_BEVEL); break; default: cairo_set_line_join (cr, CAIRO_LINE_JOIN_MITER); }
  if (g->line_style != GDK_LINE_SOLID && g->dashes && g->n_dashes > 0) {
    double d[64]; gint i, n = MIN (g->n_dashes, 64); for (i = 0; i < n; i++) d[i] = g->dashes[i];
    cairo_set_dash (cr, d, n, g->dash_offset);
  }
}

guint32 _gdk_symbian_apply_function (GdkFunction f, guint32 s, guint32 d) { return phe_raster_function (f, s, d); }

/* @dst: pixel (0,0) of the drawable inside its surface data; the loop covers x in [x0,x1), y in [y0,y1) in drawable coordinates; @cov has the drawable's size */
void
_gdk_symbian_raster_combine (GdkGC *gc, guchar *dst, gint dst_stride, gint depth, gint x0, gint y0, gint x1, gint y1, cairo_surface_t *cov)
{
  GdkGCSymbian *g = GDK_GC_SYMBIAN (gc); PheRasterState st; cairo_surface_t *ts = NULL, *ss = NULL, *ms = NULL;
  memset (&st, 0, sizeof st);
  st.function = g->function; st.depth = depth; st.fg = g->fg.pixel; st.bg = g->bg.pixel; st.fill = g->fill;
  st.ts_x = gc->ts_x_origin; st.ts_y = gc->ts_y_origin; st.clip_x = gc->clip_x_origin; st.clip_y = gc->clip_y_origin;
#define OPEN_PLANE(field, has, pixmap) do { cairo_surface_t *s_ = (pixmap) ? _gdk_drawable_ref_cairo_surface (pixmap) : NULL; \
    if (s_) { cairo_surface_flush (s_); st.field.data = cairo_image_surface_get_data (s_); st.field.stride = cairo_image_surface_get_stride (s_); \
              st.field.w = cairo_image_surface_get_width (s_); st.field.h = cairo_image_surface_get_height (s_); st.has = st.field.data != NULL; } \
    field##_s = s_; } while (0)
  if (g->fill == GDK_TILED) { cairo_surface_t *tile_s; OPEN_PLANE (tile, has_tile, g->tile); ts = tile_s; }
  if (g->fill == GDK_STIPPLED || g->fill == GDK_OPAQUE_STIPPLED) { cairo_surface_t *stipple_s; OPEN_PLANE (stipple, has_stipple, g->stipple); ss = stipple_s; }
  if (g->clip_mask) { cairo_surface_t *mask_s; OPEN_PLANE (mask, has_mask, g->clip_mask); ms = mask_s; }
#undef OPEN_PLANE
  cairo_surface_flush (cov);
  phe_raster_combine (&st, dst, dst_stride, x0, y0, x1, y1, cairo_image_surface_get_data (cov), cairo_image_surface_get_stride (cov));
  if (ts) cairo_surface_destroy (ts);
  if (ss) cairo_surface_destroy (ss);
  if (ms) cairo_surface_destroy (ms);
}

void
_gdk_windowing_gc_get_foreground (GdkGC *gc, GdkColor *color) { *color = GDK_GC_SYMBIAN (gc)->fg; }
void
_gdk_windowing_gc_get_background (GdkGC *gc, GdkColor *color) { *color = GDK_GC_SYMBIAN (gc)->bg; }

/* Sets the state of @cr from the GC. `for_fill` selects the fill source (tile / stipple / solid); line drawing always uses the solid foreground.
 * A clip mask needs the drawing to be composed first: the caller then ends with _gdk_symbian_gc_finish(). */
void
_gdk_symbian_gc_apply (GdkGC *gc, cairo_t *cr, gboolean for_fill)
{
  GdkGCSymbian *g = GDK_GC_SYMBIAN (gc); double r, gg, b, a;
  if (cairo_get_user_data (cr, &coverage_key)) { apply_coverage (g, gc, cr); return; }       /* raster-operation mode: the primitive only produces a coverage mask, see _gdk_symbian_raster_combine */
  if (g->clip_region) {
    gint i, n; GdkRectangle *rects;
    gdk_region_get_rectangles (g->clip_region, &rects, &n);
    cairo_new_path (cr);
    for (i = 0; i < n; i++) cairo_rectangle (cr, rects[i].x + gc->clip_x_origin, rects[i].y + gc->clip_y_origin, rects[i].width, rects[i].height);
    cairo_clip (cr); g_free (rects);
  }
  if (g->clip_mask || (for_fill && g->fill == GDK_STIPPLED && g->stipple)) cairo_push_group (cr);
  color_of (g, &g->fg, &r, &gg, &b, &a);
  switch (g->function) {
    default: cairo_set_operator (cr, g->depth == 1 ? CAIRO_OPERATOR_SOURCE : CAIRO_OPERATOR_OVER); cairo_set_source_rgba (cr, r, gg, b, a); break;
  }
  if (for_fill && g->function == GDK_COPY && g->fill == GDK_TILED && g->tile) {
    cairo_surface_t *ts = _gdk_drawable_ref_cairo_surface (g->tile);
    if (ts) { cairo_pattern_t *p = cairo_pattern_create_for_surface (ts); cairo_matrix_t m;
      cairo_pattern_set_extend (p, CAIRO_EXTEND_REPEAT); cairo_matrix_init_translate (&m, -gc->ts_x_origin, -gc->ts_y_origin); cairo_pattern_set_matrix (p, &m);
      cairo_set_source (cr, p); cairo_pattern_destroy (p); cairo_surface_destroy (ts); }
  }
  cairo_set_line_width (cr, g->line_width > 0 ? g->line_width : 1);
  switch (g->cap_style) { case GDK_CAP_ROUND: cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND); break; case GDK_CAP_PROJECTING: cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE); break; default: cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT); }
  switch (g->join_style) { case GDK_JOIN_ROUND: cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND); break; case GDK_JOIN_BEVEL: cairo_set_line_join (cr, CAIRO_LINE_JOIN_BEVEL); break; default: cairo_set_line_join (cr, CAIRO_LINE_JOIN_MITER); }
  if (g->line_style != GDK_LINE_SOLID && g->dashes && g->n_dashes > 0) {
    double d[64]; gint i, n = MIN (g->n_dashes, 64); for (i = 0; i < n; i++) d[i] = g->dashes[i];
    cairo_set_dash (cr, d, n, g->dash_offset);
  }
}

/* A stipple is a repeating A8 pattern the shape is masked with; a clip mask is an A8 bitmap at the clip origin. */
void
_gdk_symbian_gc_finish (GdkGC *gc, cairo_t *cr)
{
  GdkGCSymbian *g = GDK_GC_SYMBIAN (gc);
  gboolean stip = g->fill == GDK_STIPPLED && g->stipple && g->function == GDK_COPY;
  if (cairo_get_user_data (cr, &coverage_key)) return;
  if (!g->clip_mask && !stip) return;
  cairo_pop_group_to_source (cr);
  if (stip) {
    cairo_surface_t *ss = _gdk_drawable_ref_cairo_surface (g->stipple);
    if (ss) { cairo_pattern_t *p = cairo_pattern_create_for_surface (ss); cairo_matrix_t m;
      cairo_pattern_set_extend (p, CAIRO_EXTEND_REPEAT); cairo_matrix_init_translate (&m, -gc->ts_x_origin, -gc->ts_y_origin); cairo_pattern_set_matrix (p, &m);
      if (g->clip_mask) { cairo_push_group (cr); cairo_mask (cr, p); cairo_pop_group_to_source (cr); } else cairo_mask (cr, p);
      cairo_pattern_destroy (p); cairo_surface_destroy (ss); }
    if (!g->clip_mask) return;
  }
  if (g->clip_mask) {
    cairo_surface_t *ms = _gdk_drawable_ref_cairo_surface (g->clip_mask);
    if (ms) { cairo_mask_surface (cr, ms, gc->clip_x_origin, gc->clip_y_origin); cairo_surface_destroy (ms); }
    else cairo_paint (cr);
  }
}
