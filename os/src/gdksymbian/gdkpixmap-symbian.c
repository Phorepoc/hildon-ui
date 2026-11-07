/* gdkpixmap-symbian.c -- pixmaps and images of the Symbian backend. A GdkPixmap is a cairo image surface (RGB24 for 24 bit, ARGB32 for 32 bit, A8 for the 1 bit bitmaps / masks);
 * a GdkImage is client memory in the same pixel layouts. BSD-2-Clause (Phorepoc). */
#include "config.h"
#include <string.h>
#include "gdksymbian.h"
#include "gdkpixmap.h"
#include "gdkimage.h"
#include "gdkprivate.h"
#include "gdkinternals.h"

/* ---- pixmap implementation ---- */
G_DEFINE_TYPE (GdkPixmapImplSymbian, gdk_pixmap_impl_symbian, GDK_TYPE_DRAWABLE_IMPL_SYMBIAN)
static void gdk_pixmap_impl_symbian_init (GdkPixmapImplSymbian *p) { (void) p; }
static void gdk_pixmap_impl_symbian_class_init (GdkPixmapImplSymbianClass *c) { (void) c; }
GType _gdk_pixmap_impl_get_type (void) { return gdk_pixmap_impl_symbian_get_type (); }

/* native pixmap ids are the addresses of the pixmaps (everything lives in this process), registered while the pixmap exists */
static GHashTable *pixmap_registry = NULL;
static void pixmap_gone (gpointer data, GObject *where) { (void) data; g_hash_table_remove (pixmap_registry, where); }

GdkPixmap *
gdk_pixmap_lookup (GdkNativeWindow anid)
{
  GdkPixmap *p = pixmap_registry ? g_hash_table_lookup (pixmap_registry, (gpointer) (gsize) anid) : NULL;
  return p;
}
GdkPixmap *gdk_pixmap_lookup_for_display (GdkDisplay *display, GdkNativeWindow anid) { g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL); return gdk_pixmap_lookup (anid); }

/* a "foreign" pixmap of another client cannot exist in a single-process window system: an id names one of ours or nothing; the reference is the caller's */
GdkPixmap *gdk_pixmap_foreign_new (GdkNativeWindow anid) { GdkPixmap *p = gdk_pixmap_lookup (anid); return p ? g_object_ref (p) : NULL; }
GdkPixmap *gdk_pixmap_foreign_new_for_display (GdkDisplay *display, GdkNativeWindow anid) { g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL); return gdk_pixmap_foreign_new (anid); }
GdkPixmap *gdk_pixmap_foreign_new_for_screen (GdkScreen *screen, GdkNativeWindow anid, gint width, gint height, gint depth)
{ GdkPixmap *p; (void) width; (void) height; (void) depth; g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL); p = gdk_pixmap_lookup (anid); return p ? g_object_ref (p) : NULL; }

GdkPixmap *
_gdk_pixmap_new (GdkDrawable *drawable, gint width, gint height, gint depth)
{
  GdkPixmap *pixmap; GdkPixmapObject *priv; GdkDrawableImplSymbian *impl; cairo_format_t fmt;
  g_return_val_if_fail (drawable == NULL || GDK_IS_DRAWABLE (drawable), NULL);
  g_return_val_if_fail (drawable != NULL || depth != -1, NULL);
  g_return_val_if_fail (width > 0 && height > 0, NULL);
  if (!drawable) drawable = _gdk_parent_root;
  if (GDK_IS_WINDOW (drawable) && GDK_WINDOW_DESTROYED (drawable)) return NULL;
  if (depth == -1) depth = gdk_drawable_get_depth (GDK_DRAWABLE (drawable));
  switch (depth) {
    case 1: fmt = CAIRO_FORMAT_A8; break;
    case 24: fmt = CAIRO_FORMAT_RGB24; break;
    case 32: fmt = CAIRO_FORMAT_ARGB32; break;
    default: g_message ("gdk-symbian: pixmaps of depth %d are not supported (1, 24 and 32 are)", depth); return NULL;
  }
  pixmap = g_object_new (gdk_pixmap_get_type (), NULL); priv = GDK_PIXMAP_OBJECT (pixmap);
  if (!pixmap_registry) pixmap_registry = g_hash_table_new (g_direct_hash, g_direct_equal);
  g_hash_table_insert (pixmap_registry, pixmap, pixmap); g_object_weak_ref (G_OBJECT (pixmap), pixmap_gone, NULL);
  impl = g_object_new (_gdk_pixmap_impl_get_type (), NULL); priv->impl = GDK_DRAWABLE (impl);
  impl->wrapper = GDK_DRAWABLE (pixmap); impl->width = width; impl->height = height; impl->depth = depth;
  impl->surface = cairo_image_surface_create (fmt, width, height);
  priv->depth = depth;
  if (depth > 1) {
    GdkColormap *cm = gdk_drawable_get_colormap (drawable);
    gdk_drawable_set_colormap (pixmap, cm ? cm : gdk_screen_get_default_colormap (_gdk_screen));
  }
  return pixmap;
}

GdkPixmap *
_gdk_bitmap_create_from_data (GdkDrawable *drawable, const gchar *data, gint width, gint height)
{
  GdkPixmap *pixmap; GdkDrawableImplSymbian *impl; guchar *px; gint stride, x, y;
  g_return_val_if_fail (data != NULL, NULL); g_return_val_if_fail (width > 0 && height > 0, NULL);
  pixmap = _gdk_pixmap_new (drawable, width, height, 1);
  if (!pixmap) return NULL;
  impl = GDK_DRAWABLE_IMPL_SYMBIAN (GDK_PIXMAP_OBJECT (pixmap)->impl);
  cairo_surface_flush (impl->surface); px = cairo_image_surface_get_data (impl->surface); stride = cairo_image_surface_get_stride (impl->surface);
  for (y = 0; y < height; y++) for (x = 0; x < width; x++)      /* X bitmap data: rows padded to bytes, least significant bit first */
    px[y * stride + x] = (((const guchar *) data)[y * ((width + 7) / 8) + x / 8] >> (x % 8)) & 1 ? 0xff : 0x00;
  cairo_surface_mark_dirty (impl->surface);
  return pixmap;
}

GdkPixmap *
_gdk_pixmap_create_from_data (GdkDrawable *drawable, const gchar *data, gint width, gint height, gint depth, const GdkColor *fg, const GdkColor *bg)
{
  GdkPixmap *pixmap; GdkDrawableImplSymbian *impl; guchar *px; gint stride, x, y; GdkColormap *cm;
  g_return_val_if_fail (data != NULL, NULL); g_return_val_if_fail (drawable != NULL || depth != -1, NULL);
  g_return_val_if_fail (fg != NULL && bg != NULL, NULL); g_return_val_if_fail (width > 0 && height > 0, NULL);
  pixmap = _gdk_pixmap_new (drawable, width, height, depth);
  if (!pixmap) return NULL;
  impl = GDK_DRAWABLE_IMPL_SYMBIAN (GDK_PIXMAP_OBJECT (pixmap)->impl);
  cm = impl->colormap;
  cairo_surface_flush (impl->surface); px = cairo_image_surface_get_data (impl->surface); stride = cairo_image_surface_get_stride (impl->surface);
  for (y = 0; y < height; y++) for (x = 0; x < width; x++) {
    gboolean on = (((const guchar *) data)[y * ((width + 7) / 8) + x / 8] >> (x % 8)) & 1;
    const GdkColor *c = on ? fg : bg; guint32 pixel = c->pixel;
    if (cm) { GdkColor col = *c; gboolean ok; gdk_colormap_alloc_colors (cm, &col, 1, FALSE, TRUE, &ok); pixel = col.pixel; }
    if (impl->depth == 1) px[y * stride + x] = pixel & 1 ? 0xff : 0;
    else *(guint32 *) (px + y * stride + 4 * x) = pixel | (impl->depth == 32 ? 0xff000000u : 0);
  }
  cairo_surface_mark_dirty (impl->surface);
  return pixmap;
}

/* ---- GdkImage: client memory ---- */
G_DEFINE_TYPE (GdkImage, gdk_image, G_TYPE_OBJECT)
static void gdk_image_init (GdkImage *image) { (void) image; }
static void
gdk_image_finalize (GObject *object)
{
  GdkImage *image = GDK_IMAGE (object);
  g_free (image->mem);
  if (image->colormap) g_object_unref (image->colormap);
  G_OBJECT_CLASS (gdk_image_parent_class)->finalize (object);
}
static void gdk_image_class_init (GdkImageClass *c) { G_OBJECT_CLASS (c)->finalize = gdk_image_finalize; }

GdkImage *
_gdk_image_new_for_depth (GdkScreen *screen, GdkImageType type, GdkVisual *visual, gint width, gint height, gint depth)
{
  GdkImage *image;
  g_return_val_if_fail (!visual || GDK_IS_VISUAL (visual), NULL);
  g_return_val_if_fail (visual || depth != -1, NULL);
  g_return_val_if_fail (width > 0 && height > 0, NULL);
  (void) screen; (void) type;
  if (depth == -1) depth = visual->depth;
  if (depth != 1 && depth != 24 && depth != 32) { g_message ("gdk-symbian: images of depth %d are not supported (1, 24 and 32 are)", depth); return NULL; }
  image = g_object_new (gdk_image_get_type (), NULL);
  image->type = GDK_IMAGE_NORMAL; image->visual = visual; image->byte_order = GDK_LSB_FIRST;
  image->width = width; image->height = height; image->depth = depth;
  image->bpp = depth == 1 ? 1 : 4; image->bits_per_pixel = depth == 1 ? 8 : 32;
  image->bpl = (width * image->bpp + 3) & ~3;                        /* rows are 4-byte aligned: also what cairo needs for data surfaces */
  image->mem = g_malloc0 ((gsize) image->bpl * height);
  return image;
}

guint32
gdk_image_get_pixel (GdkImage *image, gint x, gint y)
{
  guchar *p;
  g_return_val_if_fail (GDK_IS_IMAGE (image), 0); g_return_val_if_fail (x >= 0 && x < image->width && y >= 0 && y < image->height, 0);
  p = (guchar *) image->mem + y * image->bpl + x * image->bpp;
  return image->bpp == 1 ? (p[0] ? 1 : 0) : *(guint32 *) p;
}

void
gdk_image_put_pixel (GdkImage *image, gint x, gint y, guint32 pixel)
{
  guchar *p;
  g_return_if_fail (GDK_IS_IMAGE (image)); g_return_if_fail (x >= 0 && x < image->width && y >= 0 && y < image->height);
  p = (guchar *) image->mem + y * image->bpl + x * image->bpp;
  if (image->bpp == 1) p[0] = pixel & 1 ? 0xff : 0; else *(guint32 *) p = pixel;
}
