/* phe_pixman_test.c -- pixman 0.42.2 on the x32 image: solid fills, an OVER composite with alpha, an image-to-image copy, a scaled copy. BSD-2-Clause (Phorepoc). */
#include <pixman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern void phe_log (const char *msg);

static int near_ (uint32_t a, uint32_t b)   /* every ARGB channel within 2 */
{
  int s; for (s = 0; s < 32; s += 8) { int x = (int) ((a >> s) & 0xff), y = (int) ((b >> s) & 0xff); if (x - y > 2 || y - x > 2) return 0; }
  return 1;
}
#define T(c, what) do { checks++; if (!(c)) { fails++; snprintf (m, sizeof m, "pixman FAIL: %s (line %d)", what, __LINE__); phe_log (m); } } while (0)

int phe_pixman_test (void)
{
  int checks = 0, fails = 0, w = 64, h = 64, i; char m[200];
  uint32_t *dst = malloc (w * h * 4), *src2 = malloc (16 * 16 * 4), *big = malloc (32 * 32 * 4);
  pixman_image_t *d, *s, *s2, *b; pixman_color_t red50 = { 0x8000, 0, 0, 0x8000 };   /* pixman_color_t is PREMULTIPLIED: 50% red = r 0x8000, a 0x8000 */ pixman_transform_t tr;
  for (i = 0; i < w * h; i++) dst[i] = 0xff0000ffu;                       /* opaque blue */
  d = pixman_image_create_bits (PIXMAN_a8r8g8b8, w, h, dst, w * 4);
  T (d != NULL, "create_bits");
  s = pixman_image_create_solid_fill (&red50);
  pixman_image_composite32 (PIXMAN_OP_OVER, s, NULL, d, 0, 0, 0, 0, 8, 8, 16, 16);
  T (near_ (dst[10 * w + 10], 0xff800080u), "OVER red 50% on blue");   /* premultiplied src (0x80,0,0,0x80) + blue * 0.5 */
  T (dst[2 * w + 2] == 0xff0000ffu, "pixel outside the rectangle untouched");
  pixman_fill (dst, w, 32, 30, 30, 4, 4, 0xff00ff00u);                      /* opaque green */
  T (dst[31 * w + 31] == 0xff00ff00u && dst[29 * w + 29] == 0xff0000ffu, "pixman_fill");
  for (i = 0; i < 16 * 16; i++) src2[i] = 0xffffff00u;                    /* opaque yellow 16x16 */
  s2 = pixman_image_create_bits (PIXMAN_a8r8g8b8, 16, 16, src2, 64);
  pixman_image_composite32 (PIXMAN_OP_SRC, s2, NULL, d, 0, 0, 0, 0, 40, 4, 16, 16);
  T (dst[5 * w + 41] == 0xffffff00u && dst[3 * w + 41] == 0xff0000ffu, "SRC copy");
  b = pixman_image_create_bits (PIXMAN_a8r8g8b8, 32, 32, big, 128);        /* 2x scaled copy of the 16x16 yellow image */
  for (i = 0; i < 32 * 32; i++) big[i] = 0;
  pixman_transform_init_scale (&tr, pixman_double_to_fixed (0.5), pixman_double_to_fixed (0.5));
  pixman_image_set_transform (s2, &tr); pixman_image_set_filter (s2, PIXMAN_FILTER_NEAREST, NULL, 0);
  pixman_image_composite32 (PIXMAN_OP_SRC, s2, NULL, b, 0, 0, 0, 0, 0, 0, 32, 32);
  T (big[0] == 0xffffff00u && big[31 * 32 + 31] == 0xffffff00u, "scaled copy");
  snprintf (m, sizeof m, "pixman %s: %d checks, %d failures, OVER pixel=%08x", pixman_version_string (), checks, fails, (unsigned) dst[10 * w + 10]); phe_log (m);
  pixman_image_unref (s); pixman_image_unref (s2); pixman_image_unref (d); pixman_image_unref (b); free (dst); free (src2); free (big);
  return fails;
}
