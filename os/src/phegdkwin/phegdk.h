/* phegdk.h -- the gdk-symbian skeleton (docs/phorepoc/hildon-port.md sections 3 and 7.8). BSD-2-Clause (Phorepoc).
 *
 * GObject classes shaped like GDK 2.24's (GdkDisplay / GdkScreen / GdkWindow); the real GDK vtables come later, these are the pieces they will be
 * rebased on: PheGdkDisplay owns the GLib main-loop source for the WSERV request statuses, PheGdkScreen the geometry, PheGdkWindow a cairo
 * surface over the pixel memory of the CFbsBitmap that the C++ side (phegdkwin.cpp) shows in an RWindow, plus a dirty rectangle and process_updates. */
#ifndef PHEGDK_H
#define PHEGDK_H
#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <pango/pangocairo.h>

/* ---- the C ABI of the C++ WSERV half (phegdkwin.cpp) ---- */
enum { PHE_EV_NONE = 0, PHE_EV_KEY = 1, PHE_EV_POINTER = 2, PHE_EV_REDRAW = 3 };
typedef struct { int type; int x, y, w, h; int code, scancode, ptype; } PheWsEvent;
int  phe_ws_init (unsigned char **pixels, int *width, int *height, int *stride);   /* connect, window, bitmap; 0 = ok */
int  phe_ws_poll (PheWsEvent *ev);                                                  /* NON-blocking: 1 = an event/redraw was fetched and its request re-armed */
void phe_ws_present (int x, int y, int w, int h);                                   /* BitBlt of the bitmap rectangle into the window */
void phe_log (const char *msg);

/* ---- classes ---- */
#define PHE_TYPE_GDK_DISPLAY (phe_gdk_display_get_type ())
#define PHE_TYPE_GDK_SCREEN  (phe_gdk_screen_get_type ())
#define PHE_TYPE_GDK_WINDOW  (phe_gdk_window_get_type ())
typedef struct _PheGdkDisplay PheGdkDisplay;
typedef struct _PheGdkScreen PheGdkScreen;
typedef struct _PheGdkWindow PheGdkWindow;
typedef void (*PheGdkEventFunc) (const PheWsEvent *ev, gpointer data);
typedef void (*PheGdkExposeFunc) (PheGdkWindow *w, cairo_t *cr, const cairo_rectangle_int_t *area, gpointer data);

struct _PheGdkScreen { GObject parent; PheGdkDisplay *display; int width, height; };
struct _PheGdkDisplay { GObject parent; PheGdkScreen *screen; unsigned char *pixels; int stride; GSource *source; PheGdkEventFunc event_func; gpointer event_data; GList *windows; };
struct _PheGdkWindow {
  GObject parent; PheGdkScreen *screen;
  int x, y, width, height;
  cairo_surface_t *surface; cairo_t *cr;
  gboolean dirty; cairo_rectangle_int_t dirty_rect;     /* union of the invalidated rectangles */
  PheGdkExposeFunc expose_func; gpointer expose_data;
  int updates, last_present_w, last_present_h;
};

typedef struct { GObjectClass parent_class; } PheGdkScreenClass;
typedef struct { GObjectClass parent_class; } PheGdkDisplayClass;
typedef struct { GObjectClass parent_class; } PheGdkWindowClass;

GType           phe_gdk_display_get_type (void);
GType           phe_gdk_screen_get_type (void);
GType           phe_gdk_window_get_type (void);
PheGdkDisplay  *phe_gdk_display_open (void);                                        /* WSERV connect + screen + main-loop source (attached to the default context) */
PheGdkScreen   *phe_gdk_display_get_screen (PheGdkDisplay *d);
void            phe_gdk_display_set_event_func (PheGdkDisplay *d, PheGdkEventFunc f, gpointer data);
PheGdkWindow   *phe_gdk_window_new_toplevel (PheGdkDisplay *d);                      /* full-screen toplevel on the bitmap */
void            phe_gdk_window_set_expose_func (PheGdkWindow *w, PheGdkExposeFunc f, gpointer data);
void            phe_gdk_window_invalidate_rect (PheGdkWindow *w, int x, int y, int width, int height);
void            phe_gdk_window_process_updates (PheGdkWindow *w);                    /* expose the dirty rectangle, then BitBlt exactly that rectangle */
cairo_surface_t *phe_gdk_pixbuf_surface (GdkPixbuf *pb);                              /* gdk_cairo_set_source_pixbuf without GDK proper */

/* the demo (phegdkwin_demo.c) */
void phe_gdk_demo_run (PheGdkDisplay *d, PheGdkWindow *w);
#endif
