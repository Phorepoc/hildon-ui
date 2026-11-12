/* Copyright The Phorepoc Authors */
/* SPDX-License-Identifier: BSD-2-Clause */
/* phehcp_osso.c -- OURS: the libosso calls of hildon-control-panel (shims/libosso.h), and the start of the control panel process (phe_hcp_start). */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <hildon/hildon.h>
#include "libosso.h"
#include "phe_respath.h"
#include "phe_theme.h"

extern void phe_log (const char *msg);
extern int phe_hcp_main (int argc, char **argv);   /* hildon-control-panel's main (hcp-main.c, its main renamed) */

const char *phe_hcp_res (const char *rel) { return phe_res (rel); }

/* hcp-main.c calls hildon_init () right after gtk_init (): the theme is loaded there (it is built with -Dhildon_init=phe_hcp_hildon_init) */
/* On Maemo the window manager maximises a stackable window; here nothing does, and the control panel (unlike the demo, which sets its size) stayed 584 px high with an unpainted strip
 * below it. hcp-program.c's gtk_widget_show_all (the build renames it, shims/phe_hcp_show.h) comes here: a stackable window gets the screen size before it is shown. */
#undef gtk_widget_show_all
void
phe_hcp_show_all (GtkWidget *w)
{
  if (HILDON_IS_STACKABLE_WINDOW (w)) {
    gtk_window_set_default_size (GTK_WINDOW (w), gdk_screen_get_width (gdk_screen_get_default ()), gdk_screen_get_height (gdk_screen_get_default ()));
    phe_log ("[phehcp] stackable window: default size = the screen");
  }
  gtk_widget_show_all (w);
}

/* hcp-app-view.c's grid item width (shims/phe_hcp_itemwidth.h): the N900 portrait width (432 px) at most the screen width less the
   window's margins, so the grid never overflows the pannable area sideways (a tap scrolled it, the icons went off the left edge) */
void
phe_hcp_set_item_width (GtkIconView *view, gint width)
{
  gint max = gdk_screen_get_width (gdk_screen_get_default ()) - 3 * HILDON_MARGIN_DOUBLE;
  if (max > 0 && width > max) width = max;
  gtk_icon_view_set_item_width (view, width);
}

void
phe_hcp_hildon_init (void)
{
  hildon_init ();
  if (!phe_theme_load (phe_log)) gtk_rc_parse (phe_res ("resource/phehcp/phehildon.rc"));
  { /* the applets' icons (general_clock, ...): our own icon theme in resource/phehcp/icons/hicolor, on whichever drive the package went to */
    gchar *idx = g_strdup (phe_res ("resource/phehcp/icons/hicolor/index.theme")), *dir = g_path_get_dirname (idx), *top = g_path_get_dirname (dir);
    gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (), top);
    g_free (idx); g_free (dir); g_free (top); }
}

struct osso_af_context_t {
  gchar *app, *version;
  osso_rpc_cb_f *rpc_cb; gpointer rpc_data;
  osso_hw_cb_f *hw_cb; gpointer hw_data;
};

#define STATE_DIR "c:/private/e7016c00"

osso_context_t *
osso_initialize (const gchar *application, const gchar *version, gboolean activation, GMainContext *context)
{
  osso_context_t *o = g_new0 (osso_context_t, 1);
  (void) activation; (void) context;
  o->app = g_strdup (application); o->version = g_strdup (version);
  return o;
}

void
osso_deinitialize (osso_context_t *o)
{
  if (!o) return;
  g_free (o->app); g_free (o->version); g_free (o);
}

osso_return_t
osso_rpc_set_default_cb_f (osso_context_t *o, osso_rpc_cb_f *cb, gpointer data)
{
  if (!o) return OSSO_INVALID;
  o->rpc_cb = cb; o->rpc_data = data;
  return OSSO_OK;
}

osso_return_t
osso_hw_set_event_cb (osso_context_t *o, osso_hw_state_t *state, osso_hw_cb_f *cb, gpointer data)
{
  (void) state;
  if (!o) return OSSO_INVALID;
  o->hw_cb = cb; o->hw_data = data;
  return OSSO_OK;
}

static gchar *
state_path (osso_context_t *o)
{
  return g_strdup_printf (STATE_DIR "/%s.state", o && o->app ? o->app : "app");
}

osso_return_t
osso_state_read (osso_context_t *o, osso_state_t *state)
{
  gchar *p, *data = NULL; gsize len = 0;
  if (!o || !state) return OSSO_INVALID;
  p = state_path (o);
  if (!g_file_get_contents (p, &data, &len, NULL)) { g_free (p); return OSSO_ERROR_NO_STATE; }
  g_free (p);
  if (state->state_size && state->state_data) {          /* the caller gave a buffer: it must fit */
    if (len != state->state_size) { g_free (data); return OSSO_ERROR_STATE_SIZE; }
    memcpy (state->state_data, data, len); g_free (data);
  } else {
    state->state_data = data; state->state_size = len;
  }
  return OSSO_OK;
}

osso_return_t
osso_state_write (osso_context_t *o, osso_state_t *state)
{
  gchar *p; gboolean ok;
  if (!o || !state) return OSSO_INVALID;
  mkdir (STATE_DIR, 0755);
  p = state_path (o);
  ok = g_file_set_contents (p, state->state_data, state->state_size, NULL);
  g_free (p);
  return ok ? OSSO_OK : OSSO_ERROR;
}

/* the categories hildon-control-panel reads from GConf (the defaults of its schema; the names are shown as they are: no translation catalogue yet) */
static void
set_default_categories (void)
{
  GConfClient *c = gconf_client_get_default ();
  GSList *names = NULL, *ids = NULL;
  if (!gconf_client_get_list (c, "/apps/osso/apps/controlpanel/group_ids", GCONF_VALUE_STRING, NULL)) {
    names = g_slist_append (names, "Personalisation"); names = g_slist_append (names, "Connectivity"); names = g_slist_append (names, "General");
    ids = g_slist_append (ids, "personalisation"); ids = g_slist_append (ids, "connectivity"); ids = g_slist_append (ids, "general");
    gconf_client_set_list (c, "/apps/osso/apps/controlpanel/groups", GCONF_VALUE_STRING, names, NULL);
    gconf_client_set_list (c, "/apps/osso/apps/controlpanel/group_ids", GCONF_VALUE_STRING, ids, NULL);
    g_slist_free (names); g_slist_free (ids);
  }
  g_object_unref (c);
}

int
phe_hcp_start (void)
{
  static char arg0[] = "controlpanel";
  char *argv[] = { arg0, NULL };
  set_default_categories ();
  phe_log ("[phehcp] hildon-control-panel starting");
  return phe_hcp_main (1, argv);
}
