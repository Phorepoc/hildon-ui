/* gdksymbian.h -- private header of the Symbian GDK backend (gdk/symbian), a real backend for GDK 2.24.33 on WSERV, modelled on the DirectFB one
 * (single display, single screen, client-side windows drawn with cairo into one bitmap that the WSERV thread shows). BSD-2-Clause (Phorepoc).
 * The WSERV side is the C ABI of phegdkwin.cpp (phe_ws_*). */
#ifndef GDKSYMBIAN_H
#define GDKSYMBIAN_H
#include <gdk/gdk.h>
#include <gdk/gdkprivate.h>
#include <gdk/gdkinternals.h>
#include <gdk/gdkdisplay.h>
#include <gdk/gdkscreen.h>
#include <cairo.h>

G_BEGIN_DECLS

/* ---- the C ABI of the WSERV thread (phegdkwin.cpp) ---- */
enum { PHE_EV_NONE = 0, PHE_EV_KEY = 1, PHE_EV_POINTER = 2, PHE_EV_REDRAW = 3 };
typedef struct { int type; int x, y, w, h; int code, scancode, ptype; } PheWsEvent;
int  phe_ws_init (unsigned char **pixels, int *width, int *height, int *stride);
int  phe_ws_poll (PheWsEvent *ev);
int  phe_ws_pending (void);
int  phe_ws_wake_fd (void);
void phe_ws_present (int x, int y, int w, int h);
void phe_ws_sync (void);                       /* returns when every queued BitBlt has been executed by the WSERV thread */
void phe_ws_beep (void);
void phe_log (const char *msg);

/* ---- classes ---- */
#define GDK_TYPE_DISPLAY_SYMBIAN (gdk_display_symbian_get_type ())
#define GDK_DISPLAY_SYMBIAN(o)   (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_DISPLAY_SYMBIAN, GdkDisplaySymbian))
#define GDK_TYPE_SCREEN_SYMBIAN  (gdk_screen_symbian_get_type ())
#define GDK_SCREEN_SYMBIAN(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_SCREEN_SYMBIAN, GdkScreenSymbian))

typedef struct _GdkDisplaySymbian GdkDisplaySymbian;
typedef struct _GdkDisplaySymbianClass { GdkDisplayClass parent_class; } GdkDisplaySymbianClass;
typedef struct _GdkScreenSymbian GdkScreenSymbian;
typedef struct _GdkScreenSymbianClass { GdkScreenClass parent_class; } GdkScreenSymbianClass;

struct _GdkDisplaySymbian {
  GdkDisplay parent_instance;
  GdkScreen *screen;
  unsigned char *pixels;          /* the CFbsBitmap pixel memory every window of this display draws into (EColor16MU = 0x00RRGGBB) */
  int stride;
  GSource *event_source;
  guint32 last_event_time;
  gulong serial;                  /* request serial counter (_gdk_windowing_window_get_next_serial) */
  /* pointer state, kept from the WSERV pointer events */
  gint pointer_x, pointer_y; guint pointer_state;
};

struct _GdkScreenSymbian {
  GdkScreen parent_instance;
  GdkDisplay *display;
  GdkVisual *visual;              /* the one visual: 24 bit TrueColor 0x00RRGGBB */
  GdkColormap *colormap;
  GdkWindow *root;
  gint width, height, width_mm, height_mm;
};

extern GdkDisplay *_gdk_display;  /* the single display, NULL until gdk_display_open */
extern GdkScreen  *_gdk_screen;
extern GdkVisual  *_gdk_visual;   /* == the screen's system visual */

GType gdk_display_symbian_get_type (void);
GType gdk_screen_symbian_get_type (void);

/* init order (gdkdisplay-symbian.c): visual, colormap, then the root window */
void _gdk_visual_init (GdkScreen *screen);
void _gdk_windowing_window_init (GdkScreen *screen);        /* gdkwindow-symbian.c: creates the root GdkWindow */
void _gdk_events_init (void);                               /* gdkevents-symbian.c: attaches the WSERV event source */
void _gdk_input_init (void);
void _gdk_symbian_screen_resized (GdkScreen *screen, int width, int height);

G_END_DECLS
#endif
