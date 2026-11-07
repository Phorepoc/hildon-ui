/* phehildon_gconf.c -- the in-process configuration store behind shims/gconf/gconf-client.h. A GConfClient is a GObject (callers g_object_unref it); the default client is shared.
 * A key "/a/b/c" is the key "c" of the group "/a/b". An unset key reads as FALSE / 0 / NULL without an error, as GConf does. BSD-2-Clause (Phorepoc). */
#include <string.h>
#include <glib-object.h>
#include "gconf/gconf-client.h"

struct _GConfClient { GObject parent; };
typedef struct { GObjectClass parent_class; } GConfClientClass;
G_DEFINE_TYPE (GConfClient, gconf_client, G_TYPE_OBJECT)
static void gconf_client_init (GConfClient *c) { (void) c; }
static void gconf_client_class_init (GConfClientClass *k) { (void) k; }

static GKeyFile *store = NULL; static gchar *store_path = NULL;
static GConfClient *default_client = NULL;

static void
open_store (void)
{
  if (store) return;
  store = g_key_file_new (); store_path = g_strdup (g_getenv ("PHE_GCONF_FILE"));
  if (store_path) g_key_file_load_from_file (store, store_path, G_KEY_FILE_NONE, NULL);
}

static gboolean
split (const gchar *key, gchar **group, gchar **name)
{
  const gchar *slash = key ? strrchr (key, '/') : NULL;
  if (!slash || slash == key) return FALSE;
  *group = g_strndup (key, slash - key); *name = g_strdup (slash + 1);
  return TRUE;
}

static void save_store (void) { if (store_path) { gsize n; gchar *d = g_key_file_to_data (store, &n, NULL); g_file_set_contents (store_path, d, n, NULL); g_free (d); } }

GConfClient *
gconf_client_get_default (void)
{
  if (!default_client) default_client = g_object_new (gconf_client_get_type (), NULL);
  return g_object_ref (default_client);
}

#define GETTER(NAME, TYPE, ZERO, CALL) TYPE NAME (GConfClient *client, const gchar *key, GError **err) \
  { gchar *g, *n; TYPE v = ZERO; (void) client; (void) err; open_store (); \
    if (split (key, &g, &n)) { if (g_key_file_has_key (store, g, n, NULL)) v = CALL (store, g, n, NULL); g_free (g); g_free (n); } return v; }
#define SETTER(NAME, TYPE, CALL) gboolean NAME (GConfClient *client, const gchar *key, TYPE val, GError **err) \
  { gchar *g, *n; (void) client; (void) err; open_store (); if (!split (key, &g, &n)) return FALSE; CALL (store, g, n, val); g_free (g); g_free (n); save_store (); return TRUE; }

GETTER (gconf_client_get_bool, gboolean, FALSE, g_key_file_get_boolean)
GETTER (gconf_client_get_int, gint, 0, g_key_file_get_integer)
GETTER (gconf_client_get_string, gchar *, NULL, g_key_file_get_string)
SETTER (gconf_client_set_bool, gboolean, g_key_file_set_boolean)
SETTER (gconf_client_set_int, gint, g_key_file_set_integer)
SETTER (gconf_client_set_string, const gchar *, g_key_file_set_string)
