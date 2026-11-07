/* phegdk.h -- the first skeleton of the "gdk-symbian" backend (docs/phorepoc/hildon-port.md section 3): a GdkWindow-like object is a cairo surface
 * over the pixel memory of a CFbsBitmap; the C++ side (phegdkwin.cpp) owns the RWindow and blits that bitmap. BSD-2-Clause (Phorepoc). */
#ifndef PHEGDK_H
#define PHEGDK_H
#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <pango/pangocairo.h>

typedef struct PheGdkWindow {
  int width, height, stride;
  unsigned char *pixels;          /* owned by the CFbsBitmap (EColor16MU = 0x00RRGGBB, the CAIRO_FORMAT_RGB24 layout) */
  cairo_surface_t *surface;
  cairo_t *cr;                    /* the window's drawing context while a paint is open */
} PheGdkWindow;

PheGdkWindow *phe_gdk_window_new (unsigned char *pixels, int width, int height, int stride);
cairo_t      *phe_gdk_window_begin_paint (PheGdkWindow *w);
void          phe_gdk_window_end_paint (PheGdkWindow *w);
void          phe_gdk_window_free (PheGdkWindow *w);
/* gdk_cairo_set_source_pixbuf, without GDK proper: copies a GdkPixbuf into a new ARGB32 surface */
cairo_surface_t *phe_gdk_pixbuf_surface (GdkPixbuf *pb);
/* the demo scene: background, a card with Pango text, a PNG picture decoded by gdk-pixbuf; returns a code for the log (0 = ok) */
int phe_gdk_demo_paint (PheGdkWindow *w);
#endif
