/* phehildonwin_demo.c -- the libhildon demo, written against the PUBLIC libhildon 2.2 API only: a HildonProgram with a HildonWindow whose content is a HildonPannableArea (finger scrolling)
 * of HildonButtons (title + value), a HildonCheckButton, a HildonEntry and a HildonPickerButton with a HildonTouchSelector; an application menu (HildonAppMenu); a banner; a HildonNote.
 * Runs on phehildon.dll over GTK 2.24 and the Symbian GDK backend. BSD-2-Clause (Phorepoc). */
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <hildon/hildon.h>
#include <gdk/gdkkeysyms.h>

extern void phe_log (const char *msg);
#define LOG(...) do { char m_[240]; g_snprintf (m_, sizeof m_, __VA_ARGS__); phe_log (m_); } while (0)

static HildonProgram *program;
static GtkWidget *window, *status, *pannable;
static HildonAppMenu *app_menu;
static gint presses;
static int st_ok, st_fail;
#define CHECK(c, what) do { if (c) st_ok++; else { st_fail++; LOG ("[phehildonwin] selftest FAIL: %s (line %d)", what, __LINE__); } } while (0)

static void
update_status (void)
{
  gchar *t = g_strdup_printf ("presses: %d", presses);
  gtk_label_set_text (GTK_LABEL (status), t); g_free (t);
}

static void on_button (GtkButton *b, gpointer d) { (void) d; presses++; update_status (); LOG ("[phehildonwin] hildon button pressed: %s / %s", hildon_button_get_title (HILDON_BUTTON (b)), hildon_button_get_value (HILDON_BUTTON (b))); }
static void on_menu_item (GtkButton *b, gpointer d) { (void) d; LOG ("[phehildonwin] app menu item: %s", hildon_button_get_title (HILDON_BUTTON (b))); }
static void on_picked (HildonPickerButton *p, gpointer d) { (void) d; LOG ("[phehildonwin] picker value: %s", hildon_button_get_value (HILDON_BUTTON (p))); }
static void on_menu_open (GtkButton *b, gpointer d) { (void) b; (void) d; hildon_app_menu_popup (app_menu, GTK_WINDOW (window)); LOG ("[phehildonwin] app menu popped up"); }
static void on_banner (GtkButton *b, gpointer d) { (void) b; (void) d; hildon_banner_show_information (window, NULL, "A HildonBanner"); LOG ("[phehildonwin] banner shown"); }

static void
on_note (GtkButton *b, gpointer d)
{
  GtkWidget *note = hildon_note_new_information (GTK_WINDOW (window), "A HildonNote");
  (void) b; (void) d;
  g_signal_connect (note, "response", G_CALLBACK (gtk_widget_destroy), NULL);
  gtk_widget_show_all (note);
  LOG ("[phehildonwin] note shown");
}

static void
selftest (void)
{
  GtkWidget *b = hildon_button_new_with_text (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH, HILDON_BUTTON_ARRANGEMENT_VERTICAL, "Title", "Value");
  g_object_ref_sink (b);
  CHECK (HILDON_IS_BUTTON (b), "HildonButton type");
  CHECK (!strcmp (hildon_button_get_title (HILDON_BUTTON (b)), "Title") && !strcmp (hildon_button_get_value (HILDON_BUTTON (b)), "Value"), "HildonButton title and value");
  hildon_button_set_value (HILDON_BUTTON (b), "Other"); CHECK (!strcmp (hildon_button_get_value (HILDON_BUTTON (b)), "Other"), "HildonButton set_value");
  g_object_unref (b);
  { GtkWidget *c = hildon_check_button_new (HILDON_SIZE_AUTO); g_object_ref_sink (c); hildon_check_button_set_active (HILDON_CHECK_BUTTON (c), TRUE); CHECK (hildon_check_button_get_active (HILDON_CHECK_BUTTON (c)), "HildonCheckButton active"); g_object_unref (c); }
  { GtkWidget *s = hildon_touch_selector_new_text (); g_object_ref_sink (s); hildon_touch_selector_append_text (HILDON_TOUCH_SELECTOR (s), "one"); hildon_touch_selector_append_text (HILDON_TOUCH_SELECTOR (s), "two");
    hildon_touch_selector_set_active (HILDON_TOUCH_SELECTOR (s), 0, 1); { gchar *t = hildon_touch_selector_get_current_text (HILDON_TOUCH_SELECTOR (s)); CHECK (t && !strcmp (t, "two"), "HildonTouchSelector current text"); g_free (t); } g_object_unref (s); }
  { GtkWidget *e = hildon_entry_new (HILDON_SIZE_AUTO); g_object_ref_sink (e); gtk_entry_set_text (GTK_ENTRY (e), "x"); CHECK (HILDON_IS_ENTRY (e) && !strcmp (gtk_entry_get_text (GTK_ENTRY (e)), "x"), "HildonEntry"); g_object_unref (e); }
  { GtkWidget *p = hildon_pannable_area_new (); g_object_ref_sink (p); CHECK (HILDON_IS_PANNABLE_AREA (p), "HildonPannableArea"); g_object_unref (p); }
  CHECK (HILDON_MAJOR_VERSION == 2 && HILDON_MINOR_VERSION == 2, "libhildon version 2.2");
  LOG ("[phehildonwin] selftest done: %d passed, %d failed", st_ok, st_fail);
}

int
phe_hildon_demo_main (void)
{
  GtkWidget *vbox, *w, *picker, *selector, *b;
  gtk_init (NULL, NULL); hildon_init ();
  LOG ("[phehildonwin] gtk + hildon init ok: GTK %d.%d.%d, libhildon %d.%d.%d", gtk_major_version, gtk_minor_version, gtk_micro_version, HILDON_MAJOR_VERSION, HILDON_MINOR_VERSION, HILDON_MICRO_VERSION);
  selftest ();
  program = hildon_program_get_instance ();
  window = hildon_window_new (); hildon_program_add_window (program, HILDON_WINDOW (window));
  gtk_window_set_title (GTK_WINDOW (window), "phehildonwin");
  gtk_window_set_default_size (GTK_WINDOW (window), gdk_screen_get_width (gdk_screen_get_default ()), gdk_screen_get_height (gdk_screen_get_default ()));
  /* the application menu */
  app_menu = HILDON_APP_MENU (hildon_app_menu_new ());
  b = hildon_gtk_button_new (HILDON_SIZE_AUTO); gtk_button_set_label (GTK_BUTTON (b), "Settings"); g_signal_connect (b, "clicked", G_CALLBACK (on_menu_item), NULL); hildon_app_menu_append (app_menu, GTK_BUTTON (b));
  b = hildon_gtk_button_new (HILDON_SIZE_AUTO); gtk_button_set_label (GTK_BUTTON (b), "About"); g_signal_connect (b, "clicked", G_CALLBACK (on_menu_item), NULL); hildon_app_menu_append (app_menu, GTK_BUTTON (b));
  gtk_widget_show_all (GTK_WIDGET (app_menu));
  hildon_window_set_app_menu (HILDON_WINDOW (window), app_menu);
  /* the content: a pannable area with the touch widgets */
  pannable = hildon_pannable_area_new ();
  vbox = gtk_vbox_new (FALSE, 6);
  w = gtk_label_new (NULL); gtk_label_set_markup (GTK_LABEL (w), "<span size='x-large' weight='bold'>Hildon</span>\nlibhildon 2.2 on GTK 2.24"); gtk_label_set_justify (GTK_LABEL (w), GTK_JUSTIFY_CENTER); gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 8);
  b = hildon_button_new_with_text (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH, HILDON_BUTTON_ARRANGEMENT_VERTICAL, "A HildonButton", "title above, value below");
  g_signal_connect (b, "clicked", G_CALLBACK (on_button), NULL); gtk_box_pack_start (GTK_BOX (vbox), b, FALSE, FALSE, 0);
  picker = hildon_picker_button_new (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH, HILDON_BUTTON_ARRANGEMENT_VERTICAL); hildon_button_set_title (HILDON_BUTTON (picker), "A HildonPickerButton");
  selector = hildon_touch_selector_new_text (); hildon_touch_selector_append_text (HILDON_TOUCH_SELECTOR (selector), "Symbian"); hildon_touch_selector_append_text (HILDON_TOUCH_SELECTOR (selector), "Maemo");
  hildon_touch_selector_append_text (HILDON_TOUCH_SELECTOR (selector), "Hildon"); hildon_picker_button_set_selector (HILDON_PICKER_BUTTON (picker), HILDON_TOUCH_SELECTOR (selector));
  hildon_picker_button_set_active (HILDON_PICKER_BUTTON (picker), 2); g_signal_connect (picker, "value-changed", G_CALLBACK (on_picked), NULL); gtk_box_pack_start (GTK_BOX (vbox), picker, FALSE, FALSE, 0);
  w = hildon_entry_new (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH); hildon_entry_set_placeholder (HILDON_ENTRY (w), "A HildonEntry"); gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);
  w = hildon_check_button_new (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH); gtk_button_set_label (GTK_BUTTON (w), "A HildonCheckButton"); gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);
  b = hildon_gtk_button_new (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH); gtk_button_set_label (GTK_BUTTON (b), "App menu"); g_signal_connect (b, "clicked", G_CALLBACK (on_menu_open), NULL); gtk_box_pack_start (GTK_BOX (vbox), b, FALSE, FALSE, 0);
  b = hildon_gtk_button_new (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH); gtk_button_set_label (GTK_BUTTON (b), "Banner"); g_signal_connect (b, "clicked", G_CALLBACK (on_banner), NULL); gtk_box_pack_start (GTK_BOX (vbox), b, FALSE, FALSE, 0);
  b = hildon_gtk_button_new (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH); gtk_button_set_label (GTK_BUTTON (b), "Note"); g_signal_connect (b, "clicked", G_CALLBACK (on_note), NULL); gtk_box_pack_start (GTK_BOX (vbox), b, FALSE, FALSE, 0);
  status = gtk_label_new (NULL); gtk_box_pack_start (GTK_BOX (vbox), status, FALSE, FALSE, 8); update_status ();
  hildon_pannable_area_add_with_viewport (HILDON_PANNABLE_AREA (pannable), vbox);
  gtk_container_add (GTK_CONTAINER (window), pannable);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_widget_show_all (window);
  LOG ("[phehildonwin] window shown (mapped %d)", gtk_widget_get_mapped (window));
  LOG ("[phehildonwin] loop running");
  gtk_main ();
  return 0;
}
