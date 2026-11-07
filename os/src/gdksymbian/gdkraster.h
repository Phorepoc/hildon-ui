/* gdkraster.h -- the pixel math of the Symbian GDK backend's raster operations, free of GLib/GDK/cairo so that it can be unit-tested on the host (tests/user/gdk_raster).
 * BSD-2-Clause (Phorepoc). Function numbers are those of GdkFunction (checked with static asserts in gdkgc-symbian.c). */
#ifndef GDKRASTER_H
#define GDKRASTER_H

enum { PHE_FN_COPY = 0, PHE_FN_INVERT, PHE_FN_XOR, PHE_FN_CLEAR, PHE_FN_AND, PHE_FN_AND_REVERSE, PHE_FN_AND_INVERT, PHE_FN_NOOP,
       PHE_FN_OR, PHE_FN_EQUIV, PHE_FN_OR_REVERSE, PHE_FN_COPY_INVERT, PHE_FN_OR_INVERT, PHE_FN_NAND, PHE_FN_NOR, PHE_FN_SET };
enum { PHE_FILL_SOLID = 0, PHE_FILL_TILED, PHE_FILL_STIPPLED, PHE_FILL_OPAQUE_STIPPLED };      /* GdkFill */

typedef struct { const unsigned char *data; int stride, w, h; } PheRasterPlane;     /* an A8 (1 byte per pixel, nonzero = 1) or 32 bit plane */

typedef struct {
  int function;                    /* PHE_FN_* */
  int depth;                       /* 1 (A8 bitmap), 24 (0x00RRGGBB), 32 (ARGB) */
  unsigned fg, bg;                 /* pixel values */
  int fill;                        /* PHE_FILL_* */
  PheRasterPlane tile, stipple, mask; int has_tile, has_stipple, has_mask;
  int ts_x, ts_y, clip_x, clip_y;  /* tile/stipple origin, clip mask origin */
} PheRasterState;

/* dest = function (source, dest) on one pixel value (all 32 bits) */
unsigned phe_raster_function (int function, unsigned s, unsigned d);

/* writes function (s, d) into the pixel (x, y) of an image of the given depth: 1 bit = bit 0 of an A8 byte, 24 bit keeps the upper byte, 32 bit all bits */
void phe_raster_put (unsigned char *dst, int dst_stride, int depth, int x, int y, int function, unsigned s);

/* The shape (coverage >= 128) of a primitive is combined with the pixels in [x0,x1) x [y0,y1): the source pixel is the foreground (SOLID), the tile pixel (TILED), the foreground
 * where the stipple bit is set (STIPPLED) or the foreground / background by the stipple bit (OPAQUE_STIPPLED); the clip mask (not repeated) is applied per pixel. */
void phe_raster_combine (const PheRasterState *st, unsigned char *dst, int dst_stride, int x0, int y0, int x1, int y1, const unsigned char *cov, int cov_stride);

typedef int (*PheClipTest) (void *user, int x, int y);
/* Copies a w x h rectangle of a source to the destination through the function: a bitmap source (src_is_bitmap, A8) gives fg for 1 bits and bg for 0 bits (XCopyPlane), any other
 * source is 32 bit pixels. Destination coordinates outside [dx0,dx1) x [dy0,dy1) and source coordinates outside the source are skipped; clip (optional) may veto a pixel. */
void phe_raster_copy (unsigned char *dst, int dst_stride, int depth, int dx0, int dy0, int dx1, int dy1,
                      const unsigned char *src, int src_stride, int sw, int sh, int src_is_bitmap,
                      int xsrc, int ysrc, int xdest, int ydest, int width, int height,
                      int function, unsigned fg, unsigned bg, PheClipTest clip, void *clip_user);

#endif
