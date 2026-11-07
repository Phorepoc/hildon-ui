/* gdkcursor-symbian.c -- cursors of the Symbian backend. WSERV draws the pointer (a software cursor on the devices that have a mouse, none on a touch phone), so a GdkCursor
 * is a typed handle: it records the standard cursor type for gdk_cursor_get_cursor_type and for window cursor bookkeeping. BSD-2-Clause (Phorepoc). */
#include "config.h"
#include "gdksymbian.h"
#include "gdkcursor.h"
#include "gdkprivate.h"
#include "gdkinternals.h"

typedef struct { GdkCursor cursor; GdkDisplay *display; } GdkCursorSymbian;

GdkCursor *
gdk_cursor_new_for_display (GdkDisplay *display, GdkCursorType cursor_type)
{
  GdkCursorSymbian *c;
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  c = g_new0 (GdkCursorSymbian, 1);
  c->cursor.type = cursor_type; c->cursor.ref_count = 1; c->display = display;
  return (GdkCursor *) c;
}

GdkCursor *
gdk_cursor_new_from_pixmap (GdkPixmap *source, GdkPixmap *mask, const GdkColor *fg, const GdkColor *bg, gint x, gint y)
{
  g_return_val_if_fail (GDK_IS_PIXMAP (source), NULL); g_return_val_if_fail (GDK_IS_PIXMAP (mask), NULL);
  (void) fg; (void) bg; (void) x; (void) y;
  return gdk_cursor_new_for_display (_gdk_display, GDK_CURSOR_IS_PIXMAP);           /* the image is not shown: WSERV owns the pointer drawing */
}

GdkCursor *
gdk_cursor_new_from_pixbuf (GdkDisplay *display, GdkPixbuf *pixbuf, gint x, gint y)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL); g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);
  (void) x; (void) y;
  return gdk_cursor_new_for_display (display, GDK_CURSOR_IS_PIXMAP);
}

GdkCursor *gdk_cursor_new_from_name (GdkDisplay *display, const gchar *name) { g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL); (void) name; return NULL; }   /* no cursor theme: NULL is the documented "no such name" */
GdkDisplay *gdk_cursor_get_display (GdkCursor *cursor) { g_return_val_if_fail (cursor != NULL, NULL); return ((GdkCursorSymbian *) cursor)->display; }
GdkPixbuf *gdk_cursor_get_image (GdkCursor *cursor) { g_return_val_if_fail (cursor != NULL, NULL); return NULL; }                      /* no image to give */
void _gdk_cursor_destroy (GdkCursor *cursor) { g_return_if_fail (cursor != NULL); g_free (cursor); }
