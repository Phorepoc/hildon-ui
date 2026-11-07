/* shim: libcanberra's client API with the platform's audio as its one backend: a sample file (media.filename) is played through the MMF (phehildon_mmf.cpp, CMdaAudioPlayerUtility), the finish
 * callback is called when the sample has been played. Sound-theme lookup by event id (event.id) is not implemented: a play request without a file name fails with CA_ERROR_NOTSUPPORTED.
 * Implemented in phehildon_canberra.c. BSD-2-Clause (Phorepoc). */
#ifndef PHE_CANBERRA_H
#define PHE_CANBERRA_H
#include <glib.h>
G_BEGIN_DECLS
typedef struct ca_context ca_context;
typedef struct ca_proplist ca_proplist;
typedef void (*ca_finish_callback_t) (ca_context *c, guint32 id, int error_code, void *userdata);
enum { CA_SUCCESS = 0, CA_ERROR_NOTSUPPORTED = -1, CA_ERROR_INVALID = -2, CA_ERROR_STATE = -3, CA_ERROR_OOM = -4, CA_ERROR_NODRIVER = -5 };
#define CA_PROP_APPLICATION_NAME "application.name"
#define CA_PROP_MEDIA_FILENAME "media.filename"
#define CA_PROP_MEDIA_ROLE "media.role"
int ca_context_create (ca_context **c);
int ca_context_destroy (ca_context *c);
int ca_context_open (ca_context *c);
int ca_context_change_props (ca_context *c, ...);
int ca_context_play_full (ca_context *c, guint32 id, ca_proplist *p, ca_finish_callback_t cb, void *userdata);
int ca_proplist_create (ca_proplist **p);
int ca_proplist_destroy (ca_proplist *p);
int ca_proplist_sets (ca_proplist *p, const char *key, const char *value);
const char *ca_strerror (int code);
G_END_DECLS
#endif
