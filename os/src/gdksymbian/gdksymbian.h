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
void _gdk_symbian_window_hooks (void);
gboolean _gdk_symbian_expose_box (GdkRectangle *box);
void _gdk_symbian_stack_raise (GdkWindow *window);           /* gdkwindow-symbian.c: put a toplevel on top of its layer (and its transients above it) */
void _gdk_symbian_stack_lower (GdkWindow *window);
void _gdk_symbian_focus_update (void);
void _gdk_symbian_drag_begin (GdkWindow *window, gint mode, gint root_x, gint root_y);   /* gdkevents-symbian.c: interactive move (mode -1) or resize by edge (GdkWindowEdge) */
void _gdk_symbian_pointer_warp (GdkDisplay *display, gint x, gint y);
GdkWindow *_gdk_symbian_focus_window (void);                 /* the toplevel that holds the keyboard focus, or NULL */
void _gdk_symbian_inject_key (GdkWindow *window, guint keyval, guint state, gboolean press);                       /* gdk_test_simulate_key */
void _gdk_symbian_inject_button (GdkWindow *window, gint x, gint y, guint button, guint state, gboolean press);   /* gdk_test_simulate_button */                      /* gdkdrawable-symbian.c: wraps the window class's ref_cairo_surface so direct cairo drawing is presented */
void _gdk_symbian_screen_resized (GdkScreen *screen, int width, int height);


/* ---- drawables: every drawable of this backend is a cairo surface. A window is a rectangle of the display's bitmap surface (abs_x/abs_y), a pixmap owns its own surface ---- */
#define GDK_TYPE_DRAWABLE_IMPL_SYMBIAN (gdk_drawable_impl_symbian_get_type ())
#define GDK_DRAWABLE_IMPL_SYMBIAN(o)   (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_DRAWABLE_IMPL_SYMBIAN, GdkDrawableImplSymbian))
#define GDK_IS_DRAWABLE_IMPL_SYMBIAN(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_DRAWABLE_IMPL_SYMBIAN))
typedef struct _GdkDrawableImplSymbian {
  GdkDrawable parent_instance;
  GdkDrawable *wrapper;           /* the GdkWindow / GdkPixmap this is the implementation of */
  GdkColormap *colormap;
  cairo_surface_t *surface;       /* the backing surface: the shared display bitmap (windows) or a private surface (pixmaps) */
  cairo_surface_t *view;          /* cached ref_cairo_surface result: a sub-surface of `surface` for windows (key: view_key) */
  gint view_key[4];
  gint abs_x, abs_y;              /* origin inside `surface` (windows) */
  gint width, height, depth;
} GdkDrawableImplSymbian;
typedef struct { GdkDrawableClass parent_class; } GdkDrawableImplSymbianClass;
GType gdk_drawable_impl_symbian_get_type (void);

#define GDK_TYPE_PIXMAP_IMPL_SYMBIAN (gdk_pixmap_impl_symbian_get_type ())
typedef struct { GdkDrawableImplSymbian drawable; } GdkPixmapImplSymbian;
typedef struct { GdkDrawableImplSymbianClass parent_class; } GdkPixmapImplSymbianClass;
GType gdk_pixmap_impl_symbian_get_type (void);

#define GDK_TYPE_WINDOW_IMPL_SYMBIAN (gdk_window_impl_symbian_get_type ())
#define GDK_WINDOW_IMPL_SYMBIAN(o)   (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_WINDOW_IMPL_SYMBIAN, GdkWindowImplSymbian))
typedef struct _GdkWindowImplSymbian {
  GdkDrawableImplSymbian drawable;
  GdkCursor *cursor;
  GdkEventMask event_mask;
  gboolean mapped;
  GHashTable *properties;         /* window properties (gdkproperty-symbian.c) */
  /* window manager hints (gdkwinhints-symbian.c): there is no window manager, so this backend acts on them itself (stacking layers, transient stacking, focus choice, geometry limits, states) */
  GdkWindowTypeHint type_hint; GdkWindow *transient_for; GdkWMDecoration decorations; gboolean has_decorations; gchar *role, *startup_id;
  gchar *title, *icon_name; GdkWindow *group; GdkPixmap *icon_pixmap, *icon_mask; GdkWindow *icon_window; GdkWMFunction functions; gboolean has_functions; GdkGeometry geometry; GdkWindowHints geom_mask; GList *icon_list;
  gboolean skip_focus_once;       /* mapped with focus-on-map off: not given the keyboard focus until gdk_window_focus () asks for it */
  gboolean accept_focus, focus_on_map, override_redirect, modal, urgent, skip_taskbar, skip_pager;
  gint layer;                     /* -1 keep below, 0 normal, 1 keep above (modal and transient windows are kept above what they belong to) */
  gboolean saved_valid; gint save_x, save_y, save_w, save_h;      /* the geometry to return to from maximized / fullscreen */
  GdkRegion *shape;               /* window shape set by gdk_window_shape_combine_region: kept for GdkWindow bookkeeping, WSERV windows stay rectangular */
} GdkWindowImplSymbian;
typedef struct { GdkDrawableImplSymbianClass parent_class; } GdkWindowImplSymbianClass;
GType gdk_window_impl_symbian_get_type (void);

/* the display's bitmap as a cairo surface, and the visible toplevels in stacking order (bottom first) */
cairo_surface_t *_gdk_symbian_display_surface (void);
GList *_gdk_symbian_toplevel_stack (void);
void _gdk_symbian_present (GdkWindow *window, gint x, gint y, gint width, gint height);   /* window-relative rectangle -> WSERV */
extern GdkWindow *_gdk_root;
extern GdkWindow *_gdk_parent_root;

/* GC */
#define GDK_TYPE_GC_SYMBIAN (gdk_gc_symbian_get_type ())
GType gdk_gc_symbian_get_type (void);
void _gdk_symbian_gc_apply (GdkGC *gc, cairo_t *cr, gboolean for_fill);    /* sets source, line attributes, operator and clip of @gc on @cr */
void _gdk_symbian_gc_finish (GdkGC *gc, cairo_t *cr);
gboolean _gdk_symbian_gc_raster_mode (GdkGC *gc);                        /* function != COPY or OPAQUE_STIPPLED: needs the per-pixel path */
gboolean _gdk_symbian_gc_is_noop (GdkGC *gc);
GdkFunction _gdk_symbian_gc_function (GdkGC *gc);
void _gdk_symbian_gc_pixels (GdkGC *gc, guint32 *fg, guint32 *bg);
gboolean _gdk_symbian_gc_clip_test (GdkGC *gc, gint x, gint y);
guint32 _gdk_symbian_apply_function (GdkFunction f, guint32 s, guint32 d);
const cairo_user_data_key_t *_gdk_symbian_coverage_key (void);
void _gdk_symbian_raster_combine (GdkGC *gc, guchar *dst, gint dst_stride, gint depth, gint x0, gint y0, gint x1, gint y1, cairo_surface_t *cov);                     /* completes a drawing done between a group push of _gdk_symbian_gc_apply */

G_END_DECLS
#endif
