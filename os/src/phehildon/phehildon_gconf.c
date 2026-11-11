/* Copyright The Phorepoc Authors */
/* SPDX-License-Identifier: BSD-2-Clause */
/* phehildon_gconf.c -- the in-process configuration store behind shims/gconf/gconf-client.h. A GConfClient is a GObject (callers g_object_unref it); the default client is shared.
 * A key is a path "/a/b/c"; values are bool, int, string or a list of strings. An unset key reads as FALSE / 0 / NULL without an error, as GConf does. Setting or unsetting a key calls the
 * notifications registered on the key or on one of its parent directories. With $PHE_GCONF_FILE the store is a GKeyFile (group = the directory of the key, "b:1" / "i:5" / "s:text" for
 * scalars, a string list for a list value) loaded when first used and saved on every change. BSD-2-Clause (Phorepoc). */
#include <string.h>
#include <stdlib.h>
#include <glib-object.h>
#include "gconf/gconf-client.h"

struct _GConfClient { GObject parent; };
typedef struct { GObjectClass parent_class; } GConfClientClass;
G_DEFINE_TYPE (GConfClient, gconf_client, G_TYPE_OBJECT)
static void gconf_client_init (GConfClient *c) { (void) c; }
static void gconf_client_class_init (GConfClientClass *k) { (void) k; }

typedef struct { GConfValueType type; gboolean b; gint i; gchar *s; GSList *list; } Value;       /* the first member is the public GConfValue */
struct _GConfEntry { gchar *key; Value *value; };
typedef struct { guint id; gchar *section; GConfClientNotifyFunc func; gpointer data; GFreeFunc destroy; } Notify;

static GHashTable *store = NULL; static GSList *notifies = NULL; static guint next_id = 1; static gchar *store_path = NULL; static GConfClient *default_client = NULL;

static void
value_free (Value *v)
{
  if (!v) return;
  if (v->type == GCONF_VALUE_LIST) { g_slist_foreach (v->list, (GFunc) value_free, NULL); g_slist_free (v->list); }
  g_free (v->s); g_free (v);
}

static Value *string_value (const gchar *s) { Value *v = g_new0 (Value, 1); v->type = GCONF_VALUE_STRING; v->s = g_strdup (s); return v; }

static void
open_store (void)
{
  GKeyFile *kf; gchar **groups, **g;
  if (store) return;
  store = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) value_free); store_path = g_strdup (g_getenv ("PHE_GCONF_FILE"));
  if (!store_path) return;
  kf = g_key_file_new ();
  if (g_key_file_load_from_file (kf, store_path, G_KEY_FILE_NONE, NULL)) {
    groups = g_key_file_get_groups (kf, NULL);
    for (g = groups; *g; g++) {
      gchar **keys = g_key_file_get_keys (kf, *g, NULL, NULL), **k;
      for (k = keys; k && *k; k++) {
        gchar *key = g_strdup_printf ("%s/%s", *g, *k), *raw = g_key_file_get_string (kf, *g, *k, NULL); Value *v = g_new0 (Value, 1);
        if (raw && raw[0] == 'b' && raw[1] == ':') { v->type = GCONF_VALUE_BOOL; v->b = raw[2] == '1'; }
        else if (raw && raw[0] == 'i' && raw[1] == ':') { v->type = GCONF_VALUE_INT; v->i = atoi (raw + 2); }
        else if (raw && raw[0] == 's' && raw[1] == ':') { v->type = GCONF_VALUE_STRING; v->s = g_strdup (raw + 2); }
        else {                                                               /* a string list */
          gsize n = 0, j; gchar **items = g_key_file_get_string_list (kf, *g, *k, &n, NULL);
          v->type = GCONF_VALUE_LIST;
          for (j = 0; items && j < n; j++) v->list = g_slist_append (v->list, string_value (items[j]));
          g_strfreev (items);
        }
        g_hash_table_replace (store, key, v); g_free (raw);
      }
      g_strfreev (keys);
    }
    g_strfreev (groups);
  }
  g_key_file_free (kf);
}

static gboolean
split (const gchar *key, gchar **group, gchar **name)
{
  const gchar *slash = key ? strrchr (key, '/') : NULL;
  if (!slash || slash == key) return FALSE;
  *group = g_strndup (key, slash - key); *name = g_strdup (slash + 1);
  return TRUE;
}

static void
save_store (void)
{
  GKeyFile *kf; GHashTableIter it; gpointer k, v; gsize n; gchar *d;
  if (!store_path) return;
  kf = g_key_file_new (); g_hash_table_iter_init (&it, store);
  while (g_hash_table_iter_next (&it, &k, &v)) {
    gchar *g, *name, *t = NULL; Value *val = v;
    if (!split (k, &g, &name)) continue;
    if (val->type == GCONF_VALUE_BOOL) t = g_strdup_printf ("b:%d", val->b);
    else if (val->type == GCONF_VALUE_INT) t = g_strdup_printf ("i:%d", val->i);
    else if (val->type == GCONF_VALUE_STRING) t = g_strdup_printf ("s:%s", val->s ? val->s : "");
    if (t) { g_key_file_set_string (kf, g, name, t); g_free (t); }
    else if (val->type == GCONF_VALUE_LIST) {
      GSList *l; gint c = g_slist_length (val->list), i = 0; const gchar **arr = g_new0 (const gchar *, c + 1);
      for (l = val->list; l; l = l->next) arr[i++] = ((Value *) l->data)->s ? ((Value *) l->data)->s : "";
      g_key_file_set_string_list (kf, g, name, arr, c); g_free (arr);
    }
    g_free (g); g_free (name);
  }
  d = g_key_file_to_data (kf, &n, NULL); g_file_set_contents (store_path, d, n, NULL); g_free (d); g_key_file_free (kf);
}

/* a notification section matches a key when it is the key or one of its parent directories */
static gboolean
section_matches (const gchar *section, const gchar *key)
{
  gsize n = strlen (section);
  return !strncmp (section, key, n) && (key[n] == 0 || key[n] == '/' || (n && section[n - 1] == '/'));
}

static void
fire (GConfClient *client, const gchar *key)
{
  GSList *copy = g_slist_copy (notifies), *l; GConfEntry entry;
  entry.key = (gchar *) key; entry.value = g_hash_table_lookup (store, key);
  for (l = copy; l; l = l->next) { Notify *n = l->data; if (section_matches (n->section, key)) n->func (client, n->id, &entry, n->data); }
  g_slist_free (copy);
}

GConfClient *
gconf_client_get_default (void)
{
  open_store ();
  if (!default_client) default_client = g_object_new (gconf_client_get_type (), NULL);
  return g_object_ref (default_client);
}

static void set_value (GConfClient *c, const gchar *key, Value *v) { open_store (); g_hash_table_replace (store, g_strdup (key), v); save_store (); fire (c, key); }
static Value *get_value (const gchar *key, GConfValueType t) { Value *v; open_store (); v = g_hash_table_lookup (store, key); return v && v->type == t ? v : NULL; }

gboolean gconf_client_get_bool (GConfClient *c, const gchar *k, GError **e) { Value *v = get_value (k, GCONF_VALUE_BOOL); (void) c; (void) e; return v ? v->b : FALSE; }
gint gconf_client_get_int (GConfClient *c, const gchar *k, GError **e) { Value *v = get_value (k, GCONF_VALUE_INT); (void) c; (void) e; return v ? v->i : 0; }
gchar *gconf_client_get_string (GConfClient *c, const gchar *k, GError **e) { Value *v = get_value (k, GCONF_VALUE_STRING); (void) c; (void) e; return v ? g_strdup (v->s) : NULL; }
gboolean gconf_client_set_bool (GConfClient *c, const gchar *k, gboolean x, GError **e) { Value *v = g_new0 (Value, 1); (void) e; v->type = GCONF_VALUE_BOOL; v->b = x != 0; set_value (c, k, v); return TRUE; }
gboolean gconf_client_set_int (GConfClient *c, const gchar *k, gint x, GError **e) { Value *v = g_new0 (Value, 1); (void) e; v->type = GCONF_VALUE_INT; v->i = x; set_value (c, k, v); return TRUE; }
gboolean gconf_client_set_string (GConfClient *c, const gchar *k, const gchar *x, GError **e) { (void) e; set_value (c, k, string_value (x)); return TRUE; }

gboolean
gconf_client_set_list (GConfClient *c, const gchar *k, GConfValueType list_type, GSList *list, GError **e)
{
  Value *v = g_new0 (Value, 1); GSList *l;
  (void) e;
  g_return_val_if_fail (list_type == GCONF_VALUE_STRING, FALSE);
  v->type = GCONF_VALUE_LIST;
  for (l = list; l; l = l->next) v->list = g_slist_append (v->list, string_value (l->data));
  set_value (c, k, v);
  return TRUE;
}

GSList *
gconf_client_get_list (GConfClient *c, const gchar *k, GConfValueType list_type, GError **e)
{
  Value *v = get_value (k, GCONF_VALUE_LIST); GSList *l, *r = NULL;
  (void) c; (void) e; (void) list_type;
  if (!v) return NULL;
  for (l = v->list; l; l = l->next) r = g_slist_append (r, g_strdup (((Value *) l->data)->s));
  return r;
}

gboolean gconf_client_unset (GConfClient *c, const gchar *k, GError **e) { (void) e; open_store (); g_hash_table_remove (store, k); save_store (); fire (c, k); return TRUE; }
void gconf_client_suggest_sync (GConfClient *c, GError **e) { (void) c; (void) e; open_store (); save_store (); }
void gconf_client_add_dir (GConfClient *c, const gchar *d, GConfClientPreloadType p, GError **e) { (void) c; (void) d; (void) p; (void) e; }

guint
gconf_client_notify_add (GConfClient *c, const gchar *section, GConfClientNotifyFunc func, gpointer data, GFreeFunc destroy, GError **e)
{
  Notify *n = g_new0 (Notify, 1);
  (void) c; (void) e;
  n->id = next_id++; n->section = g_strdup (section); n->func = func; n->data = data; n->destroy = destroy; notifies = g_slist_append (notifies, n);
  return n->id;
}

void
gconf_client_notify_remove (GConfClient *c, guint cnxn)
{
  GSList *l;
  (void) c;
  for (l = notifies; l; l = l->next) {
    Notify *n = l->data;
    if (n->id == cnxn) { notifies = g_slist_delete_link (notifies, l); if (n->destroy) n->destroy (n->data); g_free (n->section); g_free (n); return; }
  }
}

void gconf_client_notify (GConfClient *c, const gchar *key) { open_store (); fire (c, key); }

GConfValue *gconf_entry_get_value (const GConfEntry *e) { return (GConfValue *) e->value; }
const gchar *gconf_entry_get_key (const GConfEntry *e) { return e->key; }
GConfValueType gconf_value_get_list_type (const GConfValue *v) { (void) v; return GCONF_VALUE_STRING; }                   /* only lists of strings can be stored */
GSList *gconf_value_get_list (const GConfValue *v) { return ((const Value *) v)->list; }
const gchar *gconf_value_get_string (const GConfValue *v) { return ((const Value *) v)->s; }
