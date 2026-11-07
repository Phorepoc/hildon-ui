/* gdkraster.c -- see gdkraster.h. BSD-2-Clause (Phorepoc). Plain C, no GLib: built by the backend and by the host unit test. */
#include <stdint.h>
#include <string.h>
#include "gdkraster.h"

unsigned
phe_raster_function (int f, unsigned s, unsigned d)
{
  switch (f) {
    case PHE_FN_CLEAR: return 0;
    case PHE_FN_AND: return s & d;
    case PHE_FN_AND_REVERSE: return s & ~d;
    case PHE_FN_COPY: return s;
    case PHE_FN_AND_INVERT: return ~s & d;
    case PHE_FN_NOOP: return d;
    case PHE_FN_XOR: return s ^ d;
    case PHE_FN_OR: return s | d;
    case PHE_FN_NOR: return ~(s | d);
    case PHE_FN_EQUIV: return ~(s ^ d);
    case PHE_FN_INVERT: return ~d;
    case PHE_FN_OR_REVERSE: return s | ~d;
    case PHE_FN_COPY_INVERT: return ~s;
    case PHE_FN_OR_INVERT: return ~s | d;
    case PHE_FN_NAND: return ~(s & d);
    case PHE_FN_SET: return 0xffffffffu;
  }
  return s;
}

void
phe_raster_put (unsigned char *dst, int stride, int depth, int x, int y, int f, unsigned s)
{
  unsigned char *dp = dst + (size_t) y * stride + (depth == 1 ? x : 4 * x);
  if (depth == 1) { unsigned d = dp[0] ? 1u : 0u; dp[0] = (phe_raster_function (f, s & 1u, d) & 1u) ? 0xff : 0x00; }
  else {
    uint32_t d, v, keep = depth == 24 ? 0x00ffffffu : 0xffffffffu;
    memcpy (&d, dp, 4);
    v = (phe_raster_function (f, s, d) & keep) | (d & ~keep);
    memcpy (dp, &v, 4);
  }
}

static unsigned
plane_pixel (const PheRasterPlane *pl, int x, int y, int bpp)
{
  int px = ((x % pl->w) + pl->w) % pl->w, py = ((y % pl->h) + pl->h) % pl->h;
  const unsigned char *p = pl->data + (size_t) py * pl->stride + (size_t) px * bpp;
  if (bpp == 1) return p[0] ? 1u : 0u;
  { uint32_t v; memcpy (&v, p, 4); return v; }
}

void
phe_raster_combine (const PheRasterState *st, unsigned char *dst, int dst_stride, int x0, int y0, int x1, int y1, const unsigned char *cov, int cov_stride)
{
  int x, y; unsigned fg = st->fg, bg = st->bg;
  if (st->depth == 1) { fg &= 1u; bg &= 1u; } else if (st->depth == 32) { fg |= 0xff000000u; bg |= 0xff000000u; }
  for (y = y0; y < y1; y++) for (x = x0; x < x1; x++) {
    unsigned s;
    if (cov[y * cov_stride + x] < 128) continue;
    if (st->has_mask) {
      int mx = x - st->clip_x, my = y - st->clip_y;
      if (mx < 0 || my < 0 || mx >= st->mask.w || my >= st->mask.h || !st->mask.data[(size_t) my * st->mask.stride + mx]) continue;
    }
    if (st->has_stipple && (st->fill == PHE_FILL_STIPPLED || st->fill == PHE_FILL_OPAQUE_STIPPLED)) {
      int bit = plane_pixel (&st->stipple, x - st->ts_x, y - st->ts_y, 1) != 0;
      if (st->fill == PHE_FILL_STIPPLED && !bit) continue;
      s = bit ? fg : bg;
    } else if (st->has_tile && st->fill == PHE_FILL_TILED) s = plane_pixel (&st->tile, x - st->ts_x, y - st->ts_y, st->depth == 1 ? 1 : 4);
    else s = fg;
    phe_raster_put (dst, dst_stride, st->depth, x, y, st->function, s);
  }
}

void
phe_raster_copy (unsigned char *dst, int dst_stride, int depth, int dx0, int dy0, int dx1, int dy1,
                 const unsigned char *src, int src_stride, int sw, int sh, int src_is_bitmap,
                 int xsrc, int ysrc, int xdest, int ydest, int width, int height,
                 int function, unsigned fg, unsigned bg, PheClipTest clip, void *clip_user)
{
  int x, y;
  if (function == PHE_FN_NOOP) return;
  if (depth == 32) { fg |= 0xff000000u; bg |= 0xff000000u; }
  for (y = 0; y < height; y++) for (x = 0; x < width; x++) {
    int dx = xdest + x, dy = ydest + y, sx = xsrc + x, sy = ysrc + y; unsigned s;
    if (dx < dx0 || dx >= dx1 || dy < dy0 || dy >= dy1 || sx < 0 || sy < 0 || sx >= sw || sy >= sh) continue;
    if (clip && !clip (clip_user, dx, dy)) continue;
    if (src_is_bitmap) s = src[(size_t) sy * src_stride + sx] ? fg : bg;
    else { uint32_t v; memcpy (&v, src + (size_t) sy * src_stride + 4 * (size_t) sx, 4); s = v; }
    phe_raster_put (dst, dst_stride, depth, dx, dy, function, s);
  }
}
