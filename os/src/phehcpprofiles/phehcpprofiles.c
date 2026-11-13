/* Copyright The Phorepoc Authors */
/* SPDX-License-Identifier: BSD-2-Clause */
/* phehcpprofiles.c -- OURS, a minimal STAND-IN: the "Profiles" applet of the Hildon control panel, on the real plug-in interface
 * (hildon-cp-plugin-interface.h). It switches the active Symbian profile (General, Silent, Meeting, ...) through the Profiles engine
 * (phehcpprofiles_sym.cpp), as the Symbian Profiles application does; the status bar and the ringing / warning tones follow it.
 * Fremantle kept General / Silent in its "Profiles" applet; the per-profile tones stay in the Symbian Profiles application. */
#include <gtk/gtk.h>
#include <hildon/hildon.h>
#include "hildon-cp-plugin-interface.h"

#define MAXP 16
/* phehcpprofiles_sym.cpp */
extern int phe_pf_list (int *ids, char (*names)[48], int max, int *count, int *active);
extern int phe_pf_set_active (int id);

osso_return_t
execute (osso_context_t *osso, gpointer data, gboolean user_activated)
{
  GtkWidget *dialog, *picker, *selector;
  static char names[MAXP][48];
  int ids[MAXP], n = 0, active = -1, act = 0, i, err;
  (void) osso; (void) user_activated;
  err = phe_pf_list (ids, names, MAXP, &n, &active);
  g_message ("[phehcpprofiles] %d profiles, active %d: %d", n, active, err);
  dialog = gtk_dialog_new_with_buttons ("Profiles", GTK_WINDOW (data), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
                                        "Save", GTK_RESPONSE_OK, NULL);
  selector = hildon_touch_selector_new_text ();
  for (i = 0; i < n; i++) {
    hildon_touch_selector_append_text (HILDON_TOUCH_SELECTOR (selector), names[i]);
    if (ids[i] == active) act = i;
  }
  picker = hildon_picker_button_new (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH, HILDON_BUTTON_ARRANGEMENT_VERTICAL);
  hildon_button_set_title (HILDON_BUTTON (picker), "Active profile");
  hildon_picker_button_set_selector (HILDON_PICKER_BUTTON (picker), HILDON_TOUCH_SELECTOR (selector));
  if (n > 0) hildon_picker_button_set_active (HILDON_PICKER_BUTTON (picker), act);
  else hildon_button_set_value (HILDON_BUTTON (picker), "Profiles are not available");
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), picker, FALSE, FALSE, 0);
  gtk_widget_show_all (dialog);
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
    i = hildon_picker_button_get_active (HILDON_PICKER_BUTTON (picker));
    if (i >= 0 && i < n && ids[i] != active) {
      err = phe_pf_set_active (ids[i]);
      g_message ("[phehcpprofiles] profile %d -> %d (%s): %d", active, ids[i], names[i], err);
    }
  }
  gtk_widget_destroy (dialog);
  return OSSO_OK;
}

osso_return_t
save_state (osso_context_t *osso, gpointer data)
{
  (void) osso; (void) data;
  return OSSO_OK;
}
