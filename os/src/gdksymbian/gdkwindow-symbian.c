/* gdkwindow-symbian.c -- windows of the Symbian backend. Only the root window and toplevels (and windows made native on request) have an implementation; every other window is
 * client-side and drawn by the GDK core into the toplevel's drawable. A native window is a rectangle of the display bitmap (the surface WSERV shows through the WSERV thread):
 * showing, hiding, moving and stacking toplevels is bookkeeping on that rectangle plus exposing what was uncovered. BSD-2-Clause (Phorepoc). */
#include "config.h"
#include <string.h>
#include "gdksymbian.h"
#include "gdkwindow.h"
#include "gdkwindowimpl.h"
#include "gdkprivate.h"
#include "gdkinternals.h"
#include "gdkregion.h"
#include "gdkpixmap.h"

GdkWindow *_gdk_root = NULL;
GdkWindow *_gdk_parent_root = NULL;
static cairo_surface_t *display_surface = NULL;
static GList *toplevel_stack = NULL;                 /* mapped native toplevels, bottom first */

cairo_surface_t *_gdk_symbian_display_surface (void) { return display_surface; }
GList *_gdk_symbian_toplevel_stack (void) { return toplevel_stack; }

typedef struct { gboolean dirty; GdkRectangle rect; } DirtyInfo;
#define WIMPL(w) ((GdkWindowImplSymbian *) GDK_WINDOW_OBJECT (w)->impl)
void _gdk_symbian_flush_dirty (void);

/* ---- native window ids: the id of a window is its pointer (it fits 32 bits on this ABI); used for client messages, selections and drag and drop ---- */
static GHashTable *window_table = NULL;
static void register_window (GdkWindow *w) { if (!window_table) window_table = g_hash_table_new (g_direct_hash, g_direct_equal); g_hash_table_insert (window_table, GUINT_TO_POINTER ((guint32) (gsize) w), w); }
static void unregister_window (GdkWindow *w) { if (window_table) g_hash_table_remove (window_table, GUINT_TO_POINTER ((guint32) (gsize) w)); }
GdkWindow *gdk_window_lookup_for_display (GdkDisplay *display, GdkNativeWindow anid) { (void) display; return window_table ? g_hash_table_lookup (window_table, GUINT_TO_POINTER ((guint32) anid)) : NULL; }
GdkWindow *gdk_window_lookup (GdkNativeWindow anid) { return gdk_window_lookup_for_display (NULL, anid); }

/* ---- the implementation class ---- */
static void gdk_window_impl_symbian_iface_init (GdkWindowImplIface *iface);
G_DEFINE_TYPE_WITH_CODE (GdkWindowImplSymbian, gdk_window_impl_symbian, GDK_TYPE_DRAWABLE_IMPL_SYMBIAN,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_WINDOW_IMPL, gdk_window_impl_symbian_iface_init))

static GHashTable *dirty_table = NULL;               /* GdkWindow* -> DirtyInfo* */

static void gdk_window_impl_symbian_init (GdkWindowImplSymbian *impl) { impl->event_mask = 0; }

static void
gdk_window_impl_symbian_finalize (GObject *object)
{
  GdkWindowImplSymbian *impl = (GdkWindowImplSymbian *) object;
  if (impl->cursor) gdk_cursor_unref (impl->cursor);
  if (impl->shape) gdk_region_destroy (impl->shape);
  G_OBJECT_CLASS (gdk_window_impl_symbian_parent_class)->finalize (object);
}
static void gdk_window_impl_symbian_class_init (GdkWindowImplSymbianClass *c) { G_OBJECT_CLASS (c)->finalize = gdk_window_impl_symbian_finalize; }

/* ---- dirty rectangle bookkeeping (see gdkdrawable-symbian.c) ---- */
gboolean
_gdk_symbian_window_dirty (GdkWindow *win, GdkRectangle *r, gboolean add)
{
  DirtyInfo *d;
  if (!dirty_table) dirty_table = g_hash_table_new (g_direct_hash, g_direct_equal);
  d = g_hash_table_lookup (dirty_table, win);
  if (add) {
    gboolean was;
    if (!d) { d = g_new0 (DirtyInfo, 1); g_hash_table_insert (dirty_table, win, d); }
    was = d->dirty;
    if (!d->dirty) { d->rect = *r; d->dirty = TRUE; } else gdk_rectangle_union (&d->rect, r, &d->rect);
    return !was;
  }
  if (!d || !d->dirty) return FALSE;
  *r = d->rect; d->dirty = FALSE; return TRUE;
}

/* a window-relative rectangle of a native window -> the WSERV thread */
void
_gdk_symbian_present (GdkWindow *window, gint x, gint y, gint width, gint height)
{
  GdkWindowImplSymbian *impl = WIMPL (window); GdkScreen *screen = _gdk_screen;
  gint ax = impl->drawable.abs_x + x, ay = impl->drawable.abs_y + y, x2 = ax + width, y2 = ay + height;
  if (!impl->mapped && GDK_WINDOW_OBJECT (window)->window_type != GDK_WINDOW_ROOT) return;
  ax = MAX (ax, 0); ay = MAX (ay, 0); x2 = MIN (x2, gdk_screen_get_width (screen)); y2 = MIN (y2, gdk_screen_get_height (screen));
  if (x2 > ax && y2 > ay) phe_ws_present (ax, ay, x2 - ax, y2 - ay);
}

/* ---- the root window ---- */
void
_gdk_windowing_window_init (GdkScreen *screen)
{
  GdkWindowObject *private; GdkWindowImplSymbian *impl; GdkDisplaySymbian *d = GDK_DISPLAY_SYMBIAN (_gdk_display);
  gint w = gdk_screen_get_width (screen), h = gdk_screen_get_height (screen);
  g_assert (_gdk_parent_root == NULL);
  display_surface = cairo_image_surface_create_for_data (d->pixels, CAIRO_FORMAT_RGB24, w, h, d->stride);
  _gdk_parent_root = g_object_new (GDK_TYPE_WINDOW, NULL);
  _gdk_root = _gdk_parent_root;
  private = GDK_WINDOW_OBJECT (_gdk_parent_root);
  impl = g_object_new (GDK_TYPE_WINDOW_IMPL_SYMBIAN, NULL);
  private->impl = GDK_DRAWABLE (impl); private->impl_window = private;
  impl->drawable.wrapper = GDK_DRAWABLE (private); impl->drawable.surface = cairo_surface_reference (display_surface);
  impl->drawable.abs_x = impl->drawable.abs_y = 0; impl->drawable.width = w; impl->drawable.height = h; impl->drawable.depth = 24;
  impl->drawable.colormap = g_object_ref (gdk_screen_get_system_colormap (screen));
  impl->mapped = TRUE;
  private->window_type = GDK_WINDOW_ROOT; private->viewable = TRUE; private->x = private->y = 0; private->abs_x = private->abs_y = 0;
  private->width = w; private->height = h; private->depth = 24; private->state = 0;
  private->input_only = FALSE;
  ((GdkScreenSymbian *) screen)->root = _gdk_parent_root; register_window (_gdk_parent_root);
  { cairo_t *cr = cairo_create (display_surface); cairo_set_source_rgb (cr, 0, 0, 0); cairo_paint (cr); cairo_destroy (cr); }    /* black root background */
}

/* ---- new native windows ---- */
void
_gdk_window_impl_new (GdkWindow *window, GdkWindow *real_parent, GdkScreen *screen, GdkVisual *visual, GdkEventMask event_mask, GdkWindowAttr *attributes, gint attributes_mask)
{
  GdkWindowObject *private = GDK_WINDOW_OBJECT (window), *parent_private = GDK_WINDOW_OBJECT (real_parent);
  GdkWindowImplSymbian *impl, *parent_impl = (GdkWindowImplSymbian *) parent_private->impl; GdkColormap *colormap;
  (void) screen;
  if (attributes->wclass != GDK_INPUT_OUTPUT && attributes->wclass != GDK_INPUT_ONLY) { g_warning ("_gdk_window_impl_new: unsupported window class"); _gdk_window_destroy (window, FALSE); return; }
  switch (private->window_type) {
    case GDK_WINDOW_TOPLEVEL: case GDK_WINDOW_DIALOG: case GDK_WINDOW_TEMP: case GDK_WINDOW_CHILD: break;
    default: g_warning ("_gdk_window_impl_new: unsupported window type %d", private->window_type); _gdk_window_destroy (window, FALSE); return;
  }
  impl = g_object_new (GDK_TYPE_WINDOW_IMPL_SYMBIAN, NULL);
  private->impl = GDK_DRAWABLE (impl); register_window (window);
  impl->drawable.wrapper = GDK_DRAWABLE (window); impl->drawable.surface = cairo_surface_reference (display_surface);
  impl->drawable.width = MAX (1, attributes->width); impl->drawable.height = MAX (1, attributes->height); impl->drawable.depth = 24;
  impl->drawable.abs_x = private->window_type == GDK_WINDOW_CHILD ? parent_impl->drawable.abs_x + private->x : private->x;
  impl->drawable.abs_y = private->window_type == GDK_WINDOW_CHILD ? parent_impl->drawable.abs_y + private->y : private->y;
  impl->event_mask = event_mask;
  if ((attributes_mask & GDK_WA_COLORMAP) && attributes->colormap) colormap = attributes->colormap;
  else colormap = gdk_screen_get_system_colormap (_gdk_screen);
  gdk_drawable_set_colormap (GDK_DRAWABLE (window), colormap);
  (void) visual;
  if ((attributes_mask & GDK_WA_CURSOR) && attributes->cursor) gdk_window_set_cursor (window, attributes->cursor);
}

/* ---- repainting what a toplevel uncovered: the root background, then the windows below ---- */
static void
repaint_area (gint x, gint y, gint w, gint h, GdkWindow *except)
{
  GList *l; cairo_t *cr = cairo_create (display_surface);
  cairo_rectangle (cr, x, y, w, h); cairo_set_source_rgb (cr, 0, 0, 0); cairo_fill (cr); cairo_destroy (cr);
  for (l = toplevel_stack; l; l = l->next) {
    GdkWindowImplSymbian *wi = WIMPL (l->data); GdkRectangle a = { x, y, w, h }, b = { wi->drawable.abs_x, wi->drawable.abs_y, wi->drawable.width, wi->drawable.height }, i;
    if (l->data == except || !gdk_rectangle_intersect (&a, &b, &i)) continue;
    i.x -= b.x; i.y -= b.y; gdk_window_invalidate_rect (l->data, &i, TRUE);
  }
  phe_ws_present (MAX (x, 0), MAX (y, 0), w, h);
}

static gboolean
expose_idle (gpointer data)
{
  GdkWindow *w = data;
  if (!GDK_WINDOW_DESTROYED (w) && WIMPL (w)->mapped) gdk_window_invalidate_rect (w, NULL, TRUE);
  g_object_unref (w);
  return FALSE;
}

/* ---- GdkWindowImplIface ---- */
static void
impl_show (GdkWindow *window, gboolean already_mapped)
{
  GdkWindowImplSymbian *impl = WIMPL (window);
  (void) already_mapped;
  impl->mapped = TRUE;
  if (GDK_WINDOW_OBJECT (window)->window_type != GDK_WINDOW_CHILD) {
    toplevel_stack = g_list_remove (toplevel_stack, window); toplevel_stack = g_list_append (toplevel_stack, window);
  }
  gdk_threads_add_idle_full (GDK_PRIORITY_REDRAW - 1, expose_idle, g_object_ref (window), NULL);     /* a newly mapped window gets its first expose */
}

static void
impl_hide (GdkWindow *window)
{
  GdkWindowImplSymbian *impl = WIMPL (window); gint x = impl->drawable.abs_x, y = impl->drawable.abs_y, w = impl->drawable.width, h = impl->drawable.height;
  impl->mapped = FALSE;
  if (g_list_find (toplevel_stack, window)) { toplevel_stack = g_list_remove (toplevel_stack, window); repaint_area (x, y, w, h, window); }
}
static void impl_withdraw (GdkWindow *window) { impl_hide (window); }

static void
impl_raise (GdkWindow *window)
{
  GdkWindowImplSymbian *impl = WIMPL (window);
  if (!g_list_find (toplevel_stack, window)) return;
  toplevel_stack = g_list_remove (toplevel_stack, window); toplevel_stack = g_list_append (toplevel_stack, window);
  if (impl->mapped) gdk_window_invalidate_rect (window, NULL, TRUE);
}

static void
impl_lower (GdkWindow *window)
{
  GdkWindowImplSymbian *impl = WIMPL (window);
  if (!g_list_find (toplevel_stack, window)) return;
  toplevel_stack = g_list_remove (toplevel_stack, window); toplevel_stack = g_list_prepend (toplevel_stack, window);
  repaint_area (impl->drawable.abs_x, impl->drawable.abs_y, impl->drawable.width, impl->drawable.height, NULL);    /* whatever was below shows again, in stack order */
}

/* native child windows are not stacked among their siblings: the GDK core paints non-native windows in order, and a native child shares the bitmap */
static void impl_restack_under (GdkWindow *window, GList *native_siblings) { (void) window; (void) native_siblings; }

static void
impl_restack_toplevel (GdkWindow *window, GdkWindow *sibling, gboolean above)
{
  GList *sl;
  if (!g_list_find (toplevel_stack, window)) return;
  toplevel_stack = g_list_remove (toplevel_stack, window);
  sl = g_list_find (toplevel_stack, sibling);
  if (!sl) toplevel_stack = above ? g_list_append (toplevel_stack, window) : g_list_prepend (toplevel_stack, window);
  else toplevel_stack = g_list_insert_before (toplevel_stack, above ? sl->next : sl, window);
  if (WIMPL (window)->mapped) gdk_window_invalidate_rect (window, NULL, TRUE);
}

static void
impl_move_resize (GdkWindow *window, gboolean with_move, gint x, gint y, gint width, gint height)
{
  GdkWindowObject *private = GDK_WINDOW_OBJECT (window); GdkWindowImplSymbian *impl = WIMPL (window);
  gint ox = impl->drawable.abs_x, oy = impl->drawable.abs_y, ow = impl->drawable.width, oh = impl->drawable.height;
  gboolean toplevel = private->parent == NULL || GDK_WINDOW_TYPE (private->parent) == GDK_WINDOW_ROOT;
  if (with_move) {
    private->x = x; private->y = y;
    if (toplevel) { impl->drawable.abs_x = x; impl->drawable.abs_y = y; }
    else { GdkWindowImplSymbian *pi = (GdkWindowImplSymbian *) private->parent->impl; impl->drawable.abs_x = pi->drawable.abs_x + x; impl->drawable.abs_y = pi->drawable.abs_y + y; }
  }
  if (!(width < 0 && height < 0)) {
    private->width = impl->drawable.width = MAX (1, width); private->height = impl->drawable.height = MAX (1, height);
  }
  if (impl->mapped && toplevel && (ox != impl->drawable.abs_x || oy != impl->drawable.abs_y || ow != impl->drawable.width || oh != impl->drawable.height))
    repaint_area (ox, oy, ow, oh, window);       /* what the old rectangle covered; the core exposes the new/enlarged area of this window */
  if (with_move || !(width < 0 && height < 0)) {
    GdkEvent *ev = gdk_event_new (GDK_CONFIGURE);
    ev->configure.window = g_object_ref (window); ev->configure.send_event = TRUE;
    ev->configure.x = private->x; ev->configure.y = private->y; ev->configure.width = private->width; ev->configure.height = private->height;
    gdk_event_put (ev); gdk_event_free (ev);
  }
}

/* the background is painted by the core (supports_native_bg is FALSE): nothing to set natively */
static void impl_set_background (GdkWindow *window, const GdkColor *color) { (void) window; (void) color; }
static void impl_set_back_pixmap (GdkWindow *window, GdkPixmap *pixmap) { (void) window; (void) pixmap; }
static GdkEventMask impl_get_events (GdkWindow *window) { return WIMPL (window)->event_mask; }
static void impl_set_events (GdkWindow *window, GdkEventMask mask) { WIMPL (window)->event_mask = mask; }

/* reparenting changes the window server hierarchy: here only toplevel <-> child of another native window is meaningful, and it is a change of the origin */
static gboolean
impl_reparent (GdkWindow *window, GdkWindow *new_parent, gint x, gint y)
{
  GdkWindowObject *private = GDK_WINDOW_OBJECT (window), *pp = GDK_WINDOW_OBJECT (new_parent); GdkWindowImplSymbian *impl = WIMPL (window);
  gboolean root = GDK_WINDOW_TYPE (new_parent) == GDK_WINDOW_ROOT;
  if (root) { impl->drawable.abs_x = x; impl->drawable.abs_y = y; }
  else { GdkWindowImplSymbian *pi = (GdkWindowImplSymbian *) pp->impl; impl->drawable.abs_x = pi->drawable.abs_x + x; impl->drawable.abs_y = pi->drawable.abs_y + y; }
  private->x = x; private->y = y;
  return root;                       /* TRUE: the window is now a toplevel */
}

static void
impl_clear_region (GdkWindow *window, GdkRegion *region, gboolean send_expose)
{
  /* the core clears client-side (the background is not native): this is only reached for native windows, whose clearing is the expose of the region */
  if (send_expose) gdk_window_invalidate_region (window, region, FALSE);
}

static void impl_set_cursor (GdkWindow *window, GdkCursor *cursor)
{
  GdkWindowImplSymbian *impl = WIMPL (window);
  if (cursor) gdk_cursor_ref (cursor);
  if (impl->cursor) gdk_cursor_unref (impl->cursor);
  impl->cursor = cursor;
}

static void
impl_get_geometry (GdkWindow *window, gint *x, gint *y, gint *width, gint *height, gint *depth)
{
  GdkWindowObject *private = GDK_WINDOW_OBJECT (window);
  if (GDK_WINDOW_DESTROYED (window)) return;
  if (x) *x = private->x; if (y) *y = private->y; if (width) *width = private->width; if (height) *height = private->height; if (depth) *depth = private->depth;
}

static gint
impl_get_root_coords (GdkWindow *window, gint x, gint y, gint *root_x, gint *root_y)
{
  GdkWindowImplSymbian *impl = WIMPL (window);
  if (root_x) *root_x = x + impl->drawable.abs_x; if (root_y) *root_y = y + impl->drawable.abs_y; return TRUE;
}
static gint impl_get_deskrelative_origin (GdkWindow *window, gint *x, gint *y) { return impl_get_root_coords (window, 0, 0, x, y); }

static gboolean
impl_get_pointer (GdkWindow *window, gint *x, gint *y, GdkModifierType *mask)
{
  GdkDisplaySymbian *d = GDK_DISPLAY_SYMBIAN (_gdk_display); GdkWindowImplSymbian *impl = WIMPL (window);
  if (x) *x = d->pointer_x - impl->drawable.abs_x; if (y) *y = d->pointer_y - impl->drawable.abs_y; if (mask) *mask = d->pointer_state;
  return d->pointer_x >= impl->drawable.abs_x && d->pointer_y >= impl->drawable.abs_y && d->pointer_x < impl->drawable.abs_x + impl->drawable.width && d->pointer_y < impl->drawable.abs_y + impl->drawable.height;
}

/* WSERV windows are rectangular: a shape is kept (for gdk_window_get_shape bookkeeping) but cannot change what the server shows */
static void
impl_shape_combine_region (GdkWindow *window, const GdkRegion *shape, gint ox, gint oy)
{
  GdkWindowImplSymbian *impl = WIMPL (window);
  if (impl->shape) { gdk_region_destroy (impl->shape); impl->shape = NULL; }
  if (shape) { impl->shape = gdk_region_copy (shape); gdk_region_offset (impl->shape, ox, oy); }
}
static void impl_input_shape_combine_region (GdkWindow *window, const GdkRegion *shape, gint ox, gint oy) { (void) window; (void) shape; (void) ox; (void) oy; }
static gboolean impl_set_static_gravities (GdkWindow *window, gboolean use_static) { (void) window; (void) use_static; return FALSE; }   /* not supported: the core then does the moves itself */
static gboolean impl_queue_antiexpose (GdkWindow *window, GdkRegion *update_area) { (void) window; (void) update_area; return FALSE; }       /* no server-side expose to avoid */
static void impl_queue_translation (GdkWindow *window, GdkGC *gc, GdkRegion *area, gint dx, gint dy) { (void) window; (void) gc; (void) area; (void) dx; (void) dy; }   /* copies are synchronous */

static void
impl_destroy (GdkWindow *window, gboolean recursing, gboolean foreign_destroy)
{
  GdkWindowImplSymbian *impl = WIMPL (window); gint x = impl->drawable.abs_x, y = impl->drawable.abs_y, w = impl->drawable.width, h = impl->drawable.height; gboolean was = g_list_find (toplevel_stack, window) != NULL;
  (void) foreign_destroy;
  toplevel_stack = g_list_remove (toplevel_stack, window); unregister_window (window);
  if (dirty_table) { gpointer d = g_hash_table_lookup (dirty_table, window); if (d) { g_hash_table_remove (dirty_table, window); g_free (d); } }
  if (was && !recursing) repaint_area (x, y, w, h, window);
  impl->mapped = FALSE;
}
static void impl_input_window_destroy (GdkWindow *window) { (void) window; }
static void impl_input_window_crossing (GdkWindow *window, gboolean enter) { (void) window; (void) enter; }

static void
gdk_window_impl_symbian_iface_init (GdkWindowImplIface *iface)
{
  iface->show = impl_show; iface->hide = impl_hide; iface->withdraw = impl_withdraw; iface->raise = impl_raise; iface->lower = impl_lower;
  iface->restack_under = impl_restack_under; iface->restack_toplevel = impl_restack_toplevel; iface->move_resize = impl_move_resize;
  iface->set_background = impl_set_background; iface->set_back_pixmap = impl_set_back_pixmap; iface->get_events = impl_get_events; iface->set_events = impl_set_events;
  iface->reparent = impl_reparent; iface->clear_region = impl_clear_region; iface->set_cursor = impl_set_cursor; iface->get_geometry = impl_get_geometry;
  iface->get_root_coords = impl_get_root_coords; iface->get_deskrelative_origin = impl_get_deskrelative_origin; iface->get_pointer = impl_get_pointer;
  iface->shape_combine_region = impl_shape_combine_region; iface->input_shape_combine_region = impl_input_shape_combine_region;
  iface->set_static_gravities = impl_set_static_gravities; iface->queue_antiexpose = impl_queue_antiexpose; iface->queue_translation = impl_queue_translation;
  iface->destroy = impl_destroy; iface->input_window_destroy = impl_input_window_destroy; iface->input_window_crossing = impl_input_window_crossing;
  iface->supports_native_bg = FALSE;
}

/* ---- _gdk_windowing_* of gdkinternals.h that concern windows ---- */
void _gdk_windowing_before_process_all_updates (void) { }
void _gdk_windowing_after_process_all_updates (void) { _gdk_symbian_flush_dirty (); }       /* everything painted in this update cycle goes to the window server now */
void _gdk_windowing_window_process_updates_recurse (GdkWindow *window, GdkRegion *region) { _gdk_window_process_updates_recurse (window, region); }
void _gdk_windowing_window_beep (GdkWindow *window) { (void) window; gdk_display_beep (_gdk_display); }
void _gdk_windowing_window_destroy_foreign (GdkWindow *window) { (void) window; }          /* there are no foreign windows */
GdkWindow *gdk_window_foreign_new_for_display (GdkDisplay *display, GdkNativeWindow anid) { (void) display; (void) anid; return NULL; }   /* a window of another process cannot be wrapped here */
GdkRegion *_gdk_windowing_window_get_shape (GdkWindow *window) { GdkWindowImplSymbian *impl = WIMPL (window); return impl->shape ? gdk_region_copy (impl->shape) : NULL; }
GdkRegion *_gdk_windowing_window_get_input_shape (GdkWindow *window) { (void) window; return NULL; }

GdkRegion *
_gdk_windowing_get_shape_for_mask (GdkBitmap *mask)
{
  GdkRegion *region = gdk_region_new (); GdkDrawableImplSymbian *impl; cairo_surface_t *s; guchar *p; gint stride, w, h, x, y;
  impl = GDK_DRAWABLE_IMPL_SYMBIAN (GDK_PIXMAP_OBJECT (mask)->impl); s = impl->surface; w = impl->width; h = impl->height;
  cairo_surface_flush (s); p = cairo_image_surface_get_data (s); stride = cairo_image_surface_get_stride (s);
  for (y = 0; y < h; y++) {
    for (x = 0; x < w; ) {
      gint x0;
      while (x < w && !p[y * stride + x]) x++;
      x0 = x;
      while (x < w && p[y * stride + x]) x++;
      if (x > x0) { GdkRectangle r = { x0, y, x - x0, 1 }; gdk_region_union_with_rect (region, &r); }
    }
  }
  return region;
}

void
_gdk_windowing_get_pointer (GdkDisplay *display, GdkScreen **screen, gint *x, gint *y, GdkModifierType *mask)
{
  GdkDisplaySymbian *d = GDK_DISPLAY_SYMBIAN (display);
  if (screen) *screen = d->screen; if (x) *x = d->pointer_x; if (y) *y = d->pointer_y; if (mask) *mask = d->pointer_state;
}

GdkWindow *
_gdk_windowing_window_at_pointer (GdkDisplay *display, gint *win_x, gint *win_y, GdkModifierType *mask, gboolean get_toplevel)
{
  GdkDisplaySymbian *d = GDK_DISPLAY_SYMBIAN (display); GList *l; GdkWindow *found = NULL; gint rx = d->pointer_x, ry = d->pointer_y, wx = rx, wy = ry;
  for (l = g_list_last (toplevel_stack); l; l = l->prev) {                    /* topmost first */
    GdkWindowImplSymbian *wi = WIMPL (l->data);
    if (rx >= wi->drawable.abs_x && ry >= wi->drawable.abs_y && rx < wi->drawable.abs_x + wi->drawable.width && ry < wi->drawable.abs_y + wi->drawable.height) {
      found = l->data; wx = rx - wi->drawable.abs_x; wy = ry - wi->drawable.abs_y; break;
    }
  }
  if (!found) { found = _gdk_parent_root; wx = rx; wy = ry; }
  else if (!get_toplevel) { gint fx, fy; GdkWindow *c = _gdk_window_find_descendant_at (found, wx, wy, &fx, &fy); if (c) { found = c; wx = fx; wy = fy; } }
  if (win_x) *win_x = wx; if (win_y) *win_y = wy; if (mask) *mask = d->pointer_state;
  return found;
}

GdkGrabStatus
_gdk_windowing_pointer_grab (GdkWindow *window, GdkWindow *native, gboolean owner_events, GdkEventMask event_mask, GdkWindow *confine_to, GdkCursor *cursor, guint32 time)
{
  /* nobody else shares the pointer: the grab is GDK's routing of events to @window (done by the core from the display's grab list) */
  (void) window; (void) native; (void) owner_events; (void) event_mask; (void) confine_to; (void) time;
  if (cursor) gdk_window_set_cursor (native ? native : window, cursor);
  return GDK_GRAB_SUCCESS;
}

cairo_surface_t *
_gdk_windowing_create_cairo_surface (GdkDrawable *drawable, gint width, gint height)
{
  cairo_surface_t *base = _gdk_drawable_ref_cairo_surface (drawable), *s;
  if (!base) return NULL;
  s = cairo_surface_create_similar (base, cairo_surface_get_content (base), width, height);
  cairo_surface_destroy (base); return s;
}
void _gdk_windowing_set_cairo_surface_size (cairo_surface_t *surface, gint width, gint height) { (void) surface; (void) width; (void) height; }   /* image surfaces have a fixed size: the drawable's resize makes a new view */
