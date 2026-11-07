/* phe_pixbuf_test.c -- gdk-pixbuf 2.36.12 with the built-in PNG loader (libpng 1.6.43 + zlib 1.3.1): decode an 8x4 PNG through GdkPixbufLoader and through a GMemoryInputStream (gio), check the pixels, scale it. BSD-2-Clause (Phorepoc). */
#include <stdio.h>
#include <string.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gio/gio.h>
#include "phe_png_data.h"
extern void phe_log (const char *msg);
#define T(c, what) do { checks++; if (!(c)) { fails++; snprintf (m, sizeof m, "pixbuf FAIL: %s (line %d)", what, __LINE__); phe_log (m); } } while (0)

int phe_pixbuf_test (void)
{
  int checks = 0, fails = 0; char m[260]; GError *err = NULL; GdkPixbuf *pb;
  GdkPixbufLoader *ld = gdk_pixbuf_loader_new_with_type ("png", &err);
  T (ld != NULL, "png loader exists");
  if (ld) {
    gboolean ok = gdk_pixbuf_loader_write (ld, phe_png, sizeof phe_png, &err); gboolean cl = gdk_pixbuf_loader_close (ld, &err);
    pb = gdk_pixbuf_loader_get_pixbuf (ld);
    snprintf (m, sizeof m, "pixbuf: loader write=%d close=%d pixbuf=%p %s", ok, cl, (void *) pb, err ? err->message : "-"); phe_log (m); T (ok && cl && pb, "decode through the loader");
    if (pb) { const guchar *p = gdk_pixbuf_get_pixels (pb); int rs = gdk_pixbuf_get_rowstride (pb), nc = gdk_pixbuf_get_n_channels (pb);
      snprintf (m, sizeof m, "pixbuf: %dx%d, %d channels, px(0,0)=%02x%02x%02x px(7,3)=%02x%02x%02x", gdk_pixbuf_get_width (pb), gdk_pixbuf_get_height (pb), nc, p[0], p[1], p[2], p[7 * nc + 3 * rs], p[7 * nc + 3 * rs + 1], p[7 * nc + 3 * rs + 2]); phe_log (m);
      T (gdk_pixbuf_get_width (pb) == 8 && gdk_pixbuf_get_height (pb) == 4 && nc >= 3, "size 8x4");
      T (p[0] == 255 && p[1] == 0 && p[2] == 0 && p[7 * nc + 3 * rs] == 0 && p[7 * nc + 3 * rs + 2] == 255, "red left, blue right");
      { GdkPixbuf *sc = gdk_pixbuf_scale_simple (pb, 4, 2, GDK_INTERP_BILINEAR); T (sc && gdk_pixbuf_get_width (sc) == 4 && gdk_pixbuf_get_height (sc) == 2 && gdk_pixbuf_get_pixels (sc)[0] > 200, "scale_simple 4x2"); if (sc) g_object_unref (sc); }
      { GdkPixbuf *al = gdk_pixbuf_add_alpha (pb, FALSE, 0, 0, 0); T (al && gdk_pixbuf_get_has_alpha (al), "add_alpha"); if (al) g_object_unref (al); } }
    g_clear_error (&err); g_object_unref (ld); }
  { GInputStream *in0 = g_memory_input_stream_new_from_data (phe_png, sizeof phe_png, NULL); char buf0[2048]; gssize n0 = g_input_stream_read (in0, buf0, sizeof buf0, NULL, &err); GBytes *by = NULL;
    snprintf (m, sizeof m, "pixbuf: stream read n=%ld first=%02x %s png=%u bytes", (long) n0, n0 > 0 ? (unsigned char) buf0[0] : 0, err ? err->message : "-", (unsigned) sizeof phe_png); phe_log (m); g_clear_error (&err); g_object_unref (in0);
    in0 = g_memory_input_stream_new_from_data (phe_png, sizeof phe_png, NULL); by = g_input_stream_read_bytes (in0, 65536, NULL, &err);
    snprintf (m, sizeof m, "pixbuf: read_bytes size=%ld %s", by ? (long) g_bytes_get_size (by) : -1L, err ? err->message : "-"); phe_log (m); g_clear_error (&err); if (by) g_bytes_unref (by); g_object_unref (in0); }
  { GInputStream *in = g_memory_input_stream_new_from_data (phe_png, sizeof phe_png, NULL); GdkPixbuf *p2 = gdk_pixbuf_new_from_stream (in, NULL, &err);
    snprintf (m, sizeof m, "pixbuf: from_stream pixbuf=%p %s", (void *) p2, err ? err->message : "-"); phe_log (m); T (p2 && gdk_pixbuf_get_width (p2) == 8, "decode through a GInputStream"); if (p2) g_object_unref (p2); g_object_unref (in); g_clear_error (&err); }
  snprintf (m, sizeof m, "pixbuf: %d checks, %d failures", checks, fails); phe_log (m);
  return fails;
}
