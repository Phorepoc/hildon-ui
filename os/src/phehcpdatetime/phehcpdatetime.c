/* Copyright The Phorepoc Authors */
/* SPDX-License-Identifier: BSD-2-Clause */
/* phehcpdatetime.c -- OURS, a minimal STAND-IN: the "Date and time" applet of the Hildon control panel, on the real plug-in interface (hildon-cp-plugin-interface.h).
 * It sets the system's date, time and 12/24-hour clock through the Symbian calls the Symbian clock settings use (phehcpdatetime_sym.cpp: User::SetHomeTime, TLocale).
 * Replaced by Leste's applet-datetime (GPL-3) once a clockd bridge exists (docs hildon-port.md 7.104). */
#include <gtk/gtk.h>
#include <hildon/hildon.h>
#include "hildon-cp-plugin-interface.h"

/* phehcpdatetime_sym.cpp */
extern void phe_dt_get (int *year, int *month, int *day, int *hour, int *minute);   /* month 0-11, day 1-31 */
extern int phe_dt_set (int year, int month, int day, int hour, int minute);
extern int phe_dt_get_24h (void);
extern void phe_dt_set_24h (int on);

osso_return_t
execute (osso_context_t *osso, gpointer data, gboolean user_activated)
{
  GtkWidget *dialog, *box, *date, *time, *h24;
  int y, mo, d, h, mi, r;
  guint uy, umo, ud, uh, umi;
  (void) osso; (void) user_activated;
  phe_dt_get (&y, &mo, &d, &h, &mi);
  dialog = gtk_dialog_new_with_buttons ("Date and time", GTK_WINDOW (data), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
                                        "Save", GTK_RESPONSE_OK, NULL);
  box = GTK_DIALOG (dialog)->vbox;
  date = hildon_date_button_new (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH, HILDON_BUTTON_ARRANGEMENT_VERTICAL);
  hildon_date_button_set_date (HILDON_DATE_BUTTON (date), y, mo, d);
  time = hildon_time_button_new (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH, HILDON_BUTTON_ARRANGEMENT_VERTICAL);
  hildon_time_button_set_time (HILDON_TIME_BUTTON (time), h, mi);
  h24 = hildon_check_button_new (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH);
  gtk_button_set_label (GTK_BUTTON (h24), "24-hour clock");
  hildon_check_button_set_active (HILDON_CHECK_BUTTON (h24), phe_dt_get_24h ());
  gtk_box_pack_start (GTK_BOX (box), date, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), time, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), h24, FALSE, FALSE, 0);
  gtk_widget_show_all (dialog);
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
    hildon_date_button_get_date (HILDON_DATE_BUTTON (date), &uy, &umo, &ud);
    hildon_time_button_get_time (HILDON_TIME_BUTTON (time), &uh, &umi);
    phe_dt_set_24h (hildon_check_button_get_active (HILDON_CHECK_BUTTON (h24)));
    if ((int) uy != y || (int) umo != mo || (int) ud != d || (int) uh != h || (int) umi != mi) {
      r = phe_dt_set (uy, umo, ud, uh, umi);
      if (r) g_warning ("[phehcpdatetime] setting the time failed: %d", r);
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
