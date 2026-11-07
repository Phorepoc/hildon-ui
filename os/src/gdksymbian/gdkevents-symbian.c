/* gdkevents-symbian.c -- events and keymap of the Symbian backend.
 * The WSERV thread (phegdkwin.cpp) collects key and pointer events and writes one byte per batch into a pipe; the GSource here has the pipe's read end as its poll fd, so the GLib
 * main loop sleeps until WSERV has something. _gdk_events_queue() turns the raw events into GdkEvents on the display's event queue (and lets the core route them with
 * _gdk_windowing_got_event); the source's dispatch emits them. Key codes: WSERV delivers key-down(scancode), key(code), key-up(scancode) for one press; the key-down is held back
 * until the typed event supplies the keysym (key-up for keys that never type). BSD-2-Clause (Phorepoc). */
#include "config.h"
#include <string.h>
#include <unistd.h>
#include "gdksymbian.h"
#include "gdkkeysyms.h"
#include "gdkprivate.h"
#include "gdkinternals.h"
#include "gdkdisplay.h"
#include "gdkkeys.h"

/* ---- scancodes -> keyvals (e32keys.h TStdScanCode), the keymap of the device: group 0, level 0 = unshifted, level 1 = shifted ---- */
typedef struct { guint8 code; guint16 lo; guint16 hi; } KeyEntry;
#define K(c, l, h) { c, l, h }
static const KeyEntry keytable[] = {   /* sorted by nothing: looked up linearly */
  K (0x01, GDK_KEY_BackSpace, GDK_KEY_BackSpace), K (0x02, GDK_KEY_Tab, GDK_KEY_ISO_Left_Tab), K (0x03, GDK_KEY_Return, GDK_KEY_Return), K (0x04, GDK_KEY_Escape, GDK_KEY_Escape),
  K (0x05, GDK_KEY_space, GDK_KEY_space), K (0x06, GDK_KEY_Print, GDK_KEY_Print), K (0x07, GDK_KEY_Pause, GDK_KEY_Pause), K (0x08, GDK_KEY_Home, GDK_KEY_Home),
  K (0x09, GDK_KEY_End, GDK_KEY_End), K (0x0a, GDK_KEY_Page_Up, GDK_KEY_Page_Up), K (0x0b, GDK_KEY_Page_Down, GDK_KEY_Page_Down), K (0x0c, GDK_KEY_Insert, GDK_KEY_Insert),
  K (0x0d, GDK_KEY_Delete, GDK_KEY_Delete), K (0x0e, GDK_KEY_Left, GDK_KEY_Left), K (0x0f, GDK_KEY_Right, GDK_KEY_Right), K (0x10, GDK_KEY_Up, GDK_KEY_Up), K (0x11, GDK_KEY_Down, GDK_KEY_Down),
  K (0x12, GDK_KEY_Shift_L, GDK_KEY_Shift_L), K (0x13, GDK_KEY_Shift_R, GDK_KEY_Shift_R), K (0x14, GDK_KEY_Alt_L, GDK_KEY_Alt_L), K (0x15, GDK_KEY_Alt_R, GDK_KEY_Alt_R),
  K (0x16, GDK_KEY_Control_L, GDK_KEY_Control_L), K (0x17, GDK_KEY_Control_R, GDK_KEY_Control_R), K (0x1a, GDK_KEY_Caps_Lock, GDK_KEY_Caps_Lock), K (0x1b, GDK_KEY_Num_Lock, GDK_KEY_Num_Lock),
  K (0x1c, GDK_KEY_Scroll_Lock, GDK_KEY_Scroll_Lock),
  K (0x30, GDK_KEY_0, GDK_KEY_parenright), K (0x31, GDK_KEY_1, GDK_KEY_exclam), K (0x32, GDK_KEY_2, GDK_KEY_at), K (0x33, GDK_KEY_3, GDK_KEY_numbersign), K (0x34, GDK_KEY_4, GDK_KEY_dollar),
  K (0x35, GDK_KEY_5, GDK_KEY_percent), K (0x36, GDK_KEY_6, GDK_KEY_asciicircum), K (0x37, GDK_KEY_7, GDK_KEY_ampersand), K (0x38, GDK_KEY_8, GDK_KEY_asterisk), K (0x39, GDK_KEY_9, GDK_KEY_parenleft),
  K (0x41, GDK_KEY_a, GDK_KEY_A), K (0x42, GDK_KEY_b, GDK_KEY_B), K (0x43, GDK_KEY_c, GDK_KEY_C), K (0x44, GDK_KEY_d, GDK_KEY_D), K (0x45, GDK_KEY_e, GDK_KEY_E), K (0x46, GDK_KEY_f, GDK_KEY_F),
  K (0x47, GDK_KEY_g, GDK_KEY_G), K (0x48, GDK_KEY_h, GDK_KEY_H), K (0x49, GDK_KEY_i, GDK_KEY_I), K (0x4a, GDK_KEY_j, GDK_KEY_J), K (0x4b, GDK_KEY_k, GDK_KEY_K), K (0x4c, GDK_KEY_l, GDK_KEY_L),
  K (0x4d, GDK_KEY_m, GDK_KEY_M), K (0x4e, GDK_KEY_n, GDK_KEY_N), K (0x4f, GDK_KEY_o, GDK_KEY_O), K (0x50, GDK_KEY_p, GDK_KEY_P), K (0x51, GDK_KEY_q, GDK_KEY_Q), K (0x52, GDK_KEY_r, GDK_KEY_R),
  K (0x53, GDK_KEY_s, GDK_KEY_S), K (0x54, GDK_KEY_t, GDK_KEY_T), K (0x55, GDK_KEY_u, GDK_KEY_U), K (0x56, GDK_KEY_v, GDK_KEY_V), K (0x57, GDK_KEY_w, GDK_KEY_W), K (0x58, GDK_KEY_x, GDK_KEY_X),
  K (0x59, GDK_KEY_y, GDK_KEY_Y), K (0x5a, GDK_KEY_z, GDK_KEY_Z),
  K (0x60, GDK_KEY_F1, GDK_KEY_F1), K (0x61, GDK_KEY_F2, GDK_KEY_F2), K (0x62, GDK_KEY_F3, GDK_KEY_F3), K (0x63, GDK_KEY_F4, GDK_KEY_F4), K (0x64, GDK_KEY_F5, GDK_KEY_F5), K (0x65, GDK_KEY_F6, GDK_KEY_F6),
  K (0x66, GDK_KEY_F7, GDK_KEY_F7), K (0x67, GDK_KEY_F8, GDK_KEY_F8), K (0x68, GDK_KEY_F9, GDK_KEY_F9), K (0x69, GDK_KEY_F10, GDK_KEY_F10), K (0x6a, GDK_KEY_F11, GDK_KEY_F11), K (0x6b, GDK_KEY_F12, GDK_KEY_F12),
  K (0x6c, GDK_KEY_F13, GDK_KEY_F13), K (0x6d, GDK_KEY_F14, GDK_KEY_F14), K (0x6e, GDK_KEY_F15, GDK_KEY_F15), K (0x6f, GDK_KEY_F16, GDK_KEY_F16), K (0x70, GDK_KEY_F17, GDK_KEY_F17), K (0x71, GDK_KEY_F18, GDK_KEY_F18),
  K (0x72, GDK_KEY_F19, GDK_KEY_F19), K (0x73, GDK_KEY_F20, GDK_KEY_F20), K (0x74, GDK_KEY_F21, GDK_KEY_F21), K (0x75, GDK_KEY_F22, GDK_KEY_F22), K (0x76, GDK_KEY_F23, GDK_KEY_F23), K (0x77, GDK_KEY_F24, GDK_KEY_F24),
  /* punctuation (EStdKeyComma .. EStdKeyEquals; the names are those of e32keys.h, the shifted symbols those of the standard layout) */
  K (0x79, GDK_KEY_comma, GDK_KEY_less), K (0x7a, GDK_KEY_period, GDK_KEY_greater), K (0x7b, GDK_KEY_slash, GDK_KEY_question), K (0x7c, GDK_KEY_backslash, GDK_KEY_bar),
  K (0x7d, GDK_KEY_semicolon, GDK_KEY_colon), K (0x7e, GDK_KEY_apostrophe, GDK_KEY_quotedbl), K (0x7f, GDK_KEY_numbersign, GDK_KEY_asciitilde),
  K (0x80, GDK_KEY_bracketleft, GDK_KEY_braceleft), K (0x81, GDK_KEY_bracketright, GDK_KEY_braceright), K (0x82, GDK_KEY_minus, GDK_KEY_underscore), K (0x83, GDK_KEY_equal, GDK_KEY_plus),
  /* numeric keypad (EStdKeyNkp*) */
  K (0x84, GDK_KEY_KP_Divide, GDK_KEY_KP_Divide), K (0x85, GDK_KEY_KP_Multiply, GDK_KEY_KP_Multiply), K (0x86, GDK_KEY_KP_Subtract, GDK_KEY_KP_Subtract), K (0x87, GDK_KEY_KP_Add, GDK_KEY_KP_Add),
  K (0x88, GDK_KEY_KP_Enter, GDK_KEY_KP_Enter), K (0x89, GDK_KEY_KP_1, GDK_KEY_KP_End), K (0x8a, GDK_KEY_KP_2, GDK_KEY_KP_Down), K (0x8b, GDK_KEY_KP_3, GDK_KEY_KP_Page_Down),
  K (0x8c, GDK_KEY_KP_4, GDK_KEY_KP_Left), K (0x8d, GDK_KEY_KP_5, GDK_KEY_KP_Begin), K (0x8e, GDK_KEY_KP_6, GDK_KEY_KP_Right), K (0x8f, GDK_KEY_KP_7, GDK_KEY_KP_Home),
  K (0x90, GDK_KEY_KP_8, GDK_KEY_KP_Up), K (0x91, GDK_KEY_KP_9, GDK_KEY_KP_Page_Up), K (0x92, GDK_KEY_KP_0, GDK_KEY_KP_Insert), K (0x93, GDK_KEY_KP_Decimal, GDK_KEY_KP_Delete),
  K (0x94, GDK_KEY_Menu, GDK_KEY_Menu), K (0x9f, GDK_KEY_Help, GDK_KEY_Help),
};
#undef K
#define N_KEYS ((gint) G_N_ELEMENTS (keytable))

static const KeyEntry *entry_of (guint code) { gint i; for (i = 0; i < N_KEYS; i++) if (keytable[i].code == code) return &keytable[i]; return NULL; }

static GdkKeymap *the_keymap = NULL;
static gboolean caps_lock = FALSE;
static guint key_state = 0;                      /* modifier bits of the keys currently down: shift, control, alt */

GdkKeymap *
gdk_keymap_get_for_display (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  if (!the_keymap) { the_keymap = g_object_new (GDK_TYPE_KEYMAP, NULL); the_keymap->display = display; }
  return the_keymap;
}

PangoDirection gdk_keymap_get_direction (GdkKeymap *keymap) { (void) keymap; return PANGO_DIRECTION_LTR; }
gboolean gdk_keymap_have_bidi_layouts (GdkKeymap *keymap) { (void) keymap; return FALSE; }
gboolean gdk_keymap_get_caps_lock_state (GdkKeymap *keymap) { (void) keymap; return caps_lock; }

guint
gdk_keymap_lookup_key (GdkKeymap *keymap, const GdkKeymapKey *key)
{
  const KeyEntry *e;
  g_return_val_if_fail (key != NULL, 0); (void) keymap;
  if (key->group != 0 || key->level < 0 || key->level > 1) return 0;
  e = entry_of (key->keycode);
  return e ? (key->level ? e->hi : e->lo) : 0;
}

gboolean
gdk_keymap_translate_keyboard_state (GdkKeymap *keymap, guint hardware_keycode, GdkModifierType state, gint group, guint *keyval, gint *effective_group, gint *level, GdkModifierType *consumed_modifiers)
{
  const KeyEntry *e = entry_of (hardware_keycode); gint lv = 0; guint kv; GdkModifierType consumed = 0; (void) keymap; (void) group;
  if (!e) { if (keyval) *keyval = 0; if (effective_group) *effective_group = 0; if (level) *level = 0; if (consumed_modifiers) *consumed_modifiers = 0; return FALSE; }
  if (state & GDK_SHIFT_MASK) lv = 1;
  if ((state & GDK_LOCK_MASK) && e->lo != e->hi && (e->lo >= GDK_KEY_a && e->lo <= GDK_KEY_z)) lv = !lv;     /* caps lock shifts letters only */
  kv = lv ? e->hi : e->lo;
  if (lv && (state & GDK_SHIFT_MASK) && e->lo != e->hi) consumed |= GDK_SHIFT_MASK;
  if (keyval) *keyval = kv; if (effective_group) *effective_group = 0; if (level) *level = lv; if (consumed_modifiers) *consumed_modifiers = consumed;
  return TRUE;
}

gboolean
gdk_keymap_get_entries_for_keyval (GdkKeymap *keymap, guint keyval, GdkKeymapKey **keys, gint *n_keys)
{
  GArray *a = g_array_new (FALSE, FALSE, sizeof (GdkKeymapKey)); gint i;
  g_return_val_if_fail (keys != NULL && n_keys != NULL, FALSE); (void) keymap;
  for (i = 0; i < N_KEYS; i++) {
    GdkKeymapKey k; k.keycode = keytable[i].code; k.group = 0;
    if (keytable[i].lo == keyval) { k.level = 0; g_array_append_val (a, k); }
    if (keytable[i].hi == keyval && keytable[i].hi != keytable[i].lo) { k.level = 1; g_array_append_val (a, k); }
  }
  *n_keys = a->len; *keys = (GdkKeymapKey *) g_array_free (a, FALSE);
  return *n_keys > 0;
}

gboolean
gdk_keymap_get_entries_for_keycode (GdkKeymap *keymap, guint hardware_keycode, GdkKeymapKey **keys, guint **keyvals, gint *n_entries)
{
  const KeyEntry *e = entry_of (hardware_keycode); gint n; (void) keymap;
  g_return_val_if_fail (n_entries != NULL, FALSE);
  if (!e) { if (keys) *keys = NULL; if (keyvals) *keyvals = NULL; *n_entries = 0; return FALSE; }
  n = e->lo != e->hi ? 2 : 1;
  if (keys) { gint i; *keys = g_new0 (GdkKeymapKey, n); for (i = 0; i < n; i++) { (*keys)[i].keycode = hardware_keycode; (*keys)[i].group = 0; (*keys)[i].level = i; } }
  if (keyvals) { *keyvals = g_new0 (guint, n); (*keyvals)[0] = e->lo; if (n > 1) (*keyvals)[1] = e->hi; }
  *n_entries = n; return TRUE;
}

/* there are no keys bound to the virtual modifiers (super, hyper, meta): they stay as they are, and cannot be mapped to real ones */
void gdk_keymap_add_virtual_modifiers (GdkKeymap *keymap, GdkModifierType *state) { (void) keymap; (void) state; }
gboolean
gdk_keymap_map_virtual_modifiers (GdkKeymap *keymap, GdkModifierType *state)
{
  const GdkModifierType virt = GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK; gboolean had = (*state & virt) != 0; (void) keymap;
  *state &= ~virt;
  return !had;
}

/* ---- WSERV events -> GdkEvents ---- */
static guint32 now_ms (void) { return (guint32) (g_get_monotonic_time () / 1000); }

/* WSERV key codes (e32keys.h TKeyCode) -> X keysyms; printable codes are the same in both */
static guint
keysym_of (gint code, gint scancode)
{
  const KeyEntry *e;
  switch (code) {
    case 0x0e: return GDK_KEY_Return; case 0x01: return GDK_KEY_BackSpace; case 0x0b: return GDK_KEY_Escape; case 0x09: return GDK_KEY_Tab;
    case 0xf807: return GDK_KEY_Left; case 0xf808: return GDK_KEY_Right; case 0xf809: return GDK_KEY_Up; case 0xf80a: return GDK_KEY_Down;
    case 0xf845: return GDK_KEY_F5; case 0xf846: return GDK_KEY_F6; case 0xf847: return GDK_KEY_F7;      /* the softkeys EKeyDevice0..2 */
  }
  if (code >= 0x20 && code < 0x7f) return (guint) code;
  if (code == 0 && (e = entry_of (scancode)) != NULL) return e->lo;
  return 0x1008ff00u + (guint) (scancode & 0xff);                 /* vendor key space: unknown keys stay distinguishable */
}

static GdkWindow *focus_window = NULL;            /* the toplevel that gets key events: the topmost mapped one */

/* the keyboard focus goes to the topmost mapped toplevel that accepts it: override-redirect windows and temporary ones (menus, tooltips) do not take it */
static GdkWindow *
top_toplevel (void)
{
  GList *s;
  for (s = g_list_last (_gdk_symbian_toplevel_stack ()); s; s = s->prev) {
    GdkWindowImplSymbian *wi = (GdkWindowImplSymbian *) GDK_WINDOW_OBJECT (s->data)->impl;
    if (wi->override_redirect || !wi->accept_focus || wi->skip_focus_once || GDK_WINDOW_OBJECT (s->data)->window_type == GDK_WINDOW_TEMP) continue;
    return s->data;
  }
  return _gdk_parent_root;
}

static void
queue_event (GdkDisplay *display, GdkEvent *event)
{
  GList *node = _gdk_event_queue_append (display, event);
  _gdk_windowing_got_event (display, node, event, _gdk_windowing_window_get_next_serial (display));
}

/* ---- window event filters (gdk_window_add_filter): GDK stores them, the backend runs them over the native events it makes. The "native event" passed to a filter is an X-style event
 * structure (the one an X11 toolkit's filter casts to XEvent / XPropertyEvent / XClientMessageEvent); this backend builds the two it generates itself: a property change of the root window
 * (the window manager telling clients which application window is active) and the client messages sent between windows (XSendEvent in the Xlib shim). ---- */
GdkFilterReturn
gdk_symbian_apply_filters (GdkWindow *window, gpointer xevent, GdkEvent *event)
{
  GdkFilterReturn result = GDK_FILTER_CONTINUE; GList *lists[2], *l, *copy; gint k;
  lists[0] = window ? GDK_WINDOW_OBJECT (window)->filters : NULL; lists[1] = _gdk_default_filters;
  for (k = 0; k < 2 && result == GDK_FILTER_CONTINUE; k++) {
    copy = g_list_copy (lists[k]);
    for (l = copy; l && result == GDK_FILTER_CONTINUE; l = l->next) {
      GdkEventFilter *f = l->data;
      if (f->flags & GDK_EVENT_FILTER_REMOVED) continue;
      f->ref_count++;
      result = f->function (xevent, event, f->data);
      _gdk_event_filter_unref (window, f);
    }
    g_list_free (copy);
  }
  return result;
}

typedef struct { gint type; gulong serial; gint send_event; gpointer display; gulong window; gulong atom; gulong time; gint state; } PheXPropertyEvent;

/* the window manager's active-application property of the root window: the toplevel that holds the focus, and the PropertyNotify a client with a root filter reacts to */
static void
set_active_window_property (GdkWindow *top)
{
  GdkWindow *root = _gdk_parent_root; GdkAtom prop = gdk_atom_intern_static_string ("_MB_CURRENT_APP_WINDOW"); gulong xid = (gulong) (gsize) top; PheXPropertyEvent xe;
  gdk_property_change (root, prop, gdk_atom_intern_static_string ("WINDOW"), 32, GDK_PROP_MODE_REPLACE, (const guchar *) &xid, 1);
  memset (&xe, 0, sizeof xe); xe.type = 28; xe.display = _gdk_display; xe.window = (gulong) (gsize) root; xe.atom = (gulong) GPOINTER_TO_UINT (prop); xe.time = now_ms (); xe.state = 0;
  gdk_symbian_apply_filters (root, &xe, NULL);
  { GList *l, *copy = g_list_copy (_gdk_symbian_toplevel_stack ());       /* the toplevels that selected property changes see them too (what a toolkit that watches the root does through its own window) */
    for (l = copy; l; l = l->next) if (!GDK_WINDOW_DESTROYED (l->data) && (((GdkWindowImplSymbian *) GDK_WINDOW_OBJECT (l->data)->impl)->event_mask & GDK_PROPERTY_CHANGE_MASK)) gdk_symbian_apply_filters (l->data, &xe, NULL);
    g_list_free (copy); }
}

/* Called by the window code when the stack of toplevels changed: a toplevel that lost the top loses the keyboard focus, the new top gets it. */
void
_gdk_symbian_focus_update (void)
{
  GdkWindow *top = top_toplevel (); GdkDisplay *display = _gdk_display;
  if (!display || top == focus_window) return;
  if (focus_window && !GDK_WINDOW_DESTROYED (focus_window)) {
    GdkEvent *ev = gdk_event_new (GDK_FOCUS_CHANGE); ev->focus_change.window = g_object_ref (focus_window); ev->focus_change.in = FALSE; queue_event (display, ev);
  }
  focus_window = top;
  if (focus_window != _gdk_parent_root) set_active_window_property (focus_window);
  if (focus_window != _gdk_parent_root) {
    GdkEvent *ev = gdk_event_new (GDK_FOCUS_CHANGE); ev->focus_change.window = g_object_ref (focus_window); ev->focus_change.in = TRUE; queue_event (display, ev);
  }
}

static struct { gboolean down_pending; gint scancode; guint last_keyval; } kstate;

static void
modifier_update (gint scancode, gboolean down)
{
  guint bit = 0;
  switch (scancode) { case 0x12: case 0x13: bit = GDK_SHIFT_MASK; break; case 0x14: case 0x15: bit = GDK_MOD1_MASK; break; case 0x16: case 0x17: bit = GDK_CONTROL_MASK; break; }
  if (bit) { if (down) key_state |= bit; else key_state &= ~bit; }
  if (scancode == 0x1a && down) caps_lock = !caps_lock;
}

static void
queue_key (GdkDisplay *display, gboolean press, guint keyval, gint scancode)
{
  GdkEvent *ev = gdk_event_new (press ? GDK_KEY_PRESS : GDK_KEY_RELEASE); GdkWindow *w = focus_window ? focus_window : top_toplevel (); guint32 ucs;
  if (display->keyboard_grab.window && !display->keyboard_grab.owner_events) w = display->keyboard_grab.window;    /* a keyboard grab without owner events sends every key to the grab window */
  ev->key.window = g_object_ref (w); ev->key.send_event = FALSE; ev->key.time = now_ms ();
  ev->key.state = key_state | (caps_lock ? GDK_LOCK_MASK : 0) | GDK_DISPLAY_SYMBIAN (display)->pointer_state;
  ev->key.keyval = keyval; ev->key.hardware_keycode = scancode; ev->key.group = 0; ev->key.is_modifier = (scancode >= 0x12 && scancode <= 0x17) || scancode == 0x1a;
  ucs = gdk_keyval_to_unicode (keyval);
  if (ucs && ucs >= 0x20 && ucs != 0x7f) { gchar buf[8]; gint n = g_unichar_to_utf8 (ucs, buf); buf[n] = 0; ev->key.string = g_strdup (buf); ev->key.length = n; }
  else { ev->key.string = g_strdup (""); ev->key.length = 0; }
  queue_event (display, ev);
}

static void
translate_key (GdkDisplay *display, const PheWsEvent *e)
{
  if (e->ptype == 1) { kstate.down_pending = TRUE; kstate.scancode = e->scancode; modifier_update (e->scancode, TRUE); return; }
  if (e->ptype == 0) {
    if (!kstate.down_pending) return;
    kstate.down_pending = FALSE; kstate.last_keyval = keysym_of (e->code, kstate.scancode); queue_key (display, TRUE, kstate.last_keyval, kstate.scancode); return;
  }
  /* key up */
  if (kstate.down_pending) { kstate.down_pending = FALSE; kstate.last_keyval = keysym_of (0, kstate.scancode); queue_key (display, TRUE, kstate.last_keyval, kstate.scancode); }
  queue_key (display, FALSE, kstate.last_keyval ? kstate.last_keyval : keysym_of (0, e->scancode), e->scancode);
  kstate.last_keyval = 0; modifier_update (e->scancode, FALSE);
}

/* ---- interactive move / resize (gdk_window_begin_move_drag / begin_resize_drag): with no window manager the backend follows the pointer itself until the button goes up ---- */
static struct { gboolean active; GdkWindow *window; gint mode; gint start_x, start_y; gint wx, wy, ww, wh; } idrag;

void
_gdk_symbian_drag_begin (GdkWindow *window, gint mode, gint root_x, gint root_y)
{
  gint ox, oy, w, h;
  gdk_window_get_origin (window, &ox, &oy); gdk_drawable_get_size (GDK_DRAWABLE (window), &w, &h);
  idrag.active = TRUE; idrag.window = window; idrag.mode = mode; idrag.start_x = root_x; idrag.start_y = root_y; idrag.wx = ox; idrag.wy = oy; idrag.ww = w; idrag.wh = h;
}

static void
drag_motion (gint x, gint y)
{
  gint dx = x - idrag.start_x, dy = y - idrag.start_y, nx = idrag.wx, ny = idrag.wy, nw = idrag.ww, nh = idrag.wh;
  if (GDK_WINDOW_DESTROYED (idrag.window)) { idrag.active = FALSE; return; }
  if (idrag.mode < 0) { nx += dx; ny += dy; }
  else switch ((GdkWindowEdge) idrag.mode) {
    case GDK_WINDOW_EDGE_NORTH_WEST: nx += dx; ny += dy; nw -= dx; nh -= dy; break;
    case GDK_WINDOW_EDGE_NORTH: ny += dy; nh -= dy; break;
    case GDK_WINDOW_EDGE_NORTH_EAST: ny += dy; nw += dx; nh -= dy; break;
    case GDK_WINDOW_EDGE_WEST: nx += dx; nw -= dx; break;
    case GDK_WINDOW_EDGE_EAST: nw += dx; break;
    case GDK_WINDOW_EDGE_SOUTH_WEST: nx += dx; nw -= dx; nh += dy; break;
    case GDK_WINDOW_EDGE_SOUTH: nh += dy; break;
    case GDK_WINDOW_EDGE_SOUTH_EAST: nw += dx; nh += dy; break;
  }
  if (idrag.mode < 0) gdk_window_move (idrag.window, nx, ny); else gdk_window_move_resize (idrag.window, nx, ny, MAX (nw, 1), MAX (nh, 1));
}

void
_gdk_symbian_pointer_warp (GdkDisplay *display, gint x, gint y)
{
  PheWsEvent e; memset (&e, 0, sizeof e); e.type = PHE_EV_POINTER; e.ptype = 7; e.x = x; e.y = y;
  { extern void _gdk_symbian_translate_pointer (GdkDisplay *, const PheWsEvent *); _gdk_symbian_translate_pointer (display, &e); }
}

static void translate_pointer_inner (GdkDisplay *display, const PheWsEvent *e);
void _gdk_symbian_translate_pointer (GdkDisplay *display, const PheWsEvent *e) { translate_pointer_inner (display, e); }

static void
translate_pointer (GdkDisplay *display, const PheWsEvent *e)
{
  if (idrag.active) {
    if (e->ptype == 6 || e->ptype == 7) drag_motion (e->x, e->y);
    else if (e->ptype == 1) idrag.active = FALSE;                              /* button up ends the interactive move or resize */
  }
  translate_pointer_inner (display, e);
}

/* ---- EnterNotify / LeaveNotify of the native windows: a window server reports the pointer crossing from one native window to another; GDK derives the crossings between its client-side
 * children from them (without the first enter a toolkit never learns which widget the pointer is in, and a button ignores a release it did not see the pointer enter for) ---- */
static GdkWindow *entered_win = NULL;
static void entered_gone (gpointer data, GObject *where) { (void) data; if (entered_win == (GdkWindow *) where) entered_win = NULL; }

static void
queue_crossing (GdkDisplay *display, GdkEventType type, GdkWindow *w, gint x, gint y, gint xr, gint yr)
{
  GdkEvent *ev = gdk_event_new (type); GdkDisplaySymbian *d = GDK_DISPLAY_SYMBIAN (display);
  ev->crossing.window = g_object_ref (w); ev->crossing.send_event = FALSE; ev->crossing.subwindow = NULL; ev->crossing.time = now_ms ();
  ev->crossing.x = x; ev->crossing.y = y; ev->crossing.x_root = xr; ev->crossing.y_root = yr;
  ev->crossing.mode = GDK_CROSSING_NORMAL; ev->crossing.detail = GDK_NOTIFY_NONLINEAR; ev->crossing.focus = FALSE; ev->crossing.state = d->pointer_state | key_state;
  queue_event (display, ev);
}

static void
update_entered (GdkDisplay *display, GdkWindow *win, gint wx, gint wy, gint xr, gint yr)
{
  if (win == entered_win) return;
  if (entered_win && !GDK_WINDOW_DESTROYED (entered_win)) {
    gint ox = 0, oy = 0; gdk_window_get_origin (entered_win, &ox, &oy);
    queue_crossing (display, GDK_LEAVE_NOTIFY, entered_win, xr - ox, yr - oy, xr, yr);
  }
  if (entered_win) g_object_weak_unref (G_OBJECT (entered_win), entered_gone, NULL);
  entered_win = NULL;
  if (win && win != _gdk_parent_root) { entered_win = win; g_object_weak_ref (G_OBJECT (win), entered_gone, NULL); queue_crossing (display, GDK_ENTER_NOTIFY, win, wx, wy, xr, yr); }
}

static void
translate_pointer_inner (GdkDisplay *display, const PheWsEvent *e)
{
  GdkDisplaySymbian *d = GDK_DISPLAY_SYMBIAN (display); GdkWindow *win; gint wx = 0, wy = 0; GdkEvent *ev; guint before = d->pointer_state;
  d->pointer_x = e->x; d->pointer_y = e->y;
  win = _gdk_windowing_window_at_pointer (display, &wx, &wy, NULL, TRUE);
  update_entered (display, win, wx, wy, e->x, e->y);
  switch (e->ptype) {
    case 0: /* EButton1Down */
      ev = gdk_event_new (GDK_BUTTON_PRESS);
      ev->button.state = before | key_state; ev->button.button = 1; d->pointer_state |= GDK_BUTTON1_MASK;
      break;
    case 1: /* EButton1Up */
      ev = gdk_event_new (GDK_BUTTON_RELEASE);
      ev->button.state = before | key_state; ev->button.button = 1; d->pointer_state &= ~GDK_BUTTON1_MASK;
      break;
    case 6: case 7: /* EDrag, EMove */
      ev = gdk_event_new (GDK_MOTION_NOTIFY);
      ev->motion.state = before | key_state; ev->motion.is_hint = FALSE; ev->motion.device = display->core_pointer;
      ev->motion.x = wx; ev->motion.y = wy; ev->motion.x_root = e->x; ev->motion.y_root = e->y; ev->motion.axes = NULL;
      ev->motion.window = g_object_ref (win); ev->motion.send_event = FALSE; ev->motion.time = now_ms ();
      queue_event (display, ev); return;
    default: return;
  }
  ev->button.window = g_object_ref (win); ev->button.send_event = FALSE; ev->button.time = now_ms (); ev->button.device = display->core_pointer; ev->button.axes = NULL;
  ev->button.x = wx; ev->button.y = wy; ev->button.x_root = e->x; ev->button.y_root = e->y;
  queue_event (display, ev);
  if (ev->type == GDK_BUTTON_PRESS) _gdk_event_button_generate (display, ev);     /* double and triple clicks */
}

void
_gdk_events_queue (GdkDisplay *display)
{
  PheWsEvent e; gchar sink[64]; GdkDisplaySymbian *d = GDK_DISPLAY_SYMBIAN (display);
  (void) d;
  _gdk_symbian_focus_update ();
  while (read (phe_ws_wake_fd (), sink, sizeof sink) > 0) ;
  while (phe_ws_poll (&e)) {
    if (e.type == PHE_EV_KEY) translate_key (display, &e);
    else if (e.type == PHE_EV_POINTER) translate_pointer (display, &e);
  }
}

/* ---- the event source ---- */
typedef struct { GSource base; GPollFD pfd; GdkDisplay *display; } GdkEventSource;

static gboolean
event_prepare (GSource *source, gint *timeout)
{
  GdkEventSource *s = (GdkEventSource *) source; gboolean ready;
  *timeout = -1;
  _gdk_events_queue (s->display);
  ready = _gdk_event_queue_find_first (s->display) != NULL;
  return ready;
}
static gboolean
event_check (GSource *source)
{
  GdkEventSource *s = (GdkEventSource *) source;
  if (s->pfd.revents & G_IO_IN) _gdk_events_queue (s->display);
  return _gdk_event_queue_find_first (s->display) != NULL;
}
static gboolean
event_dispatch (GSource *source, GSourceFunc callback, gpointer user_data)
{
  GdkEventSource *s = (GdkEventSource *) source; GdkEvent *event; (void) callback; (void) user_data;
  event = gdk_display_get_event (s->display);
  if (event) { if (_gdk_event_func) (*_gdk_event_func) (event, _gdk_event_data); gdk_event_free (event); }
  return TRUE;
}
static GSourceFuncs event_funcs = { event_prepare, event_check, event_dispatch, NULL, NULL, NULL };

void
_gdk_events_init (void)
{
  GSource *source = g_source_new (&event_funcs, sizeof (GdkEventSource)); GdkEventSource *s = (GdkEventSource *) source;
  s->display = _gdk_display; s->pfd.fd = phe_ws_wake_fd (); s->pfd.events = G_IO_IN; s->pfd.revents = 0;
  g_source_add_poll (source, &s->pfd);
  g_source_set_priority (source, GDK_PRIORITY_EVENTS);
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);
  GDK_DISPLAY_SYMBIAN (_gdk_display)->event_source = source;
}

/* ---- the rest of gdkinternals.h's event interface ---- */
void _gdk_windowing_event_data_copy (const GdkEvent *src, GdkEvent *dst) { (void) src; (void) dst; }   /* this backend attaches no data of its own to an event: nothing to copy */
void _gdk_windowing_event_data_free (GdkEvent *event) { (void) event; }                               /* ... and nothing to free */

/* ---- client message filters (gdk_add_client_message_filter): called for client messages of a type before they are delivered ---- */
typedef struct { GdkAtom message_type; GdkFilterFunc func; gpointer data; } ClientFilter;
static GSList *client_filters = NULL;
void gdk_display_add_client_message_filter (GdkDisplay *display, GdkAtom message_type, GdkFilterFunc func, gpointer data)
{ ClientFilter *f = g_new (ClientFilter, 1); (void) display; f->message_type = message_type; f->func = func; f->data = data; client_filters = g_slist_append (client_filters, f); }
void gdk_add_client_message_filter (GdkAtom message_type, GdkFilterFunc func, gpointer data) { gdk_display_add_client_message_filter (gdk_display_get_default (), message_type, func, data); }

/* returns FALSE when a filter removed the event */
static gboolean
run_client_filters (GdkEvent *ev)
{
  GSList *l;
  for (l = client_filters; l; l = l->next) {
    ClientFilter *f = l->data;
    if (f->message_type == ev->client.message_type) {
      GdkFilterReturn r = f->func ((GdkXEvent *) ev, ev, f->data);
      if (r == GDK_FILTER_REMOVE) return FALSE;
      if (r == GDK_FILTER_TRANSLATE) return TRUE;                              /* the filter filled in the GdkEvent itself */
    }
  }
  return TRUE;
}

/* Client messages go to windows of this process (there are no other GDK clients on the display): the target is the native id of a window, which is its pointer. */
gboolean
gdk_event_send_client_message_for_display (GdkDisplay *display, GdkEvent *event, guint32 xid)
{
  GdkWindow *win;
  g_return_val_if_fail (event != NULL, FALSE);
  win = gdk_window_lookup_for_display (display, (GdkNativeWindow) xid);
  if (!win || GDK_WINDOW_DESTROYED (win)) return FALSE;
  { GdkEvent *copy = gdk_event_copy (event);
    if (copy->any.window) g_object_unref (copy->any.window);
    copy->any.window = g_object_ref (win); copy->any.send_event = TRUE;
    if (run_client_filters (copy)) gdk_event_put (copy);
    gdk_event_free (copy); }
  return TRUE;
}

GdkWindow *_gdk_symbian_focus_window (void) { return focus_window && !GDK_WINDOW_DESTROYED (focus_window) && focus_window != _gdk_parent_root ? focus_window : NULL; }

/* ---- synthetic input for gdk_test_simulate_*: the same events a key or tap from the window server becomes ---- */
void
_gdk_symbian_inject_key (GdkWindow *window, guint keyval, guint state, gboolean press)
{
  GdkDisplay *display = gdk_window_get_display (window);
  GdkWindow *saved = focus_window; guint saved_state = key_state;
  focus_window = window; key_state = state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK);
  queue_key (display, press, keyval, 0);
  focus_window = saved; key_state = saved_state;
}

void
_gdk_symbian_inject_button (GdkWindow *window, gint x, gint y, guint button, guint state, gboolean press)
{
  GdkDisplay *display = gdk_window_get_display (window); GdkDisplaySymbian *d = GDK_DISPLAY_SYMBIAN (display);
  GdkEvent *ev = gdk_event_new (press ? GDK_BUTTON_PRESS : GDK_BUTTON_RELEASE); gint ox = 0, oy = 0;
  gdk_window_get_origin (window, &ox, &oy);
  ev->button.window = g_object_ref (window); ev->button.send_event = FALSE; ev->button.time = now_ms (); ev->button.device = display->core_pointer; ev->button.axes = NULL;
  ev->button.x = x; ev->button.y = y; ev->button.x_root = ox + x; ev->button.y_root = oy + y; ev->button.button = button;
  ev->button.state = state; d->pointer_x = ox + x; d->pointer_y = oy + y;
  if (button >= 1 && button <= 5) { guint m = GDK_BUTTON1_MASK << (button - 1); if (press) d->pointer_state |= m; else d->pointer_state &= ~m; }
  queue_event (display, ev);
  if (press) _gdk_event_button_generate (display, ev);
}

gboolean
gdk_test_simulate_key (GdkWindow *window, gint x, gint y, guint keyval, GdkModifierType modifiers, GdkEventType key_pressrelease)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);
  (void) x; (void) y;
  _gdk_symbian_inject_key (window, keyval, modifiers, key_pressrelease == GDK_KEY_PRESS);
  return TRUE;
}

gboolean
gdk_test_simulate_button (GdkWindow *window, gint x, gint y, guint button, GdkModifierType modifiers, GdkEventType button_pressrelease)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);
  _gdk_symbian_inject_button (window, x, y, button, modifiers, button_pressrelease == GDK_BUTTON_PRESS);
  return TRUE;
}

/* everything is presented by the time the display is synchronised */
void gdk_test_render_sync (GdkWindow *window) { g_return_if_fail (GDK_IS_WINDOW (window)); gdk_window_process_all_updates (); phe_ws_sync (); }
