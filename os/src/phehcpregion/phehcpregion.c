/* Copyright The Phorepoc Authors */
/* SPDX-License-Identifier: BSD-2-Clause */
/* phehcpregion.c -- OURS, a minimal STAND-IN: the "Regional settings" applet of the Hildon control panel, on the real plug-in interface (hildon-cp-plugin-interface.h).
 * It sets the system's date format through TLocale (phehcpregion_sym.cpp), the setting the Symbian regional settings change.
 * Replaced by Leste's osso-applet-languageregional once its dependencies are ported (docs hildon-port.md 7.104). */
#include <gtk/gtk.h>
#include <hildon/hildon.h>
#include "hildon-cp-plugin-interface.h"

/* phehcpregion_sym.cpp: 0 = day/month/year, 1 = month/day/year, 2 = year/month/day */
extern int phe_rg_get_date_format (void);
extern void phe_rg_set_date_format (int format);

osso_return_t
execute (osso_context_t *osso, gpointer data, gboolean user_activated)
{
  static const char *formats[] = { "Day / month / year", "Month / day / year", "Year / month / day" };
  GtkWidget *dialog, *picker, *selector;
  int i, cur;
  (void) osso; (void) user_activated;
  dialog = gtk_dialog_new_with_buttons ("Regional settings", GTK_WINDOW (data), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
                                        "Save", GTK_RESPONSE_OK, NULL);
  selector = hildon_touch_selector_new_text ();
  for (i = 0; i < 3; i++) hildon_touch_selector_append_text (HILDON_TOUCH_SELECTOR (selector), formats[i]);
  picker = hildon_picker_button_new (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH, HILDON_BUTTON_ARRANGEMENT_VERTICAL);
  hildon_button_set_title (HILDON_BUTTON (picker), "Date format");
  hildon_picker_button_set_selector (HILDON_PICKER_BUTTON (picker), HILDON_TOUCH_SELECTOR (selector));
  cur = phe_rg_get_date_format ();
  hildon_picker_button_set_active (HILDON_PICKER_BUTTON (picker), cur);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), picker, FALSE, FALSE, 0);
  gtk_widget_show_all (dialog);
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
    i = hildon_picker_button_get_active (HILDON_PICKER_BUTTON (picker));
    if (i >= 0 && i != cur) phe_rg_set_date_format (i);
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
