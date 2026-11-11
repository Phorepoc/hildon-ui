/* Copyright The Phorepoc Authors */
/* SPDX-License-Identifier: BSD-2-Clause */
/* libosso.h -- OURS: the part of libosso (Maemo's application framework library) that hildon-control-panel and its applets use, implemented in phehcp_osso.c.
 * A context is a small struct; the state is one file in the program's private directory; the RPC default handler and the hardware-state callback are recorded but
 * not driven yet (no session-bus name is taken: the control panel is started from the application menu, not over D-Bus). Types and constants follow
 * the libosso API so the real sources compile unmodified. */
#ifndef PHE_LIBOSSO_H
#define PHE_LIBOSSO_H
#include <glib.h>
#include <dbus/dbus.h>

G_BEGIN_DECLS

typedef enum { OSSO_OK = 0, OSSO_ERROR = -1, OSSO_INVALID = -2, OSSO_RPC_ERROR = -3, OSSO_ERROR_NAME = -4, OSSO_ERROR_NO_STATE = -5, OSSO_ERROR_STATE_SIZE = -6 } osso_return_t;

typedef struct osso_af_context_t osso_context_t;

typedef struct {
  int type;
  union { guint32 u; gint32 i; gboolean b; gdouble d; gchar *s; } value;
} osso_rpc_t;

typedef gint (osso_rpc_cb_f) (const gchar *interface, const gchar *method, GArray *arguments, gpointer data, osso_rpc_t *retval);

typedef struct {
  gboolean shutdown_ind;
  gboolean save_unsaved_data_ind;
  gboolean memory_low_ind;
  gboolean system_inactivity_ind;
  gint sig_device_mode_ind;
} osso_hw_state_t;

typedef void (osso_hw_cb_f) (osso_hw_state_t *state, gpointer data);

typedef struct {
  guint32 state_size;
  gpointer state_data;
} osso_state_t;

osso_context_t *osso_initialize (const gchar *application, const gchar *version, gboolean activation, GMainContext *context);
void osso_deinitialize (osso_context_t *osso);
osso_return_t osso_rpc_set_default_cb_f (osso_context_t *osso, osso_rpc_cb_f *cb, gpointer data);
osso_return_t osso_hw_set_event_cb (osso_context_t *osso, osso_hw_state_t *state, osso_hw_cb_f *cb, gpointer data);
osso_return_t osso_state_read (osso_context_t *osso, osso_state_t *state);
osso_return_t osso_state_write (osso_context_t *osso, osso_state_t *state);

G_END_DECLS
#endif
