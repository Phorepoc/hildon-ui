/* gdkproperty-symbian.c -- atoms, window properties and the text-property conversions of the Symbian backend. There is no window server property store behind GDK here, so the properties
 * live in the window implementations of this process (as in the DirectFB backend); a change or deletion is announced with a GDK_PROPERTY_NOTIFY event when the window selects it.
 * Text conversions: ASCII text is STRING, Latin-1 and other text is COMPOUND_TEXT in the ISO 2022 forms GTK/Xlib use (G1 = Latin-1 right half; everything else in the
 * extended UTF-8 segment ESC % G ... ESC % @). BSD-2-Clause (Phorepoc). */
#include "config.h"
#include <string.h>
#include "gdksymbian.h"
#include "gdkproperty.h"
#include "gdkselection.h"
#include "gdkprivate.h"
#include "gdkinternals.h"

/* ---- atoms ---- */
static GHashTable *names_to_atoms = NULL;
static GPtrArray *atoms_to_names = NULL;

/* the predefined X atoms 1..68 (GDK_NONE is 0), in X protocol order; atom 69 is CLIPBOARD (GDK_SELECTION_CLIPBOARD) */
static const char *const predefined[] = {
  "NONE", "PRIMARY", "SECONDARY", "ARC", "ATOM", "BITMAP", "CARDINAL", "COLORMAP", "CURSOR", "CUT_BUFFER0", "CUT_BUFFER1", "CUT_BUFFER2", "CUT_BUFFER3", "CUT_BUFFER4",
  "CUT_BUFFER5", "CUT_BUFFER6", "CUT_BUFFER7", "DRAWABLE", "FONT", "INTEGER", "PIXMAP", "POINT", "RECTANGLE", "RESOURCE_MANAGER", "RGB_COLOR_MAP", "RGB_BEST_MAP",
  "RGB_BLUE_MAP", "RGB_DEFAULT_MAP", "RGB_GRAY_MAP", "RGB_GREEN_MAP", "RGB_RED_MAP", "STRING", "VISUALID", "WINDOW", "WM_COMMAND", "WM_HINTS", "WM_CLIENT_MACHINE",
  "WM_ICON_NAME", "WM_ICON_SIZE", "WM_NAME", "WM_NORMAL_HINTS", "WM_SIZE_HINTS", "WM_ZOOM_HINTS", "MIN_SPACE", "NORM_SPACE", "MAX_SPACE", "END_SPACE", "SUPERSCRIPT_X",
  "SUPERSCRIPT_Y", "SUBSCRIPT_X", "SUBSCRIPT_Y", "UNDERLINE_POSITION", "UNDERLINE_THICKNESS", "STRIKEOUT_ASCENT", "STRIKEOUT_DESCENT", "ITALIC_ANGLE", "X_HEIGHT",
  "QUAD_WIDTH", "WEIGHT", "POINT_SIZE", "RESOLUTION", "COPYRIGHT", "NOTICE", "FONT_NAME", "FAMILY_NAME", "FULL_NAME", "CAP_HEIGHT", "WM_CLASS", "WM_TRANSIENT_FOR",
  "CLIPBOARD"
};

static void
ensure_atoms (void)
{
  guint i;
  if (names_to_atoms) return;
  names_to_atoms = g_hash_table_new (g_str_hash, g_str_equal);
  atoms_to_names = g_ptr_array_sized_new (G_N_ELEMENTS (predefined) + 32);
  for (i = 0; i < G_N_ELEMENTS (predefined); i++) {
    g_hash_table_insert (names_to_atoms, (gchar *) predefined[i], GUINT_TO_POINTER (i));
    g_ptr_array_add (atoms_to_names, (gchar *) predefined[i]);
  }
}

static GdkAtom
intern_atom (const gchar *name, gboolean allocate, gboolean only_if_exists)
{
  gpointer result; gchar *n;
  g_return_val_if_fail (name != NULL, GDK_NONE);
  ensure_atoms ();
  if (g_hash_table_lookup_extended (names_to_atoms, name, NULL, &result)) return result;
  if (only_if_exists) return GDK_NONE;
  result = GUINT_TO_POINTER (atoms_to_names->len);
  n = allocate ? g_strdup (name) : (gchar *) name;
  g_hash_table_insert (names_to_atoms, n, result);
  g_ptr_array_add (atoms_to_names, n);
  return result;
}

GdkAtom gdk_atom_intern (const gchar *atom_name, gboolean only_if_exists) { return intern_atom (atom_name, TRUE, only_if_exists); }
GdkAtom gdk_atom_intern_static_string (const gchar *atom_name) { return intern_atom (atom_name, FALSE, FALSE); }
gchar *
gdk_atom_name (GdkAtom atom)
{
  ensure_atoms ();
  if (GPOINTER_TO_UINT (atom) >= atoms_to_names->len) return NULL;
  return g_strdup (g_ptr_array_index (atoms_to_names, GPOINTER_TO_UINT (atom)));
}

/* ---- window properties ---- */
typedef struct { GdkAtom type; gint format; gint length; guchar *data; } WindowProperty;       /* length in bytes */

static void free_property (gpointer p) { WindowProperty *wp = p; g_free (wp->data); g_free (wp); }

static GHashTable *
properties_of (GdkWindow *window, gboolean create)
{
  GdkWindowImplSymbian *impl = (GdkWindowImplSymbian *) GDK_WINDOW_OBJECT (window)->impl;
  if (!impl->properties && create) impl->properties = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, free_property);
  return impl->properties;
}

static void
notify_property (GdkWindow *window, GdkAtom property, GdkPropertyState state)
{
  GdkEvent *ev;
  if (!(gdk_window_get_events (window) & GDK_PROPERTY_CHANGE_MASK)) return;
  ev = gdk_event_new (GDK_PROPERTY_NOTIFY);
  ev->property.window = g_object_ref (window); ev->property.send_event = FALSE; ev->property.atom = property; ev->property.time = (guint32) (g_get_monotonic_time () / 1000); ev->property.state = state;
  gdk_event_put (ev); gdk_event_free (ev);
}

void
gdk_property_delete (GdkWindow *window, GdkAtom property)
{
  GHashTable *t;
  g_return_if_fail (GDK_IS_WINDOW (window));
  t = properties_of (window, FALSE);
  if (!t || !g_hash_table_lookup (t, property)) return;
  g_hash_table_remove (t, property);
  notify_property (window, property, GDK_PROPERTY_DELETE);
}

gboolean
gdk_property_get (GdkWindow *window, GdkAtom property, GdkAtom type, gulong offset, gulong length, gint pdelete, GdkAtom *actual_property_type, gint *actual_format_type, gint *actual_length, guchar **data)
{
  GHashTable *t; WindowProperty *prop; gint nbytes;
  g_return_val_if_fail (window == NULL || GDK_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (data != NULL, FALSE);
  if (!window) window = _gdk_parent_root;
  if (GDK_WINDOW_DESTROYED (window)) return FALSE;
  *data = NULL;
  if (actual_length) *actual_length = 0;
  t = properties_of (window, FALSE); prop = t ? g_hash_table_lookup (t, property) : NULL;
  if (!prop) { if (actual_property_type) *actual_property_type = GDK_NONE; return FALSE; }
  if (actual_property_type) *actual_property_type = prop->type;
  if (actual_format_type) *actual_format_type = prop->format;
  if (type != GDK_NONE && prop->type != type) return TRUE;                    /* exists, but of another type: the caller sees the type and gets no data (as X does) */
  nbytes = (gint) MIN ((gulong) MAX (prop->length - (gint) (offset * 4), 0), length);
  if (nbytes > 0) { *data = g_malloc (nbytes + 1); memcpy (*data, prop->data + offset * 4, nbytes); (*data)[nbytes] = 0; }
  if (actual_length) *actual_length = nbytes;
  if (pdelete && *data && (gulong) nbytes + offset * 4 >= (gulong) prop->length) gdk_property_delete (window, property);       /* only when it was read completely */
  return TRUE;
}

void
gdk_property_change (GdkWindow *window, GdkAtom property, GdkAtom type, gint format, GdkPropMode mode, const guchar *data, gint nelements)
{
  GHashTable *t; WindowProperty *prop; gint bytes = nelements * (format / 8);
  g_return_if_fail (window == NULL || GDK_IS_WINDOW (window));
  g_return_if_fail (format == 8 || format == 16 || format == 32);
  if (!window) window = _gdk_parent_root;
  if (GDK_WINDOW_DESTROYED (window)) return;
  t = properties_of (window, TRUE); prop = g_hash_table_lookup (t, property);
  if (!prop || mode == GDK_PROP_MODE_REPLACE) {
    if (!prop) { prop = g_new0 (WindowProperty, 1); g_hash_table_insert (t, property, prop); }
    g_free (prop->data);
    prop->data = g_malloc (bytes + 1); if (bytes) memcpy (prop->data, data, bytes); prop->data[bytes] = 0; prop->length = bytes; prop->type = type; prop->format = format;
  } else {
    guchar *nd;
    if (prop->type != type || prop->format != format) { g_warning ("gdk_property_change: appending/prepending to a property of another type or format"); return; }
    nd = g_malloc (prop->length + bytes + 1);
    if (mode == GDK_PROP_MODE_APPEND) { memcpy (nd, prop->data, prop->length); if (bytes) memcpy (nd + prop->length, data, bytes); }
    else { if (bytes) memcpy (nd, data, bytes); memcpy (nd + bytes, prop->data, prop->length); }
    nd[prop->length + bytes] = 0; g_free (prop->data); prop->data = nd; prop->length += bytes;
  }
  notify_property (window, property, GDK_PROPERTY_NEW_VALUE);
}

/* ---- text property conversions ---- */
static GdkAtom compound_text_atom (void) { return gdk_atom_intern_static_string ("COMPOUND_TEXT"); }
static GdkAtom utf8_string_atom (void) { return gdk_atom_intern_static_string ("UTF8_STRING"); }

/* UTF-8 -> STRING (ASCII only) or COMPOUND_TEXT */
gboolean
gdk_utf8_to_compound_text_for_display (GdkDisplay *display, const gchar *str, GdkAtom *encoding, gint *format, guchar **ctext, gint *length)
{
  GString *out; const gchar *p; gboolean ascii = TRUE, g1_latin1 = FALSE, in_utf8 = FALSE;
  (void) display;
  g_return_val_if_fail (str != NULL, FALSE);
  if (!g_utf8_validate (str, -1, NULL)) return FALSE;
  for (p = str; *p; p = g_utf8_next_char (p)) if (g_utf8_get_char (p) >= 0x80) { ascii = FALSE; break; }
  if (encoding) *encoding = ascii ? GDK_TARGET_STRING : compound_text_atom ();
  if (format) *format = 8;
  out = g_string_new (NULL);
  for (p = str; *p; p = g_utf8_next_char (p)) {
    gunichar c = g_utf8_get_char (p);
    if (c < 0x80) { if (in_utf8) { g_string_append (out, "\033%@"); in_utf8 = FALSE; } g_string_append_c (out, (gchar) c); }
    else if (c >= 0xa0 && c <= 0xff) {
      if (in_utf8) { g_string_append (out, "\033%@"); in_utf8 = FALSE; }
      if (!g1_latin1) { g_string_append (out, "\033-A"); g1_latin1 = TRUE; }
      g_string_append_c (out, (gchar) c);
    } else {
      gchar buf[8]; gint n = g_unichar_to_utf8 (c, buf);
      if (!in_utf8) { g_string_append (out, "\033%G"); in_utf8 = TRUE; }
      g_string_append_len (out, buf, n);
    }
  }
  if (in_utf8) g_string_append (out, "\033%@");
  if (length) *length = (gint) out->len;
  if (ctext) *ctext = (guchar *) g_string_free (out, FALSE); else g_string_free (out, TRUE);
  return TRUE;
}

/* a string in the locale charset */
gint
gdk_string_to_compound_text_for_display (GdkDisplay *display, const gchar *str, GdkAtom *encoding, gint *format, guchar **ctext, gint *length)
{
  gchar *utf8; gboolean ok;
  g_return_val_if_fail (str != NULL, 1);
  utf8 = g_locale_to_utf8 (str, -1, NULL, NULL, NULL);
  if (!utf8) return 1;
  ok = gdk_utf8_to_compound_text_for_display (display, utf8, encoding, format, ctext, length);
  g_free (utf8);
  return ok ? 0 : 1;
}

void gdk_free_compound_text (guchar *ctext) { g_free (ctext); }
void gdk_free_text_list (gchar **list) { g_strfreev (list); }

/* UTF-8 -> Latin-1 for the STRING selection target; characters that Latin-1 cannot hold are dropped, as the selection convention says */
gchar *
gdk_utf8_to_string_target (const gchar *str)
{
  GString *out = g_string_new (NULL); const gchar *p;
  g_return_val_if_fail (str != NULL, NULL);
  for (p = str; *p; p = g_utf8_next_char (p)) { gunichar c = g_utf8_get_char (p); if (c <= 0xff) g_string_append_c (out, (gchar) c); }
  return g_string_free (out, FALSE);
}

/* the characters of a COMPOUND_TEXT value as UTF-8: G0 = ASCII, G1 = Latin-1 right half after ESC - A, the extended UTF-8 segment ESC % G ... ESC % @; other escape sequences are skipped */
static gchar *
compound_text_to_utf8 (const guchar *text, gint length)
{
  GString *out = g_string_new (NULL); gint i = 0; gboolean utf8 = FALSE, g1_latin1 = FALSE;
  while (i < length) {
    guchar c = text[i];
    if (c == 0x1b) {
      if (i + 2 < length && text[i + 1] == '%' && text[i + 2] == 'G') { utf8 = TRUE; i += 3; continue; }
      if (i + 2 < length && text[i + 1] == '%' && text[i + 2] == '@') { utf8 = FALSE; i += 3; continue; }
      if (i + 2 < length && text[i + 1] == '-' && text[i + 2] == 'A') { g1_latin1 = TRUE; i += 3; continue; }
      i += 2; while (i < length && text[i] >= 0x20 && text[i] <= 0x2f) i++;                  /* any other ESC sequence: intermediates then a final byte */
      if (i < length) i++;
      continue;
    }
    if (utf8) { gint n = 1; if (c >= 0xf0) n = 4; else if (c >= 0xe0) n = 3; else if (c >= 0xc0) n = 2; if (i + n > length) break; g_string_append_len (out, (const gchar *) text + i, n); i += n; continue; }
    if (c < 0x80) g_string_append_c (out, (gchar) c);
    else if (c >= 0xa0 && g1_latin1) g_string_append_unichar (out, c);
    i++;
  }
  return g_string_free (out, FALSE);
}

/* a text property value -> the UTF-8 strings (the value holds NUL separated strings); returns how many */
gint
gdk_text_property_to_utf8_list_for_display (GdkDisplay *display, GdkAtom encoding, gint format, const guchar *text, gint length, gchar ***list)
{
  GPtrArray *a = g_ptr_array_new (); gint i = 0, start = 0;
  (void) display;
  g_return_val_if_fail (list != NULL, 0);
  if (format != 8 || (encoding != GDK_TARGET_STRING && encoding != utf8_string_atom () && encoding != compound_text_atom ())) { *list = NULL; g_ptr_array_free (a, TRUE); return 0; }
  while (start <= length) {
    gchar *s = NULL;
    for (i = start; i < length && text[i]; i++) ;
    if (i == length && start == length) break;                                                 /* no empty trailing string after the last NUL */
    if (encoding == GDK_TARGET_STRING) { GString *g = g_string_new (NULL); gint k; for (k = start; k < i; k++) g_string_append_unichar (g, text[k]); s = g_string_free (g, FALSE); }
    else if (encoding == utf8_string_atom ()) { s = g_strndup ((const gchar *) text + start, i - start); if (!g_utf8_validate (s, -1, NULL)) { g_free (s); s = NULL; } }
    else s = compound_text_to_utf8 (text + start, i - start);
    if (s) g_ptr_array_add (a, s);
    start = i + 1;
  }
  { gint n = (gint) a->len; g_ptr_array_add (a, NULL); *list = (gchar **) g_ptr_array_free (a, FALSE); return n; }
}

/* the same, in the locale charset */
gint
gdk_text_property_to_text_list_for_display (GdkDisplay *display, GdkAtom encoding, gint format, const guchar *text, gint length, gchar ***list)
{
  gchar **u; gint n, i, j = 0; GPtrArray *a;
  n = gdk_text_property_to_utf8_list_for_display (display, encoding, format, text, length, &u);
  if (n == 0) { *list = NULL; return 0; }
  a = g_ptr_array_new ();
  for (i = 0; i < n; i++) { gchar *l = g_locale_from_utf8 (u[i], -1, NULL, NULL, NULL); if (l) { g_ptr_array_add (a, l); j++; } }
  g_strfreev (u);
  g_ptr_array_add (a, NULL); *list = (gchar **) g_ptr_array_free (a, FALSE);
  return j;
}
