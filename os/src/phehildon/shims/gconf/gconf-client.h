/* shim: the part of GConf libhildon and libhildondesktop use -- typed get / set of keys by path, string lists, change notification -- implemented in phehildon_gconf.c as an in-process key store:
 * there is no GConf daemon here, so the "configuration database" lives for the life of the process (and is loaded from / saved to the GKeyFile $PHE_GCONF_FILE when that names a file).
 * BSD-2-Clause (Phorepoc). */
#ifndef PHE_GCONF_CLIENT_H
#define PHE_GCONF_CLIENT_H
#include <glib-object.h>
G_BEGIN_DECLS
typedef enum { GCONF_VALUE_INVALID, GCONF_VALUE_STRING, GCONF_VALUE_INT, GCONF_VALUE_FLOAT, GCONF_VALUE_BOOL, GCONF_VALUE_SCHEMA, GCONF_VALUE_LIST, GCONF_VALUE_PAIR } GConfValueType;
typedef struct _GConfValue { GConfValueType type; } GConfValue;                  /* the rest of a value is private */
typedef struct _GConfEntry GConfEntry;
typedef struct _GConfClient GConfClient;
typedef void (*GConfClientNotifyFunc) (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data);
typedef enum { GCONF_CLIENT_PRELOAD_NONE, GCONF_CLIENT_PRELOAD_ONELEVEL, GCONF_CLIENT_PRELOAD_RECURSIVE } GConfClientPreloadType;

GConfClient *gconf_client_get_default (void);
gboolean     gconf_client_get_bool   (GConfClient *client, const gchar *key, GError **err);
gint         gconf_client_get_int    (GConfClient *client, const gchar *key, GError **err);
gchar       *gconf_client_get_string (GConfClient *client, const gchar *key, GError **err);
GSList      *gconf_client_get_list   (GConfClient *client, const gchar *key, GConfValueType list_type, GError **err);    /* a list of newly allocated strings (GCONF_VALUE_STRING) */
gboolean     gconf_client_set_bool   (GConfClient *client, const gchar *key, gboolean val, GError **err);
gboolean     gconf_client_set_int    (GConfClient *client, const gchar *key, gint val, GError **err);
gboolean     gconf_client_set_string (GConfClient *client, const gchar *key, const gchar *val, GError **err);
gboolean     gconf_client_set_list   (GConfClient *client, const gchar *key, GConfValueType list_type, GSList *list, GError **err);
gboolean     gconf_client_unset      (GConfClient *client, const gchar *key, GError **err);
void         gconf_client_suggest_sync (GConfClient *client, GError **err);
guint        gconf_client_notify_add (GConfClient *client, const gchar *namespace_section, GConfClientNotifyFunc func, gpointer user_data, GFreeFunc destroy_notify, GError **err);
void         gconf_client_notify_remove (GConfClient *client, guint cnxn);
void         gconf_client_notify     (GConfClient *client, const gchar *key);
void         gconf_client_add_dir    (GConfClient *client, const gchar *dir, GConfClientPreloadType preload, GError **err);

GConfValue  *gconf_entry_get_value (const GConfEntry *entry);
const gchar *gconf_entry_get_key (const GConfEntry *entry);
GConfValueType gconf_value_get_list_type (const GConfValue *value);
GSList      *gconf_value_get_list (const GConfValue *value);                      /* of GConfValue * */
const gchar *gconf_value_get_string (const GConfValue *value);
G_END_DECLS
#endif
