/* gdkdrawable-symbian.c -- the drawable implementation of the Symbian backend: every drawable is a cairo surface; each GdkDrawable drawing primitive is a cairo operation with the state of
 * the GC (gdkgc-symbian.c). A window drawable is a rectangle of the display's bitmap surface; drawing into one records the rectangle as dirty and a flush (after the update cycle, or an idle
 * for drawing outside it) hands the rectangles to the WSERV thread (phe_ws_present). A pixmap owns its surface. BSD-2-Clause (Phorepoc). */
#include "config.h"
#include <math.h>
#include <string.h>
#include <pango/pangocairo.h>
#include "gdksymbian.h"
#include "gdkdrawable.h"
#include "gdkpixmap.h"
#include "gdkimage.h"
#include "gdkprivate.h"
#include "gdkinternals.h"
#include "gdkregion.h"
#include "gdkpixbuf.h"
#include "gdkraster.h"
#include "gdkfont.h"

GdkGC *_gdk_symbian_gc_new (GdkDrawable *drawable, GdkGCValues *values, GdkGCValuesMask mask, gint depth);

G_DEFINE_TYPE (GdkDrawableImplSymbian, gdk_drawable_impl_symbian, GDK_TYPE_DRAWABLE)

static void
gdk_drawable_impl_symbian_init (GdkDrawableImplSymbian *impl)
{
  impl->width = 1; impl->height = 1; impl->depth = 24; impl->view_key[0] = impl->view_key[1] = impl->view_key[2] = impl->view_key[3] = -1;
}

static void
gdk_drawable_impl_symbian_finalize (GObject *object)
{
  GdkDrawableImplSymbian *impl = GDK_DRAWABLE_IMPL_SYMBIAN (object);
  if (impl->view) cairo_surface_destroy (impl->view);
  if (impl->surface) cairo_surface_destroy (impl->surface);
  if (impl->colormap) g_object_unref (impl->colormap);
  G_OBJECT_CLASS (gdk_drawable_impl_symbian_parent_class)->finalize (object);
}

/* ---- surfaces ---- */
/* Every request returns a NEW cairo surface object over the same pixels. GDK sets a device offset on the surface it is given (begin_paint, the cached surface of each client side child window), and
 * those offsets must not leak between the users of one drawable: with one shared surface object the last one to set an offset moved every other user's drawing. */
static cairo_user_data_key_t wrapped_key;
static cairo_surface_t *
impl_surface (GdkDrawableImplSymbian *impl)
{
  if (!impl->surface) return NULL;
  if (!GDK_IS_WINDOW (impl->wrapper)) {                                        /* a pixmap: its pixel memory, kept alive by the wrapper */
    cairo_surface_t *w = cairo_image_surface_create_for_data (cairo_image_surface_get_data (impl->surface), cairo_image_surface_get_format (impl->surface),
                                                              cairo_image_surface_get_width (impl->surface), cairo_image_surface_get_height (impl->surface), cairo_image_surface_get_stride (impl->surface));
    if (cairo_surface_status (w) != CAIRO_STATUS_SUCCESS) return w;
    cairo_surface_set_user_data (w, &wrapped_key, cairo_surface_reference (impl->surface), (cairo_destroy_func_t) cairo_surface_destroy);
    return w;
  }
  /* a window: the rectangle of the shared bitmap surface */
  return cairo_surface_create_for_rectangle (impl->surface, impl->abs_x, impl->abs_y, impl->width, impl->height);
}

static cairo_surface_t *gdk_drawable_impl_symbian_ref_cairo_surface (GdkDrawable *d) { return impl_surface (GDK_DRAWABLE_IMPL_SYMBIAN (d)); }

/* A surface "for" a drawable: for a window the surface IS the window (the rectangle of the display bitmap: this is what gdk_cairo_create draws on when no paint is open), for a pixmap
 * a new surface of the same kind (cairo_surface_create_similar semantics, as the X11 backend gives for pixmaps). */
static cairo_surface_t *
gdk_drawable_impl_symbian_create_cairo_surface (GdkDrawable *d, int width, int height)
{
  GdkDrawableImplSymbian *impl = GDK_DRAWABLE_IMPL_SYMBIAN (d); cairo_surface_t *base = impl_surface (impl), *s;
  if (!base) return NULL;
  if (GDK_IS_WINDOW (impl->wrapper)) return base;
  s = cairo_surface_create_similar (base, impl->depth == 1 ? CAIRO_CONTENT_ALPHA : CAIRO_CONTENT_COLOR, width, height);
  cairo_surface_destroy (base); return s;
}

static void gdk_drawable_impl_symbian_set_cairo_clip (GdkDrawable *d, cairo_t *cr) { (void) d; cairo_new_path (cr); }       /* the clip of a window is applied by the GDK core (client-side clipping) */
static GdkDrawable *gdk_drawable_impl_symbian_get_source_drawable (GdkDrawable *d) { return GDK_DRAWABLE_IMPL_SYMBIAN (d)->wrapper; }

/* ---- dirty rectangles of windows ---- */
static void window_mark_dirty (GdkDrawableImplSymbian *impl, gint x, gint y, gint w, gint h);

#define IMPL(d) GDK_DRAWABLE_IMPL_SYMBIAN (d)

/* ---- simple attributes ---- */
static gint gdk_drawable_impl_symbian_get_depth (GdkDrawable *d) { return IMPL (d)->depth; }
static void gdk_drawable_impl_symbian_get_size (GdkDrawable *d, gint *w, gint *h) { if (w) *w = IMPL (d)->width; if (h) *h = IMPL (d)->height; }
static void
gdk_drawable_impl_symbian_set_colormap (GdkDrawable *d, GdkColormap *cmap)
{
  GdkDrawableImplSymbian *impl = IMPL (d);
  if (impl->colormap == cmap) return;
  if (cmap) g_object_ref (cmap);
  if (impl->colormap) g_object_unref (impl->colormap);
  impl->colormap = cmap;
}
/* a window without a colormap of its own (an input-only window, or one whose colormap was cleared) has the one it inherits from its parent, as the window server would report; a depth 1 pixmap has none */
static GdkColormap *
gdk_drawable_impl_symbian_get_colormap (GdkDrawable *d)
{
  GdkDrawableImplSymbian *impl = IMPL (d); GdkDrawable *w = impl->wrapper;
  if (impl->colormap || impl->depth == 1) return impl->colormap;
  while (w && GDK_IS_WINDOW (w)) {
    GdkWindowObject *p = GDK_WINDOW_OBJECT (w)->parent ? GDK_WINDOW_OBJECT (GDK_WINDOW_OBJECT (w)->parent) : NULL;
    GdkColormap *c;
    if (!p) break;
    c = ((GdkDrawableImplSymbian *) p->impl)->colormap;
    if (c) return c;
    w = GDK_DRAWABLE (p);
  }
  return impl->wrapper && GDK_IS_WINDOW (impl->wrapper) ? gdk_screen_get_system_colormap (_gdk_screen) : NULL;
}
static GdkVisual *gdk_drawable_impl_symbian_get_visual (GdkDrawable *d) { return IMPL (d)->depth == 24 ? _gdk_visual : NULL; }
static GdkScreen *gdk_drawable_impl_symbian_get_screen (GdkDrawable *d) { (void) d; return _gdk_screen; }

static GdkGC *
gdk_drawable_impl_symbian_create_gc (GdkDrawable *d, GdkGCValues *values, GdkGCValuesMask mask)
{
  return _gdk_symbian_gc_new (d, values, mask, IMPL (d)->depth);
}

/* ---- drawing: one cairo context per primitive ----
 * Normally the context draws into the drawable. When the GC needs a raster operation (gdkgc-symbian.c) the context draws a coverage mask instead and end() combines it with the pixels. */
/* The native windows share one bitmap, so a window must not draw over the part of itself that a window stacked above it covers (a window server clips by stacking order itself). The region covered
 * by the mapped toplevels above this window, in this window's coordinates; NULL when nothing covers it (and for anything that is not a native window). */
static GdkRegion *
occluders (GdkDrawableImplSymbian *impl)
{
  GList *l; GdkRegion *r = NULL; gboolean above = FALSE; GdkRectangle self;
  if (!impl->wrapper || !GDK_IS_WINDOW (impl->wrapper) || GDK_WINDOW_OBJECT (impl->wrapper)->window_type == GDK_WINDOW_ROOT) return NULL;
  self.x = impl->abs_x; self.y = impl->abs_y; self.width = impl->width; self.height = impl->height;
  for (l = _gdk_symbian_toplevel_stack (); l; l = l->next) {
    GdkWindow *w = l->data; GdkWindowImplSymbian *wi = (GdkWindowImplSymbian *) GDK_WINDOW_OBJECT (w)->impl; GdkRectangle b, i;
    if ((GdkDrawable *) wi == (GdkDrawable *) impl) { above = TRUE; continue; }
    if (!above || !wi->mapped) continue;
    b.x = wi->drawable.abs_x; b.y = wi->drawable.abs_y; b.width = wi->drawable.width; b.height = wi->drawable.height;
    if (!gdk_rectangle_intersect (&self, &b, &i)) continue;
    i.x -= self.x; i.y -= self.y;
    if (!r) r = gdk_region_new ();
    gdk_region_union_with_rect (r, &i);
  }
  return r;
}

static GdkRegion *copy_occ = NULL;                                  /* the occluders of the copy in progress (copy_pixels) */

/* clip the context to what no window above covers */
static void
clip_occluders (GdkDrawableImplSymbian *impl, cairo_t *cr)
{
  GdkRegion *r = occluders (impl); GdkRectangle *rects; gint n, k;
  if (!r) return;
  gdk_region_get_rectangles (r, &rects, &n);
  cairo_new_path (cr);
  cairo_rectangle (cr, 0, 0, impl->width, impl->height);
  for (k = 0; k < n; k++) cairo_rectangle (cr, rects[k].x, rects[k].y, rects[k].width, rects[k].height);
  cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD); cairo_clip (cr);                    /* (the clip stays in the state of the context: no save / restore around it) */
  cairo_set_fill_rule (cr, CAIRO_FILL_RULE_WINDING); cairo_new_path (cr);
  g_free (rects); gdk_region_destroy (r);
}

static cairo_t *
begin (GdkDrawableImplSymbian *impl, GdkGC *gc)
{
  cairo_surface_t *s; cairo_t *cr;
  if (gc && _gdk_symbian_gc_is_noop (gc)) return NULL;                 /* GDK_NOOP draws nothing */
  if (gc && _gdk_symbian_gc_raster_mode (gc)) {
    cairo_surface_t *cov = cairo_image_surface_create (CAIRO_FORMAT_A8, impl->width, impl->height);
    cr = cairo_create (cov); cairo_surface_destroy (cov);
    cairo_set_user_data (cr, _gdk_symbian_coverage_key (), cairo_surface_reference (cov), (cairo_destroy_func_t) cairo_surface_destroy);
    clip_occluders (impl, cr);
    return cr;
  }
  s = impl_surface (impl);
  if (!s) return NULL;
  cr = cairo_create (s); cairo_surface_destroy (s); clip_occluders (impl, cr); return cr;
}

/* the data of the drawable's origin inside its surface (a window is a rectangle of the display bitmap) */
static gboolean
raster_target (GdkDrawableImplSymbian *impl, guchar **dst, gint *stride, gint *x0, gint *y0, gint *x1, gint *y1)
{
  guchar *data; gint sw, sh, bpp = impl->depth == 1 ? 1 : 4;
  if (!impl->surface) return FALSE;
  cairo_surface_flush (impl->surface);
  data = cairo_image_surface_get_data (impl->surface); *stride = cairo_image_surface_get_stride (impl->surface);
  sw = cairo_image_surface_get_width (impl->surface); sh = cairo_image_surface_get_height (impl->surface);
  if (!data) return FALSE;
  *dst = data + impl->abs_y * *stride + impl->abs_x * bpp;
  *x0 = MAX (0, -impl->abs_x); *y0 = MAX (0, -impl->abs_y); *x1 = MIN (impl->width, sw - impl->abs_x); *y1 = MIN (impl->height, sh - impl->abs_y);
  return *x1 > *x0 && *y1 > *y0;
}

static void
end (GdkDrawableImplSymbian *impl, GdkGC *gc, cairo_t *cr, gint x, gint y, gint w, gint h)
{
  cairo_surface_t *cov = cairo_get_user_data (cr, _gdk_symbian_coverage_key ());
  if (cov) {
    guchar *dst; gint stride, x0, y0, x1, y1;
    cairo_surface_flush (cov);
    if (raster_target (impl, &dst, &stride, &x0, &y0, &x1, &y1)) { _gdk_symbian_raster_combine (gc, dst, stride, impl->depth, x0, y0, x1, y1, cov); cairo_surface_mark_dirty (impl->surface); }
  }
  if (GDK_IS_WINDOW (impl->wrapper)) window_mark_dirty (impl, x, y, w, h);
  cairo_destroy (cr);
}

static void
gdk_drawable_impl_symbian_draw_rectangle (GdkDrawable *d, GdkGC *gc, gboolean filled, gint x, gint y, gint width, gint height)
{
  GdkDrawableImplSymbian *impl = IMPL (d); cairo_t *cr = begin (impl, gc);
  if (!cr) return;
  _gdk_symbian_gc_apply (gc, cr, filled);
  if (filled) { cairo_rectangle (cr, x, y, width, height); cairo_fill (cr); }
  else { cairo_rectangle (cr, x + 0.5, y + 0.5, width, height); cairo_stroke (cr); }
  _gdk_symbian_gc_finish (gc, cr);
  end (impl, gc, cr, x - 1, y - 1, width + 3, height + 3);
}

static void
gdk_drawable_impl_symbian_draw_arc (GdkDrawable *d, GdkGC *gc, gboolean filled, gint x, gint y, gint width, gint height, gint angle1, gint angle2)
{
  GdkDrawableImplSymbian *impl = IMPL (d); cairo_t *cr = begin (impl, gc); double a1, a2;
  if (!cr) return;
  if (width <= 0 || height <= 0) { cairo_destroy (cr); return; }
  _gdk_symbian_gc_apply (gc, cr, filled);
  a1 = -angle1 * (G_PI / 180.0 / 64.0); a2 = -(angle1 + angle2) * (G_PI / 180.0 / 64.0);   /* X angles run counter-clockwise from 3 o'clock; cairo's clockwise (y down) */
  cairo_save (cr);
  cairo_translate (cr, x + width / 2.0, y + height / 2.0); cairo_scale (cr, width / 2.0, height / 2.0);
  if (filled) cairo_move_to (cr, 0, 0);
  if (angle2 >= 0) cairo_arc_negative (cr, 0, 0, 1, a1, a2); else cairo_arc (cr, 0, 0, 1, a1, a2);
  if (filled) cairo_close_path (cr);
  cairo_restore (cr);
  if (filled) cairo_fill (cr); else cairo_stroke (cr);
  _gdk_symbian_gc_finish (gc, cr);
  end (impl, gc, cr, x - 1, y - 1, width + 3, height + 3);
}

static void
bounds (const GdkPoint *p, gint n, gint *x, gint *y, gint *w, gint *h)
{
  gint i, x1 = G_MAXINT, y1 = G_MAXINT, x2 = G_MININT, y2 = G_MININT;
  for (i = 0; i < n; i++) { x1 = MIN (x1, p[i].x); y1 = MIN (y1, p[i].y); x2 = MAX (x2, p[i].x); y2 = MAX (y2, p[i].y); }
  *x = x1 - 2; *y = y1 - 2; *w = x2 - x1 + 5; *h = y2 - y1 + 5;
}

static void
gdk_drawable_impl_symbian_draw_polygon (GdkDrawable *d, GdkGC *gc, gboolean filled, GdkPoint *points, gint npoints)
{
  GdkDrawableImplSymbian *impl = IMPL (d); cairo_t *cr; gint i, bx, by, bw, bh;
  if (npoints < 2 || !(cr = begin (impl, gc))) return;
  _gdk_symbian_gc_apply (gc, cr, filled);
  for (i = 0; i < npoints; i++) { double px = points[i].x + (filled ? 0 : 0.5), py = points[i].y + (filled ? 0 : 0.5); if (i == 0) cairo_move_to (cr, px, py); else cairo_line_to (cr, px, py); }
  cairo_close_path (cr);
  if (filled) cairo_fill (cr); else cairo_stroke (cr);
  _gdk_symbian_gc_finish (gc, cr);
  bounds (points, npoints, &bx, &by, &bw, &bh); end (impl, gc, cr, bx, by, bw, bh);
}

static void
gdk_drawable_impl_symbian_draw_points (GdkDrawable *d, GdkGC *gc, GdkPoint *points, gint npoints)
{
  GdkDrawableImplSymbian *impl = IMPL (d); cairo_t *cr; gint i, bx, by, bw, bh;
  if (npoints < 1 || !(cr = begin (impl, gc))) return;
  _gdk_symbian_gc_apply (gc, cr, TRUE);
  for (i = 0; i < npoints; i++) cairo_rectangle (cr, points[i].x, points[i].y, 1, 1);
  cairo_fill (cr);
  _gdk_symbian_gc_finish (gc, cr);
  bounds (points, npoints, &bx, &by, &bw, &bh); end (impl, gc, cr, bx, by, bw, bh);
}

static void
gdk_drawable_impl_symbian_draw_segments (GdkDrawable *d, GdkGC *gc, GdkSegment *segs, gint nsegs)
{
  GdkDrawableImplSymbian *impl = IMPL (d); cairo_t *cr; gint i, x1 = G_MAXINT, y1 = G_MAXINT, x2 = G_MININT, y2 = G_MININT;
  if (nsegs < 1 || !(cr = begin (impl, gc))) return;
  _gdk_symbian_gc_apply (gc, cr, FALSE);
  for (i = 0; i < nsegs; i++) {
    cairo_move_to (cr, segs[i].x1 + 0.5, segs[i].y1 + 0.5); cairo_line_to (cr, segs[i].x2 + 0.5, segs[i].y2 + 0.5);
    x1 = MIN (x1, MIN (segs[i].x1, segs[i].x2)); y1 = MIN (y1, MIN (segs[i].y1, segs[i].y2)); x2 = MAX (x2, MAX (segs[i].x1, segs[i].x2)); y2 = MAX (y2, MAX (segs[i].y1, segs[i].y2));
  }
  cairo_stroke (cr);
  _gdk_symbian_gc_finish (gc, cr);
  end (impl, gc, cr, x1 - 2, y1 - 2, x2 - x1 + 5, y2 - y1 + 5);
}

static void
gdk_drawable_impl_symbian_draw_lines (GdkDrawable *d, GdkGC *gc, GdkPoint *points, gint npoints)
{
  GdkDrawableImplSymbian *impl = IMPL (d); cairo_t *cr; gint i, bx, by, bw, bh;
  if (npoints < 2 || !(cr = begin (impl, gc))) return;
  _gdk_symbian_gc_apply (gc, cr, FALSE);
  for (i = 0; i < npoints; i++) { if (i == 0) cairo_move_to (cr, points[i].x + 0.5, points[i].y + 0.5); else cairo_line_to (cr, points[i].x + 0.5, points[i].y + 0.5); }
  cairo_stroke (cr);
  _gdk_symbian_gc_finish (gc, cr);
  bounds (points, npoints, &bx, &by, &bw, &bh); end (impl, gc, cr, bx, by, bw, bh);
}

static void
draw_glyphs_matrix (GdkDrawable *d, GdkGC *gc, const PangoMatrix *matrix, PangoFont *font, gint x, gint y, PangoGlyphString *glyphs)
{
  GdkDrawableImplSymbian *impl = IMPL (d); cairo_t *cr = begin (impl, gc); PangoRectangle ink;
  if (!cr) return;
  _gdk_symbian_gc_apply (gc, cr, FALSE);
  if (matrix) { cairo_matrix_t m; cairo_matrix_init (&m, matrix->xx, matrix->yx, matrix->xy, matrix->yy, matrix->x0, matrix->y0); cairo_transform (cr, &m); }
  cairo_move_to (cr, x, y);
  pango_cairo_show_glyph_string (cr, font, glyphs);
  _gdk_symbian_gc_finish (gc, cr);
  pango_glyph_string_extents (glyphs, font, &ink, NULL);
  end (impl, gc, cr, x + PANGO_PIXELS (ink.x) - 2, y + PANGO_PIXELS (ink.y) - 2, PANGO_PIXELS (ink.width) + 5, PANGO_PIXELS (ink.height) + 5);
}
static void gdk_drawable_impl_symbian_draw_glyphs (GdkDrawable *d, GdkGC *gc, PangoFont *font, gint x, gint y, PangoGlyphString *glyphs) { draw_glyphs_matrix (d, gc, NULL, font, x, y, glyphs); }
static void gdk_drawable_impl_symbian_draw_glyphs_transformed (GdkDrawable *d, GdkGC *gc, PangoMatrix *matrix, PangoFont *font, gint x, gint y, PangoGlyphString *glyphs)
{
  /* GDK passes x, y in the transformed space in Pango units' integer pixels: apply the matrix to the origin as well, as the X11 backend does with its glyph matrix */
  draw_glyphs_matrix (d, gc, matrix, font, x, y, glyphs);
}

static void
gdk_drawable_impl_symbian_draw_trapezoids (GdkDrawable *d, GdkGC *gc, GdkTrapezoid *traps, gint n)
{
  GdkDrawableImplSymbian *impl = IMPL (d); cairo_t *cr; gint i;
  if (n < 1 || !(cr = begin (impl, gc))) return;
  _gdk_symbian_gc_apply (gc, cr, TRUE);
  for (i = 0; i < n; i++) {
    cairo_move_to (cr, traps[i].x11, traps[i].y1); cairo_line_to (cr, traps[i].x21, traps[i].y1);
    cairo_line_to (cr, traps[i].x22, traps[i].y2); cairo_line_to (cr, traps[i].x12, traps[i].y2); cairo_close_path (cr);
  }
  cairo_fill (cr);
  _gdk_symbian_gc_finish (gc, cr);
  end (impl, gc, cr, 0, 0, impl->width, impl->height);
}

/* Per-pixel copy of a source (cairo image surface data or GdkImage memory) with the raster function and clip of the GC: used when the function is not COPY and when the source is a
 * bitmap (XCopyPlane semantics: a 1 bit becomes the foreground pixel, a 0 bit the background pixel). The pixel math is gdkraster.c (unit-tested on the host). */
static int clip_cb (void *gc, int x, int y) { if (copy_occ && gdk_region_point_in (copy_occ, x, y)) return 0; return _gdk_symbian_gc_clip_test (gc, x, y); }

static void
copy_pixels (GdkDrawableImplSymbian *impl, GdkGC *gc, const guchar *sd, gint sstride, gint sw, gint sh, gboolean src_is_bitmap, gint xsrc, gint ysrc, gint xdest, gint ydest, gint width, gint height)
{
  guchar *dst; gint stride, x0, y0, x1, y1; guint32 fg, bg;
  if (_gdk_symbian_gc_function (gc) == GDK_NOOP || !raster_target (impl, &dst, &stride, &x0, &y0, &x1, &y1)) return;
  _gdk_symbian_gc_pixels (gc, &fg, &bg);
  copy_occ = occluders (impl);
  phe_raster_copy (dst, stride, impl->depth, x0, y0, x1, y1, sd, sstride, sw, sh, src_is_bitmap, xsrc, ysrc, xdest, ydest, width, height, _gdk_symbian_gc_function (gc), fg, bg, clip_cb, gc);
  if (copy_occ) { gdk_region_destroy (copy_occ); copy_occ = NULL; }
  cairo_surface_mark_dirty (impl->surface);
  if (GDK_IS_WINDOW (impl->wrapper)) window_mark_dirty (impl, xdest, ydest, width, height);
}

/* copy from another drawable: the source surface is painted through the GC clip */
static void
gdk_drawable_impl_symbian_draw_drawable_with_src (GdkDrawable *d, GdkGC *gc, GdkDrawable *src, gint xsrc, gint ysrc, gint xdest, gint ydest, gint width, gint height, GdkDrawable *original_src)
{
  GdkDrawableImplSymbian *impl = IMPL (d); cairo_t *cr; cairo_surface_t *ss;
  (void) original_src;
  ss = _gdk_drawable_ref_cairo_surface (src);
  if (!ss) return;
  if (gc && (_gdk_symbian_gc_raster_mode (gc) || cairo_surface_get_content (ss) == CAIRO_CONTENT_ALPHA)) {
    const guchar *sd; gint sstride, sw_, sh_;
    cairo_surface_flush (ss);
    sd = cairo_image_surface_get_data (ss); sstride = cairo_image_surface_get_stride (ss); sw_ = cairo_image_surface_get_width (ss); sh_ = cairo_image_surface_get_height (ss);
    if (!sd && GDK_IS_WINDOW (src)) {                                                /* a window source: the rectangle of the shared bitmap (the window's offset inside its native window included) */
      GdkWindowObject *sp = GDK_WINDOW_OBJECT (src); GdkDrawableImplSymbian *si = (GdkDrawableImplSymbian *) sp->impl;
      sstride = cairo_image_surface_get_stride (si->surface);
      sd = cairo_image_surface_get_data (si->surface) + (si->abs_y + sp->abs_y) * sstride + 4 * (si->abs_x + sp->abs_x); sw_ = sp->width; sh_ = sp->height;
    }
    copy_pixels (impl, gc, sd, sstride, sw_, sh_, cairo_surface_get_content (ss) == CAIRO_CONTENT_ALPHA, xsrc, ysrc, xdest, ydest, width, height);
    cairo_surface_destroy (ss); return;
  }
  if (!(cr = begin (impl, gc))) { cairo_surface_destroy (ss); return; }
  if (gc) _gdk_symbian_gc_apply (gc, cr, FALSE);
  cairo_rectangle (cr, xdest, ydest, width, height); cairo_clip (cr);
  if (cairo_surface_get_content (ss) == CAIRO_CONTENT_ALPHA) {
    /* a bitmap source: paint the foreground colour of the GC through its alpha (what XCopyPlane does) */
    if (!gc) cairo_set_source_rgba (cr, 0, 0, 0, 1);
    cairo_mask_surface (cr, ss, xdest - xsrc, ydest - ysrc);
  } else {
    cairo_set_source_surface (cr, ss, xdest - xsrc, ydest - ysrc);
    if (!gc) cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint (cr);
  }
  cairo_surface_destroy (ss);
  if (gc) _gdk_symbian_gc_finish (gc, cr);
  end (impl, gc, cr, xdest, ydest, width, height);
}
static void gdk_drawable_impl_symbian_draw_drawable (GdkDrawable *d, GdkGC *gc, GdkDrawable *src, gint xsrc, gint ysrc, gint xdest, gint ydest, gint width, gint height)
{
  gdk_drawable_impl_symbian_draw_drawable_with_src (d, gc, src, xsrc, ysrc, xdest, ydest, width, height, src);
}

static void
gdk_drawable_impl_symbian_draw_image (GdkDrawable *d, GdkGC *gc, GdkImage *image, gint xsrc, gint ysrc, gint xdest, gint ydest, gint width, gint height)
{
  GdkDrawableImplSymbian *impl = IMPL (d); cairo_t *cr; cairo_surface_t *is; cairo_format_t fmt;
  if (gc && (_gdk_symbian_gc_raster_mode (gc) || image->depth == 1)) {
    copy_pixels (impl, gc, image->mem, image->bpl, image->width, image->height, image->depth == 1, xsrc, ysrc, xdest, ydest, width, height);
    return;
  }
  if (!(cr = begin (impl, gc))) return;
  fmt = image->depth == 1 ? CAIRO_FORMAT_A8 : CAIRO_FORMAT_RGB24;
  is = cairo_image_surface_create_for_data (image->mem, fmt, image->width, image->height, image->bpl);
  if (gc) _gdk_symbian_gc_apply (gc, cr, FALSE);
  cairo_rectangle (cr, xdest, ydest, width, height); cairo_clip (cr);
  if (fmt == CAIRO_FORMAT_A8) cairo_mask_surface (cr, is, xdest - xsrc, ydest - ysrc);
  else { cairo_set_source_surface (cr, is, xdest - xsrc, ydest - ysrc); cairo_paint (cr); }
  cairo_surface_destroy (is);
  if (gc) _gdk_symbian_gc_finish (gc, cr);
  end (impl, gc, cr, xdest, ydest, width, height);
}

/* ---- reading back ---- */
static GdkImage *
gdk_drawable_impl_symbian_copy_to_image (GdkDrawable *d, GdkImage *image, gint src_x, gint src_y, gint dest_x, gint dest_y, gint width, gint height)
{
  GdkDrawableImplSymbian *impl = IMPL (d); cairo_surface_t *s; guchar *data; gint stride, x, y, sw, sh;
  if (!image) image = _gdk_image_new_for_depth (_gdk_screen, GDK_IMAGE_FASTEST, NULL, width, height, impl->depth);
  if (!impl->surface) return image;
  s = cairo_surface_reference (impl->surface);
  cairo_surface_flush (s);
  data = cairo_image_surface_get_data (s); stride = cairo_image_surface_get_stride (s);
  sw = cairo_image_surface_get_width (s); sh = cairo_image_surface_get_height (s);
  if (GDK_IS_WINDOW (impl->wrapper) && data) {                                      /* a window is a rectangle of the shared bitmap: its pixels start at its origin there */
    data += impl->abs_y * stride + 4 * impl->abs_x; sw = impl->width; sh = impl->height;
  }
  if (data) for (y = 0; y < height; y++) for (x = 0; x < width; x++) {
    gint sx = src_x + x, sy = src_y + y; guint32 px = 0;
    if (sx >= 0 && sy >= 0 && sx < sw && sy < sh) px = impl->depth == 1 ? (data[sy * stride + sx] ? 1 : 0) : (*(guint32 *) (data + sy * stride + 4 * sx) & 0x00ffffff);
    gdk_image_put_pixel (image, dest_x + x, dest_y + y, px);
  }
  cairo_surface_destroy (s);
  return image;
}
static GdkImage *gdk_drawable_impl_symbian_get_image (GdkDrawable *d, gint x, gint y, gint width, gint height) { return gdk_drawable_impl_symbian_copy_to_image (d, NULL, x, y, 0, 0, width, height); }

/* text in a GdkFont (gdkfont-symbian.c): a Pango layout drawn through the same cairo path as the other primitives */
void _gdk_symbian_font_show (cairo_t *cr, GdkFont *font, gint x, gint y, const gchar *utf8);
gchar *_gdk_symbian_font_utf8 (const gchar *text, gint length);
gchar *_gdk_symbian_font_utf8_wc (const GdkWChar *text, gint length);

static void
draw_text_utf8 (GdkDrawable *d, GdkFont *font, GdkGC *gc, gint x, gint y, const gchar *utf8)
{
  GdkDrawableImplSymbian *impl = IMPL (d); cairo_t *cr = begin (impl, gc); gint w;
  if (!cr) return;
  _gdk_symbian_gc_apply (gc, cr, FALSE);
  _gdk_symbian_font_show (cr, font, x, y, utf8);
  _gdk_symbian_gc_finish (gc, cr);
  w = gdk_text_width (font, utf8, (gint) strlen (utf8));
  end (impl, gc, cr, x - 2, y - font->ascent - 2, w + 5, font->ascent + font->descent + 5);
}
static void gdk_drawable_impl_symbian_draw_text (GdkDrawable *d, GdkFont *font, GdkGC *gc, gint x, gint y, const gchar *text, gint len) { gchar *u = _gdk_symbian_font_utf8 (text, len); draw_text_utf8 (d, font, gc, x, y, u); g_free (u); }
static void gdk_drawable_impl_symbian_draw_text_wc (GdkDrawable *d, GdkFont *font, GdkGC *gc, gint x, gint y, const GdkWChar *text, gint len) { gchar *u = _gdk_symbian_font_utf8_wc (text, len); draw_text_utf8 (d, font, gc, x, y, u); g_free (u); }

static void
gdk_drawable_impl_symbian_class_init (GdkDrawableImplSymbianClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass); GdkDrawableClass *dc = GDK_DRAWABLE_CLASS (klass);
  object_class->finalize = gdk_drawable_impl_symbian_finalize;
  dc->create_gc = gdk_drawable_impl_symbian_create_gc;
  dc->draw_rectangle = gdk_drawable_impl_symbian_draw_rectangle;
  dc->draw_arc = gdk_drawable_impl_symbian_draw_arc;
  dc->draw_polygon = gdk_drawable_impl_symbian_draw_polygon;
  dc->draw_text = gdk_drawable_impl_symbian_draw_text;
  dc->draw_text_wc = gdk_drawable_impl_symbian_draw_text_wc;
  dc->draw_points = gdk_drawable_impl_symbian_draw_points;
  dc->draw_segments = gdk_drawable_impl_symbian_draw_segments;
  dc->draw_lines = gdk_drawable_impl_symbian_draw_lines;
  dc->draw_glyphs = gdk_drawable_impl_symbian_draw_glyphs;
  dc->draw_glyphs_transformed = gdk_drawable_impl_symbian_draw_glyphs_transformed;
  dc->draw_trapezoids = gdk_drawable_impl_symbian_draw_trapezoids;
  dc->draw_drawable = gdk_drawable_impl_symbian_draw_drawable;
  dc->draw_drawable_with_src = gdk_drawable_impl_symbian_draw_drawable_with_src;
  dc->draw_image = gdk_drawable_impl_symbian_draw_image;
  dc->get_depth = gdk_drawable_impl_symbian_get_depth;
  dc->get_size = gdk_drawable_impl_symbian_get_size;
  dc->set_colormap = gdk_drawable_impl_symbian_set_colormap;
  dc->get_colormap = gdk_drawable_impl_symbian_get_colormap;
  dc->get_visual = gdk_drawable_impl_symbian_get_visual;
  dc->get_screen = gdk_drawable_impl_symbian_get_screen;
  dc->get_image = gdk_drawable_impl_symbian_get_image;
  dc->_copy_to_image = gdk_drawable_impl_symbian_copy_to_image;
  dc->ref_cairo_surface = gdk_drawable_impl_symbian_ref_cairo_surface;
  dc->create_cairo_surface = gdk_drawable_impl_symbian_create_cairo_surface;
  dc->set_cairo_clip = gdk_drawable_impl_symbian_set_cairo_clip;
  dc->get_source_drawable = gdk_drawable_impl_symbian_get_source_drawable;
}

/* ---- dirty rectangles of windows -> WSERV ---- */
static GList *dirty_windows = NULL;
static guint dirty_idle = 0;
static gboolean flush_idle (gpointer data);

static void
window_mark_dirty (GdkDrawableImplSymbian *impl, gint x, gint y, gint w, gint h)
{
  GdkWindowImplSymbian *wi = (GdkWindowImplSymbian *) impl; GdkWindow *win = GDK_WINDOW (impl->wrapper); GdkRectangle r, cur;
  extern gboolean _gdk_symbian_window_dirty (GdkWindow *win, GdkRectangle *r, gboolean add);
  (void) wi;
  r.x = MAX (0, x); r.y = MAX (0, y); r.width = MIN (impl->width, x + w) - r.x; r.height = MIN (impl->height, y + h) - r.y;
  if (r.width <= 0 || r.height <= 0) return;
  cur = r;
  if (_gdk_symbian_window_dirty (win, &cur, TRUE) && !g_list_find (dirty_windows, win)) dirty_windows = g_list_prepend (dirty_windows, win);
  if (!dirty_idle) dirty_idle = gdk_threads_add_idle_full (GDK_PRIORITY_REDRAW + 1, flush_idle, NULL, NULL);
}

/* Direct drawing on a window: gdk_cairo_create (window) outside a paint hands out the surface of the window itself (a rectangle of the display bitmap), and cairo then draws into the bitmap
 * with nothing to tell this backend which pixels changed. The window class's ref_cairo_surface is wrapped (installed by _gdk_symbian_window_hooks at display open): a request for the direct
 * surface marks the exposed region (see _gdk_windowing_window_process_updates_recurse) or, outside an expose, the whole native window dirty, so the next flush presents it. */
gboolean _gdk_symbian_expose_box (GdkRectangle *box);
static cairo_surface_t *(*orig_ref_cairo_surface) (GdkDrawable *drawable) = NULL;
static void (*orig_set_cairo_clip) (GdkDrawable *drawable, cairo_t *cr) = NULL;

/* gdk_cairo_create () on a window ends in the window class's set_cairo_clip: after the core has clipped the context to the window's own visible region, the part covered by the windows
 * stacked above (the native windows share one bitmap) is clipped out as well, so direct cairo drawing cannot paint over them */
static void
window_set_cairo_clip_occluded (GdkDrawable *drawable, cairo_t *cr)
{
  GdkWindowObject *p = GDK_WINDOW_OBJECT (drawable); GdkRegion *r;
  orig_set_cairo_clip (drawable, cr);
  if (p->paint_stack || !p->impl) return;                                        /* painting into a buffer is clipped when the buffer is copied */
  r = occluders ((GdkDrawableImplSymbian *) p->impl);
  { static int trace = -1; if (trace < 0) trace = g_getenv ("PHEGDK_TRACE_CLIP") != NULL;
    if (trace) { GdkRectangle b = { 0, 0, 0, 0 }; gdouble x1, y1, x2, y2; gchar *m; if (r) gdk_region_get_clipbox (r, &b); cairo_clip_extents (cr, &x1, &y1, &x2, &y2);
      m = g_strdup_printf ("[phegdk] direct clip: window %p abs %d,%d occluders %s box %d,%d %dx%d, clip extents after the core %.0f,%.0f %.0f,%.0f", (void *) drawable, p->abs_x, p->abs_y, r ? "yes" : "none", b.x, b.y, b.width, b.height, x1, y1, x2, y2);
      phe_log (m); g_free (m); } }
  if (r) {
    GdkRectangle *rects; gint n, k;
    gdk_region_offset (r, -p->abs_x, -p->abs_y);                                  /* from the coordinates of the native window to those of this window */
    gdk_region_get_rectangles (r, &rects, &n);
    cairo_new_path (cr);
    cairo_rectangle (cr, -32768, -32768, 65536, 65536);
    for (k = 0; k < n; k++) cairo_rectangle (cr, rects[k].x, rects[k].y, rects[k].width, rects[k].height);
    cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD); cairo_clip (cr); cairo_set_fill_rule (cr, CAIRO_FILL_RULE_WINDING); cairo_new_path (cr);
    g_free (rects); gdk_region_destroy (r);
  }
}

static cairo_surface_t *
window_ref_cairo_surface_marking (GdkDrawable *drawable)
{
  cairo_surface_t *s = orig_ref_cairo_surface (drawable); GdkWindowObject *p = GDK_WINDOW_OBJECT (drawable);
  if (s && !p->paint_stack && p->impl_window && p->impl_window->impl && GDK_IS_DRAWABLE_IMPL_SYMBIAN (p->impl_window->impl)) {
    GdkDrawableImplSymbian *impl = GDK_DRAWABLE_IMPL_SYMBIAN (p->impl_window->impl);
    GdkRectangle box;
    if (_gdk_symbian_expose_box (&box)) window_mark_dirty (impl, box.x, box.y, box.width, box.height);        /* inside an expose: the exposed region */
    else window_mark_dirty (impl, 0, 0, impl->width, impl->height);                                              /* direct drawing with no bound: the whole window */
  }
  return s;
}

void
_gdk_symbian_window_hooks (void)
{
  GdkDrawableClass *wc = GDK_DRAWABLE_CLASS (g_type_class_ref (GDK_TYPE_WINDOW));
  if (!orig_ref_cairo_surface) { orig_ref_cairo_surface = wc->ref_cairo_surface; wc->ref_cairo_surface = window_ref_cairo_surface_marking; orig_set_cairo_clip = wc->set_cairo_clip; wc->set_cairo_clip = window_set_cairo_clip_occluded; }
}

void
_gdk_symbian_flush_dirty (void)
{
  GList *l, *list = dirty_windows; dirty_windows = NULL;
  extern gboolean _gdk_symbian_window_dirty (GdkWindow *win, GdkRectangle *r, gboolean add);
  for (l = list; l; l = l->next) {
    GdkRectangle r;
    if (_gdk_symbian_window_dirty (l->data, &r, FALSE)) _gdk_symbian_present (l->data, r.x, r.y, r.width, r.height);
  }
  g_list_free (list);
}
static gboolean flush_idle (gpointer data) { (void) data; dirty_idle = 0; _gdk_symbian_flush_dirty (); return FALSE; }
