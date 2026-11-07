/* shim: the part of GConf libhildon uses (typed get/set of keys by path), implemented in phehildon_gconf.c as an in-process key store: no GConf daemon exists here,
 * so the "configuration database" is a GKeyFile that lives for the life of the process (and is loaded from / saved to $PHE_GCONF_FILE when that names a file). BSD-2-Clause (Phorepoc). */
#ifndef PHE_GCONF_CLIENT_H
#define PHE_GCONF_CLIENT_H
#include <glib-object.h>
G_BEGIN_DECLS
typedef struct _GConfClient GConfClient;
GConfClient *gconf_client_get_default (void);
gboolean     gconf_client_get_bool   (GConfClient *client, const gchar *key, GError **err);
gint         gconf_client_get_int    (GConfClient *client, const gchar *key, GError **err);
gchar       *gconf_client_get_string (GConfClient *client, const gchar *key, GError **err);
gboolean     gconf_client_set_bool   (GConfClient *client, const gchar *key, gboolean val, GError **err);
gboolean     gconf_client_set_int    (GConfClient *client, const gchar *key, gint val, GError **err);
gboolean     gconf_client_set_string (GConfClient *client, const gchar *key, const gchar *val, GError **err);
G_END_DECLS
#endif
