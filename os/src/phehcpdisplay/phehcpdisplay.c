/* Copyright The Phorepoc Authors */
/* SPDX-License-Identifier: BSD-2-Clause */
/* phehcpdisplay.c -- OURS, a minimal STAND-IN: the "Display" applet of the Hildon control panel, on the real plug-in interface (hildon-cp-plugin-interface.h).
 * It sets how long the screen light stays on, the central repository key the Symbian display settings change (phehcpdisplay_sym.cpp).
 * Replaced by Leste's osso-applet-display once an MCE bridge exists (docs hildon-port.md 7.104). */
#include <gtk/gtk.h>
#include <hildon/hildon.h>
#include "hildon-cp-plugin-interface.h"

/* phehcpdisplay_sym.cpp: seconds, -1 = not readable; set returns a Symbian error code */
extern int phe_dp_get_timeout (void);
extern int phe_dp_set_timeout (int seconds);

static const int secs[] = { 15, 30, 60, 120, 300, 600 };
static const char *names[] = { "15 seconds", "30 seconds", "1 minute", "2 minutes", "5 minutes", "10 minutes" };
#define N (int) (sizeof secs / sizeof secs[0])

osso_return_t
execute (osso_context_t *osso, gpointer data, gboolean user_activated)
{
  GtkWidget *dialog, *picker, *selector;
  int i, cur = phe_dp_get_timeout (), act = 0;
  (void) osso; (void) user_activated;
  dialog = gtk_dialog_new_with_buttons ("Display", GTK_WINDOW (data), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
                                        "Save", GTK_RESPONSE_OK, NULL);
  selector = hildon_touch_selector_new_text ();
  for (i = 0; i < N; i++) {
    hildon_touch_selector_append_text (HILDON_TOUCH_SELECTOR (selector), names[i]);
    if (cur >= secs[i]) act = i;                 /* the nearest choice at or below the stored value */
  }
  picker = hildon_picker_button_new (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH, HILDON_BUTTON_ARRANGEMENT_VERTICAL);
  hildon_button_set_title (HILDON_BUTTON (picker), "Turn the light off after");
  hildon_picker_button_set_selector (HILDON_PICKER_BUTTON (picker), HILDON_TOUCH_SELECTOR (selector));
  hildon_picker_button_set_active (HILDON_PICKER_BUTTON (picker), act);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), picker, FALSE, FALSE, 0);
  gtk_widget_show_all (dialog);
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
    i = hildon_picker_button_get_active (HILDON_PICKER_BUTTON (picker));
    if (i >= 0 && secs[i] != cur) {
      int err = phe_dp_set_timeout (secs[i]);
      g_message ("[phehcpdisplay] light timeout %d -> %d s: %d", cur, secs[i], err);
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
