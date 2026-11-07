/* phehildon_canberra.c -- see shims/canberra.h: the contexts and property lists are real, playing is the MMF through phehildon_mmf.cpp. BSD-2-Clause (Phorepoc). */
#include <stdarg.h>
#include "canberra.h"

struct ca_context { GHashTable *props; };
struct ca_proplist { GHashTable *props; };

int ca_context_create (ca_context **c) { *c = g_new0 (ca_context, 1); (*c)->props = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free); return CA_SUCCESS; }
int ca_context_destroy (ca_context *c) { if (!c) return CA_ERROR_INVALID; g_hash_table_destroy (c->props); g_free (c); return CA_SUCCESS; }
int ca_context_open (ca_context *c) { return c ? CA_SUCCESS : CA_ERROR_INVALID; }               /* the MMF is opened per playback */

int
ca_context_change_props (ca_context *c, ...)
{
  va_list ap; const char *k;
  if (!c) return CA_ERROR_INVALID;
  va_start (ap, c);
  while ((k = va_arg (ap, const char *)) != NULL) { const char *v = va_arg (ap, const char *); if (!v) break; g_hash_table_replace (c->props, g_strdup (k), g_strdup (v)); }
  va_end (ap);
  return CA_SUCCESS;
}

typedef void (*PheAudioDone) (int error, void *user);
int phe_audio_play_file (const char *path, PheAudioDone done, void *user);

typedef struct { ca_context *c; guint32 id; ca_finish_callback_t cb; void *userdata; } PlayRequest;

static void
play_done (int error, void *user)
{
  PlayRequest *r = user;
  if (r->cb) r->cb (r->c, r->id, error == 0 ? CA_SUCCESS : CA_ERROR_NOTSUPPORTED, r->userdata);
  g_free (r);
}

int
ca_context_play_full (ca_context *c, guint32 id, ca_proplist *p, ca_finish_callback_t cb, void *userdata)
{
  const char *file; PlayRequest *r;
  if (!c || !p) return CA_ERROR_INVALID;
  file = g_hash_table_lookup (p->props, CA_PROP_MEDIA_FILENAME);
  if (!file) return CA_ERROR_NOTSUPPORTED;                                    /* no sound theme: only sample files */
  r = g_new0 (PlayRequest, 1); r->c = c; r->id = id; r->cb = cb; r->userdata = userdata;
  if (phe_audio_play_file (file, play_done, r) != 0) { g_free (r); return CA_ERROR_OOM; }
  return CA_SUCCESS;
}
int ca_proplist_create (ca_proplist **p) { *p = g_new0 (ca_proplist, 1); (*p)->props = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free); return CA_SUCCESS; }
int ca_proplist_destroy (ca_proplist *p) { if (!p) return CA_ERROR_INVALID; g_hash_table_destroy (p->props); g_free (p); return CA_SUCCESS; }
int ca_proplist_sets (ca_proplist *p, const char *key, const char *value) { if (!p || !key || !value) return CA_ERROR_INVALID; g_hash_table_replace (p->props, g_strdup (key), g_strdup (value)); return CA_SUCCESS; }

const char *
ca_strerror (int code)
{
  switch (code) { case CA_SUCCESS: return "Success"; case CA_ERROR_NOTSUPPORTED: return "Operation not supported"; case CA_ERROR_INVALID: return "Invalid argument";
    case CA_ERROR_STATE: return "Invalid state"; case CA_ERROR_OOM: return "Out of memory"; case CA_ERROR_NODRIVER: return "No such driver"; default: return "Unknown error"; }
}
