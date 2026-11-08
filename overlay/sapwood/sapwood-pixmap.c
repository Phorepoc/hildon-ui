/* GTK+ Sapwood Engine -- in-process image tiles for Phorepoc. LGPL-2.1-or-later (the engine's licence).
 * Replaces sapwood-pixmap.c of the upstream engine: that file asked the separate sapwood-server for X pixmaps over a socket. Here the engine loads the image itself (gdk-pixbuf), cuts
 * it at the borders into 3x3 tiles and draws them with cairo (tiled, as upstream: no scaling; 8-bit alpha works, the upstream engine had 1-bit). Shaped windows are not supported. */
#include <config.h>
#include "sapwood-pixmap.h"
#include <string.h>

struct _SapwoodPixmap {
  gint       width;
  gint       height;
  GdkPixbuf *tile[3][3];
};


SapwoodPixmap *
sapwood_pixmap_get_for_file (const char *filename, int bl, int br, int bt, int bb, GError **err)
{
  GdkPixbuf *pb = gdk_pixbuf_new_from_file (filename, err);
  SapwoodPixmap *self; gint w, h, xs[4], ys[4], i, j;
  if (!pb) return NULL;
  w = gdk_pixbuf_get_width (pb); h = gdk_pixbuf_get_height (pb);
  if (bl + br > w || bt + bb > h) {
    g_set_error (err, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED, "%s: borders (%d,%d,%d,%d) larger than the %dx%d image", filename, bl, br, bt, bb, w, h);
    g_object_unref (pb); return NULL;
  }
  self = g_new0 (SapwoodPixmap, 1); self->width = w; self->height = h;
  xs[0] = 0; xs[1] = bl; xs[2] = w - br; xs[3] = w;
  ys[0] = 0; ys[1] = bt; ys[2] = h - bb; ys[3] = h;
  for (i = 0; i < 3; i++)
    for (j = 0; j < 3; j++)
      if (xs[j + 1] > xs[j] && ys[i + 1] > ys[i]) {
        GdkPixbuf *sub = gdk_pixbuf_new_subpixbuf (pb, xs[j], ys[i], xs[j + 1] - xs[j], ys[i + 1] - ys[i]);
        self->tile[i][j] = gdk_pixbuf_copy (sub);           /* own pixels: the tile is a cairo source of its own */
        g_object_unref (sub);
      }
  g_object_unref (pb);
  return self;
}

void
sapwood_pixmap_free (SapwoodPixmap *self)
{
  gint i, j;
  if (!self) return;
  for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) if (self->tile[i][j]) g_object_unref (self->tile[i][j]);
  g_free (self);
}

gboolean
sapwood_pixmap_get_geometry (SapwoodPixmap *self, gint *width, gint *height)
{
  if (!self) return FALSE;
  if (width) *width = self->width;
  if (height) *height = self->height;
  return TRUE;
}

void
sapwood_pixmap_get_pixmap (SapwoodPixmap *self, gint x, gint y, gpointer *pixmap, gpointer *pixmask)
{
  *pixmap = self->tile[y][x];
  *pixmask = NULL;
}

void
sapwood_pixmap_render_rects (SapwoodPixmap *self, GtkWidget *widget, GdkDrawable *draw, gint draw_x, gint draw_y, gint width, gint height,
                             GdkBitmap *mask, gint mask_x, gint mask_y, gboolean mask_required, GdkRectangle *clip_rect, gint n_rect, SapwoodRect *rect)
{
  cairo_t *cr; gint n;
  (void) self; (void) widget; (void) draw_x; (void) draw_y; (void) width; (void) height; (void) mask; (void) mask_x; (void) mask_y; (void) mask_required;
  cr = gdk_cairo_create (draw);
  for (n = 0; n < n_rect; n++) {
    GdkRectangle *dest = &rect[n].dest, area;
    cairo_pattern_t *pat;
    if (!rect[n].pixmap) continue;
    if (clip_rect) { if (!gdk_rectangle_intersect (dest, clip_rect, &area)) continue; } else area = *dest;
    cairo_save (cr);
    cairo_rectangle (cr, area.x, area.y, area.width, area.height); cairo_clip (cr);
    gdk_cairo_set_source_pixbuf (cr, GDK_PIXBUF (rect[n].pixmap), dest->x, dest->y);
    pat = cairo_get_source (cr);
    cairo_pattern_set_extend (pat, CAIRO_EXTEND_REPEAT); cairo_pattern_set_filter (pat, CAIRO_FILTER_NEAREST);
    cairo_paint (cr);
    cairo_restore (cr);
  }
  cairo_destroy (cr);
}
