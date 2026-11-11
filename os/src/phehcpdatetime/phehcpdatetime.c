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

/* The time button's label follows the 24-hour choice: the selector's format policy is set from the check button, and the current time is put back so the label is rebuilt. */
static void
apply_24h (HildonTimeButton *time, gboolean on)
{
  HildonTouchSelector *sel = hildon_picker_button_get_selector (HILDON_PICKER_BUTTON (time));
  guint h, m;
  hildon_time_button_get_time (time, &h, &m);
  g_object_set (sel, "time_format_policy", on ? HILDON_TIME_SELECTOR_FORMAT_POLICY_24H : HILDON_TIME_SELECTOR_FORMAT_POLICY_AMPM, NULL);
  hildon_time_button_set_time (time, h, m);
  { /* the button's label is only rebuilt by libhildon when the selector's text changes, which a policy change does not do: write it the way the two libhildon formats do (%H:%M, %l:%M am/pm) */
    gchar *t = on ? g_strdup_printf ("%02u:%02u", h, m) : g_strdup_printf ("%u:%02u %s", h % 12 ? h % 12 : 12, m, h < 12 ? "am" : "pm");
    hildon_button_set_value (HILDON_BUTTON (time), t); g_free (t); }
}

static void
h24_toggled (GtkWidget *b, gpointer time)
{
  apply_24h (HILDON_TIME_BUTTON (time), hildon_check_button_get_active (HILDON_CHECK_BUTTON (b)));
}

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
  apply_24h (HILDON_TIME_BUTTON (time), phe_dt_get_24h ());
  hildon_time_button_set_time (HILDON_TIME_BUTTON (time), h, mi);
  h24 = hildon_check_button_new (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH);
  gtk_button_set_label (GTK_BUTTON (h24), "24-hour clock");
  hildon_check_button_set_active (HILDON_CHECK_BUTTON (h24), phe_dt_get_24h ());
  g_signal_connect (h24, "toggled", G_CALLBACK (h24_toggled), time);
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
