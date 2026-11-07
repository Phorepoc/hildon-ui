/* phegdkwin_demo.c -- the gdk-symbian demo, written against the PUBLIC GDK 2.24 API only: a real GdkWindow, its expose events drawn with cairo + Pango + gdk-pixbuf, the deprecated
 * GdkGC/GdkFont drawing (XOR, opaque stipple, text) over the same window, live key and tap events, and a start-up self test of the backend pieces that have no pixels (atoms,
 * properties, selection ownership, text-property conversions, keymap, fonts, drag-and-drop registration, raster operations on a pixmap read back with gdk_drawable_get_image). BSD-2-Clause (Phorepoc). */
#include <stdio.h>
#include <string.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <pango/pangocairo.h>
#include "phe_png_data.h"

extern void phe_log (const char *msg);
#define LOG(...) do { char m_[240]; g_snprintf (m_, sizeof m_, __VA_ARGS__); phe_log (m_); } while (0)

static GdkWindow *win;
static GdkPixbuf *scaled;
static gint ticks, keys, taps; static gdouble dots[16][2];
static GdkRectangle box;                       /* the live status box */
static GdkGC *xor_gc, *stipple_gc; static GdkPixmap *stipple; static GdkFont *font;
static gint sw, sh;

static void
round_rect (cairo_t *cr, double x, double y, double w, double h, double r)
{
  cairo_new_sub_path (cr);
  cairo_arc (cr, x + w - r, y + r, r, -1.5708, 0); cairo_arc (cr, x + w - r, y + h - r, r, 0, 1.5708);
  cairo_arc (cr, x + r, y + h - r, r, 1.5708, 3.14159); cairo_arc (cr, x + r, y + r, r, 3.14159, 4.71239); cairo_close_path (cr);
}

static void
text (cairo_t *cr, const char *fontname, const char *str, double x, double y, double a)
{
  PangoLayout *lay = pango_cairo_create_layout (cr); PangoFontDescription *fd = pango_font_description_from_string (fontname);
  pango_layout_set_font_description (lay, fd); pango_font_description_free (fd); pango_layout_set_text (lay, str, -1);
  cairo_set_source_rgba (cr, 1, 1, 1, a); cairo_move_to (cr, x, y); pango_cairo_show_layout (cr, lay); g_object_unref (lay);
}

/* the scene, drawn for GDK_EXPOSE of the window: everything intersecting the expose region */
static void
paint (GdkWindow *w, GdkRegion *region)
{
  cairo_t *cr = gdk_cairo_create (w); double W = sw, H = sh, mg = W * 0.07, cw = W - 2 * mg, ch = H * 0.42, cy = H * 0.08; cairo_pattern_t *g; gchar buf[96]; gint i;
  gdk_cairo_region (cr, region); cairo_clip (cr);
  g = cairo_pattern_create_linear (0, 0, 0, H);
  cairo_pattern_add_color_stop_rgb (g, 0, 0.10, 0.16, 0.30); cairo_pattern_add_color_stop_rgb (g, 1, 0.02, 0.05, 0.10); cairo_set_source (cr, g); cairo_paint (cr); cairo_pattern_destroy (g);
  round_rect (cr, mg, cy, cw, ch, W * 0.04); cairo_set_source_rgba (cr, 1, 1, 1, 0.16); cairo_fill_preserve (cr); cairo_set_source_rgba (cr, 1, 1, 1, 0.55); cairo_set_line_width (cr, 2); cairo_stroke (cr);
  text (cr, "Sans 30", "Hildon", mg + W * 0.05, cy + ch * 0.07, 1.0);
  text (cr, "Sans 16", "a real GdkWindow", mg + W * 0.05, cy + ch * 0.34, 0.9);
  text (cr, "Sans 16", "GDK 2.24 + Symbian backend", mg + W * 0.05, cy + ch * 0.50, 0.9);
  text (cr, "Sans 12", "cairo, Pango, gdk-pixbuf", mg + W * 0.05, cy + ch * 0.70, 0.7);
  if (scaled) { gdk_cairo_set_source_pixbuf (cr, scaled, W * 0.2, H * 0.775); cairo_paint (cr); }
  round_rect (cr, box.x, box.y, box.width, box.height, 8); cairo_set_source_rgba (cr, 0.05, 0.4, 0.2, 0.55); cairo_fill (cr);
  g_snprintf (buf, sizeof buf, "ticks %d   keys %d   taps %d", ticks, keys, taps);
  text (cr, "Sans 14", buf, box.x + 14, box.y + 8, 1.0);
  for (i = 0; i < taps && i < 16; i++) { cairo_set_source_rgba (cr, 0.95, 0.75, 0.15, 0.9); cairo_arc (cr, dots[i][0], dots[i][1], 14, 0, 6.28318); cairo_fill (cr); }
  cairo_destroy (cr);

  /* the deprecated GdkGC / GdkFont API on the same window, clipped to the expose region (XOR twice would undo itself) */
  gdk_gc_set_clip_region (xor_gc, region); gdk_gc_set_clip_region (stipple_gc, region);
  gdk_draw_rectangle (w, stipple_gc, TRUE, (gint) (W * 0.07), (gint) (H * 0.665), (gint) (W * 0.86), 12);                           /* opaque stippled: red / blue dots */
  gdk_draw_rectangle (w, xor_gc, TRUE, (gint) (W * 0.07), (gint) (H * 0.71), (gint) (W * 0.86), 12);                                /* XOR: inverts what is below (the gradient) */
  if (font) { GdkGC *tg = gdk_gc_new (w); GdkColor c = { 0xffffff, 0xffff, 0xffff, 0xffff }; gdk_gc_set_clip_region (tg, region); gdk_gc_set_foreground (tg, &c);
    gdk_draw_string (w, font, tg, (gint) (W * 0.07), (gint) (H * 0.635), "GdkFont over Pango (gdk_draw_string)"); g_object_unref (tg); }
}

static void
invalidate_box (void) { gdk_window_invalidate_rect (win, &box, FALSE); }

static gboolean on_tick (gpointer d) { (void) d; ticks++; invalidate_box (); LOG ("[phegdkwin] tick %d", ticks); return ticks < 5; }
static gboolean on_stats (gpointer d) { (void) d; LOG ("[phegdkwin] stats timer fired"); return TRUE; }

static void
handler (GdkEvent *ev, gpointer data)
{
  (void) data;
  switch (ev->type) {
    case GDK_EXPOSE: {
      GdkRectangle r; gdk_region_get_clipbox (ev->expose.region, &r);
      LOG ("[phegdkwin] GDK_EXPOSE %d,%d %dx%d", r.x, r.y, r.width, r.height);
      paint (ev->expose.window, ev->expose.region); break; }
    case GDK_KEY_PRESS:
      keys++; invalidate_box ();
      LOG ("[phegdkwin] GDK_KEY_PRESS keyval=0x%x hw=0x%x string=[%s] state=0x%x", ev->key.keyval, ev->key.hardware_keycode, ev->key.string ? ev->key.string : "", ev->key.state); break;
    case GDK_KEY_RELEASE: LOG ("[phegdkwin] GDK_KEY_RELEASE keyval=0x%x", ev->key.keyval); break;
    case GDK_BUTTON_PRESS:
      LOG ("[phegdkwin] GDK_BUTTON_PRESS button=%u at %.0f,%.0f root %.0f,%.0f", ev->button.button, ev->button.x, ev->button.y, ev->button.x_root, ev->button.y_root);
      if (taps < 16) { dots[taps][0] = ev->button.x; dots[taps][1] = ev->button.y; taps++; gdk_window_invalidate_rect (win, NULL, FALSE); }
      break;
    case GDK_BUTTON_RELEASE: LOG ("[phegdkwin] GDK_BUTTON_RELEASE at %.0f,%.0f", ev->button.x, ev->button.y); break;
    case GDK_CONFIGURE: LOG ("[phegdkwin] GDK_CONFIGURE %d,%d %dx%d", ev->configure.x, ev->configure.y, ev->configure.width, ev->configure.height); break;
    case GDK_FOCUS_CHANGE: LOG ("[phegdkwin] GDK_FOCUS_CHANGE in=%d", ev->focus_change.in); break;
    case GDK_SELECTION_CLEAR: LOG ("[phegdkwin] GDK_SELECTION_CLEAR"); break;
    case GDK_PROPERTY_NOTIFY: LOG ("[phegdkwin] GDK_PROPERTY_NOTIFY state=%d", ev->property.state); break;
    default: break;
  }
}

/* ---- self test of the backend pieces that have no pixels ---- */
static gint st_ok, st_fail;
#define CHECK(c, what) do { if (c) st_ok++; else { st_fail++; LOG ("[phegdkwin] selftest FAIL: %s (line %d)", what, __LINE__); } } while (0)

static guint32
pixel_at (GdkPixmap *pm, gint x, gint y)
{
  GdkImage *im = gdk_drawable_get_image (GDK_DRAWABLE (pm), x, y, 1, 1); guint32 p = gdk_image_get_pixel (im, 0, 0); g_object_unref (im); return p & 0x00ffffff;
}

/* the window manager facing API: hints, layers, states, geometry limits, grabs (acted on by the backend itself: there is no window manager) */
static void
selftest_windows (void)
{
  GdkWindowAttr a; GdkWindow *t; GdkGeometry g; gint w, h, before = st_fail; GList *stack, *last; GdkWMDecoration dec;
  memset (&a, 0, sizeof a);
  a.window_type = GDK_WINDOW_TOPLEVEL; a.wclass = GDK_INPUT_OUTPUT; a.x = 10; a.y = 10; a.width = 100; a.height = 80; a.event_mask = GDK_STRUCTURE_MASK;
  t = gdk_window_new (NULL, &a, GDK_WA_X | GDK_WA_Y);
  gdk_window_set_type_hint (t, GDK_WINDOW_TYPE_HINT_DIALOG); CHECK (gdk_window_get_type_hint (t) == GDK_WINDOW_TYPE_HINT_DIALOG, "type hint round trip");
  CHECK (!gdk_window_get_decorations (t, &dec), "decorations unset until asked"); gdk_window_set_decorations (t, GDK_DECOR_BORDER);
  CHECK (gdk_window_get_decorations (t, &dec) && dec == GDK_DECOR_BORDER, "decorations round trip");
  g.min_width = 120; g.min_height = 90; gdk_window_set_geometry_hints (t, &g, GDK_HINT_MIN_SIZE); gdk_window_resize (t, 50, 50); gdk_drawable_get_size (GDK_DRAWABLE (t), &w, &h);
  CHECK (w == 120 && h == 90, "geometry hints: minimum size is enforced");
  g.max_width = 200; g.max_height = 150; gdk_window_set_geometry_hints (t, &g, GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE); gdk_window_resize (t, 500, 500); gdk_drawable_get_size (GDK_DRAWABLE (t), &w, &h);
  CHECK (w == 200 && h == 150, "geometry hints: maximum size is enforced");
  gdk_window_show (t); CHECK (!(gdk_window_get_state (t) & GDK_WINDOW_STATE_WITHDRAWN), "mapped: not withdrawn");
  g.max_width = 0; g.max_height = 0; gdk_window_set_geometry_hints (t, &g, GDK_HINT_MIN_SIZE);                      /* a maximum size would (rightly) keep maximize from filling the screen */
  gdk_window_maximize (t); gdk_drawable_get_size (GDK_DRAWABLE (t), &w, &h);
  CHECK ((gdk_window_get_state (t) & GDK_WINDOW_STATE_MAXIMIZED) && w == sw && h == sh, "maximize: screen sized, state set");
  gdk_window_unmaximize (t); gdk_drawable_get_size (GDK_DRAWABLE (t), &w, &h);
  CHECK (!(gdk_window_get_state (t) & GDK_WINDOW_STATE_MAXIMIZED) && w == 200 && h == 150, "unmaximize: the saved size is back, state cleared");
  gdk_window_fullscreen (t); CHECK ((gdk_window_get_state (t) & GDK_WINDOW_STATE_FULLSCREEN) != 0, "fullscreen state"); gdk_window_unfullscreen (t);
  gdk_window_iconify (t); stack = gdk_screen_get_window_stack (gdk_screen_get_default ()); last = g_list_last (stack);
  CHECK ((gdk_window_get_state (t) & GDK_WINDOW_STATE_ICONIFIED) && last && last->data != t, "iconify: state set, off the stack");
  g_list_foreach (stack, (GFunc) g_object_unref, NULL); g_list_free (stack);
  gdk_window_deiconify (t); stack = gdk_screen_get_window_stack (gdk_screen_get_default ()); last = g_list_last (stack);
  CHECK (!(gdk_window_get_state (t) & GDK_WINDOW_STATE_ICONIFIED) && last && last->data == t, "deiconify: back on top");
  g_list_foreach (stack, (GFunc) g_object_unref, NULL); g_list_free (stack);
  gdk_window_set_keep_above (win, TRUE); gdk_window_raise (t);
  stack = gdk_screen_get_window_stack (gdk_screen_get_default ()); last = g_list_last (stack);
  CHECK (last && last->data == win && (gdk_window_get_state (win) & GDK_WINDOW_STATE_ABOVE), "keep above: stays on top of a later raised window");
  g_list_foreach (stack, (GFunc) g_object_unref, NULL); g_list_free (stack);
  gdk_window_set_keep_above (win, FALSE); gdk_window_raise (t);
  stack = gdk_screen_get_window_stack (gdk_screen_get_default ()); last = g_list_last (stack);
  CHECK (last && last->data == t, "layer cleared: the raised window is on top");
  g_list_foreach (stack, (GFunc) g_object_unref, NULL); g_list_free (stack);
  CHECK (gdk_keyboard_grab (t, FALSE, GDK_CURRENT_TIME) == GDK_GRAB_SUCCESS, "keyboard grab"); gdk_keyboard_ungrab (GDK_CURRENT_TIME);
  gdk_error_trap_push (); CHECK (gdk_error_trap_pop () == 0, "error trap");
  { GdkRectangle fr; gdk_window_get_frame_extents (t, &fr); CHECK (fr.width == 200 && fr.height == 150, "frame extents = window rectangle"); }
  gdk_window_destroy (t);
  LOG ("[phegdkwin] window API selftest done: %d failed", st_fail - before);
}

static void
selftest (void)
{
  GdkAtom prop = gdk_atom_intern ("PHE_TEST_PROP", FALSE), type; gint fmt, len; guchar *data = NULL; gchar **list = NULL; guchar *ct = NULL; gint ctlen, cfmt, n; GdkAtom cenc;
  GdkKeymap *km = gdk_keymap_get_for_display (gdk_display_get_default ()); guint kv; GdkModifierType consumed; GdkWindow *got;
  const gchar *utf8 = "Gr\303\274\303\237 \342\202\254 abc";
  CHECK (gdk_atom_intern ("CLIPBOARD", FALSE) == GDK_SELECTION_CLIPBOARD, "CLIPBOARD atom is 69");
  { gchar *nm = gdk_atom_name (GDK_TARGET_STRING); CHECK (nm && !strcmp (nm, "STRING"), "STRING atom name"); g_free (nm); }
  gdk_property_change (win, prop, GDK_TARGET_STRING, 8, GDK_PROP_MODE_REPLACE, (const guchar *) "hello", 5);
  CHECK (gdk_property_get (win, prop, GDK_TARGET_STRING, 0, 100, FALSE, &type, &fmt, &len, &data) && len == 5 && !memcmp (data, "hello", 5) && fmt == 8, "property round trip");
  g_free (data); gdk_property_change (win, prop, GDK_TARGET_STRING, 8, GDK_PROP_MODE_APPEND, (const guchar *) " world", 6); data = NULL;
  CHECK (gdk_property_get (win, prop, GDK_NONE, 0, 100, TRUE, &type, &fmt, &len, &data) && len == 11 && !memcmp (data, "hello world", 11), "property append, read and delete"); g_free (data); data = NULL;
  CHECK (!gdk_property_get (win, prop, GDK_NONE, 0, 100, FALSE, &type, &fmt, &len, &data), "property gone after delete");
  CHECK (gdk_utf8_to_compound_text_for_display (gdk_display_get_default (), utf8, &cenc, &cfmt, &ct, &ctlen) && cfmt == 8 && ctlen > 0, "utf8 -> compound text");
  n = gdk_text_property_to_utf8_list_for_display (gdk_display_get_default (), cenc, cfmt, ct, ctlen, &list);
  CHECK (n == 1 && list && !strcmp (list[0], utf8), "compound text -> utf8 round trip"); g_strfreev (list); gdk_free_compound_text (ct);
  { guchar *c2 = NULL; GdkAtom e2; gint f2, l2; CHECK (gdk_utf8_to_compound_text_for_display (gdk_display_get_default (), "plain ascii", &e2, &f2, &c2, &l2) && e2 == GDK_TARGET_STRING && l2 == 11, "ascii is STRING"); g_free (c2); }
  { gchar *l1 = gdk_utf8_to_string_target (utf8); CHECK (l1 && (guchar) l1[2] == 0xfc && (guchar) l1[3] == 0xdf, "utf8 -> STRING target (Latin-1)"); g_free (l1); }
  CHECK (gdk_selection_owner_set_for_display (gdk_display_get_default (), win, GDK_SELECTION_PRIMARY, GDK_CURRENT_TIME, TRUE), "selection owner set");
  got = gdk_selection_owner_get_for_display (gdk_display_get_default (), GDK_SELECTION_PRIMARY); CHECK (got == win, "selection owner get");
  gdk_selection_owner_set_for_display (gdk_display_get_default (), NULL, GDK_SELECTION_PRIMARY, GDK_CURRENT_TIME, TRUE);
  CHECK (gdk_selection_owner_get_for_display (gdk_display_get_default (), GDK_SELECTION_PRIMARY) == NULL, "selection owner cleared");
  CHECK (gdk_keymap_translate_keyboard_state (km, 0x41, GDK_SHIFT_MASK, 0, &kv, NULL, NULL, &consumed) && kv == GDK_KEY_A && (consumed & GDK_SHIFT_MASK), "keymap: shift+A");
  CHECK (gdk_keymap_translate_keyboard_state (km, 0x41, 0, 0, &kv, NULL, NULL, &consumed) && kv == GDK_KEY_a, "keymap: a");
  CHECK (gdk_keymap_translate_keyboard_state (km, 0x79, 0, 0, &kv, NULL, NULL, &consumed) && kv == GDK_KEY_comma, "keymap: comma scancode");
  { GdkKeymapKey *keys_; gint nk; CHECK (gdk_keymap_get_entries_for_keyval (km, GDK_KEY_Return, &keys_, &nk) && nk >= 1 && keys_[0].keycode == 0x03, "keymap: entries for Return"); g_free (keys_); }
  { gint wd, as, de; CHECK (font && font->ascent > 0 && font->descent >= 0, "GdkFont metrics"); gdk_text_extents (font, "Hello", 5, NULL, NULL, &wd, &as, &de); CHECK (wd > 10 && as > 0, "gdk_text_extents"); }
  { GdkDragProtocol pr; guint32 id = (guint32) (gsize) win; gdk_window_register_dnd (win); CHECK (gdk_drag_get_protocol_for_display (gdk_display_get_default (), id, &pr) == id && pr == GDK_DRAG_PROTO_LOCAL, "dnd: registered window is a local destination"); }
  { GdkPixmap *pm = gdk_pixmap_new (win, 8, 8, 24); GdkGC *gc = gdk_gc_new (pm); GdkColor c; GdkBitmap *bm; static const gchar bits[] = { 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa }; GdkGC *sg;
    c.pixel = 0x0000ff00; gdk_gc_set_foreground (gc, &c); gdk_draw_rectangle (pm, gc, TRUE, 0, 0, 8, 8);
    CHECK (pixel_at (pm, 3, 3) == 0x0000ff00, "pixmap fill (COPY)");
    c.pixel = 0x000000ff; gdk_gc_set_foreground (gc, &c); gdk_gc_set_function (gc, GDK_XOR); gdk_draw_rectangle (pm, gc, TRUE, 0, 0, 4, 8);
    CHECK (pixel_at (pm, 1, 1) == 0x0000ffff && pixel_at (pm, 6, 1) == 0x0000ff00, "XOR through the GC: 00ff00 ^ 0000ff = 00ffff, outside untouched");
    gdk_draw_rectangle (pm, gc, TRUE, 0, 0, 4, 8); CHECK (pixel_at (pm, 1, 1) == 0x0000ff00, "XOR twice restores");
    gdk_gc_set_function (gc, GDK_AND); c.pixel = 0x0000f0f0; gdk_gc_set_foreground (gc, &c); gdk_draw_rectangle (pm, gc, TRUE, 4, 0, 4, 8); CHECK (pixel_at (pm, 5, 1) == 0x0000f000, "AND: ff00 & f0f0 = f000");
    gdk_gc_set_function (gc, GDK_INVERT); gdk_draw_rectangle (pm, gc, TRUE, 4, 0, 4, 8); CHECK (pixel_at (pm, 5, 1) == 0x00ff0fff, "INVERT: ~0000f000 = ff0fff in 24 bits");
    bm = gdk_bitmap_create_from_data (NULL, bits, 8, 8); sg = gdk_gc_new (pm);
    gdk_gc_set_function (sg, GDK_COPY); gdk_gc_set_fill (sg, GDK_OPAQUE_STIPPLED); gdk_gc_set_stipple (sg, bm);
    c.pixel = 0x00ff0000; gdk_gc_set_foreground (sg, &c); c.pixel = 0x000000ff; gdk_gc_set_background (sg, &c); gdk_draw_rectangle (pm, sg, TRUE, 0, 0, 8, 8);
    CHECK (pixel_at (pm, 0, 0) == 0x00ff0000 && pixel_at (pm, 1, 0) == 0x000000ff && pixel_at (pm, 0, 1) == 0x000000ff && pixel_at (pm, 1, 1) == 0x00ff0000, "opaque stipple: bit 1 -> foreground, 0 -> background");
    g_object_unref (sg); g_object_unref (bm); g_object_unref (gc); g_object_unref (pm); }
  LOG ("[phegdkwin] selftest done: %d passed, %d failed", st_ok, st_fail);
}

static void
make_gcs (void)
{
  static const gchar bits[] = { 0x03, 0x03, 0x0c, 0x0c, 0x03, 0x03, 0x0c, 0x0c };       /* a 2x2 checker of 2x2 cells */
  GdkColor c;
  xor_gc = gdk_gc_new (win); c.pixel = 0x00ffffff; gdk_gc_set_foreground (xor_gc, &c); gdk_gc_set_function (xor_gc, GDK_XOR);
  stipple = gdk_bitmap_create_from_data (NULL, bits, 8, 8);
  stipple_gc = gdk_gc_new (win); gdk_gc_set_fill (stipple_gc, GDK_OPAQUE_STIPPLED); gdk_gc_set_stipple (stipple_gc, stipple);
  c.pixel = 0x00e03030; gdk_gc_set_foreground (stipple_gc, &c); c.pixel = 0x003050e0; gdk_gc_set_background (stipple_gc, &c);
}

int
phe_gdk_demo_main (void)
{
  GdkWindowAttr a; GdkPixbufLoader *ld; GError *err = NULL; PangoFontDescription *fd; GMainLoop *loop;
  if (!gdk_init_check (NULL, NULL)) { LOG ("[phegdkwin] gdk_init_check failed"); return 1; }
  sw = gdk_screen_get_width (gdk_screen_get_default ()); sh = gdk_screen_get_height (gdk_screen_get_default ());
  LOG ("[phegdkwin] gdk_init_check ok: display %s, screen %dx%d, visual depth %d", gdk_display_get_name (gdk_display_get_default ()), sw, sh, gdk_visual_get_depth (gdk_visual_get_system ()));
  memset (&a, 0, sizeof a);
  a.window_type = GDK_WINDOW_TOPLEVEL; a.wclass = GDK_INPUT_OUTPUT; a.x = 0; a.y = 0; a.width = sw; a.height = sh;
  a.event_mask = GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_STRUCTURE_MASK | GDK_FOCUS_CHANGE_MASK | GDK_PROPERTY_CHANGE_MASK;
  win = gdk_window_new (NULL, &a, GDK_WA_X | GDK_WA_Y);
  box.x = (gint) (sw * 0.07); box.width = (gint) (sw * 0.86); box.height = 40; box.y = (gint) (sh * 0.54);
  fd = pango_font_description_from_string ("Sans 12"); font = gdk_font_from_description (fd); pango_font_description_free (fd);
  ld = gdk_pixbuf_loader_new ();
  if (gdk_pixbuf_loader_write (ld, phe_png, sizeof phe_png, &err) && gdk_pixbuf_loader_close (ld, &err)) scaled = gdk_pixbuf_scale_simple (gdk_pixbuf_loader_get_pixbuf (ld), (gint) (sw * 0.6), (gint) (sw * 0.18), GDK_INTERP_NEAREST);
  g_clear_error (&err); g_object_unref (ld);
  make_gcs ();
  gdk_event_handler_set (handler, NULL, NULL);
  selftest ();
  gdk_window_show (win);
  selftest_windows ();
  LOG ("[phegdkwin] window shown (id 0x%x, visible %d)", (guint) (gsize) win, gdk_window_is_visible (win));
  g_timeout_add (400, on_tick, NULL); g_timeout_add (2000, on_stats, NULL);
  loop = g_main_loop_new (NULL, FALSE);
  LOG ("[phegdkwin] loop running");
  g_main_loop_run (loop);
  return 0;
}
