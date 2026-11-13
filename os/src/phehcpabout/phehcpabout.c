/* Copyright The Phorepoc Authors */
/* SPDX-License-Identifier: BSD-2-Clause */
/* phehcpabout.c -- OURS, a minimal STAND-IN: the "About device" applet of the Hildon control panel (Fremantle's "About product"), on the real plug-in
 * interface (hildon-cp-plugin-interface.h). Read only: the software version, the UI language, memory and storage, from the Symbian sources
 * (phehcpabout_sym.cpp). */
#include <gtk/gtk.h>
#include <hildon/hildon.h>
#include "hildon-cp-plugin-interface.h"

/* phehcpabout_sym.cpp */
extern int phe_ab_sw_version (char *buf, int len);
extern int phe_ab_language (void);
extern void phe_ab_ram (int *total_kb, int *free_kb);
extern int phe_ab_drive (int drive, int *total_kb, int *free_kb);

/* TLanguage (e32lang.h) -> a name, the common ones */
static const char *
lang_name (int l)
{
  static const struct { int id; const char *name; } t[] = {
    { 1, "English (UK)" }, { 2, "French" }, { 3, "German" }, { 4, "Spanish" }, { 5, "Italian" }, { 6, "Swedish" }, { 7, "Danish" },
    { 8, "Norwegian" }, { 9, "Finnish" }, { 10, "English (US)" }, { 13, "Portuguese" }, { 14, "Turkish" }, { 16, "Russian" },
    { 17, "Hungarian" }, { 18, "Dutch" }, { 27, "Polish" }, { 54, "Greek" }, { 0, NULL } };
  int i;
  for (i = 0; t[i].name; i++) if (t[i].id == l) return t[i].name;
  return NULL;
}

static void
row (GtkWidget *box, const char *title, const char *value)
{
  GtkWidget *b = hildon_button_new_with_text (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH, HILDON_BUTTON_ARRANGEMENT_VERTICAL, title, value);
  gtk_button_set_alignment (GTK_BUTTON (b), 0.0, 0.5);
  gtk_widget_set_sensitive (b, TRUE);
  gtk_box_pack_start (GTK_BOX (box), b, FALSE, FALSE, 0);
}

static gchar *
mb (int free_kb, int total_kb)
{
  if (total_kb < 0) return g_strdup ("not known");
  return g_strdup_printf ("%d MB free of %d MB", free_kb / 1024, total_kb / 1024);
}

osso_return_t
execute (osso_context_t *osso, gpointer data, gboolean user_activated)
{
  GtkWidget *dialog, *box;
  char sw[128];
  const char *ln;
  gchar *s;
  int t = -1, f = -1, l;
  (void) osso; (void) user_activated;
  dialog = gtk_dialog_new_with_buttons ("About device", GTK_WINDOW (data), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
                                        "Close", GTK_RESPONSE_OK, NULL);
  box = GTK_DIALOG (dialog)->vbox;
  /* sw.txt is one line of fields separated by a literal "\n": "V <version>\n<dd-mm-yy>\n<model>\n(c)<maker>" (the Symbian convention) */
  if (phe_ab_sw_version (sw, sizeof sw) == 0 && sw[0]) {
    gchar **f = g_strsplit (sw, "\\n", 4);
    const char *ver = f[0] && g_str_has_prefix (f[0], "V ") ? f[0] + 2 : f[0];
    row (box, "Device", f[0] && f[1] && f[2] ? f[2] : "Phorepoc");
    s = g_strdup (ver);                    /* the date field repeats the version's own date */
    row (box, "Software version", s); g_free (s);
    g_strfreev (f);
  } else {
    row (box, "Device", "Phorepoc");
    row (box, "Software version", "not known");
  }
  l = phe_ab_language ();
  ln = lang_name (l);
  s = ln ? g_strdup (ln) : g_strdup_printf ("Language %d", l);
  row (box, "Language", s); g_free (s);
  phe_ab_ram (&t, &f);
  s = mb (f, t); row (box, "Memory", s); g_free (s);
  t = f = -1;
  if (phe_ab_drive (2, &t, &f) != 0) t = -1;
  s = mb (f, t); row (box, "Storage (C:)", s); g_free (s);
  g_message ("[phehcpabout] sw \"%s\", language %d", sw, l);
  gtk_widget_show_all (dialog);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
  return OSSO_OK;
}

osso_return_t
save_state (osso_context_t *osso, gpointer data)
{
  (void) osso; (void) data;
  return OSSO_OK;
}
