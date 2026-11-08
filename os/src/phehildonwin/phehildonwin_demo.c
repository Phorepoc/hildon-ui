/* phehildonwin_demo.c -- the libhildon demo, written against the PUBLIC libhildon 2.2 API only: a HildonProgram with a HildonWindow whose content is a HildonPannableArea (finger scrolling)
 * of HildonButtons (title + value), a HildonCheckButton, a HildonEntry and a HildonPickerButton with a HildonTouchSelector; an application menu (HildonAppMenu); a banner; a HildonNote.
 * Runs on phehildon.dll over GTK 2.24 and the Symbian GDK backend. BSD-2-Clause (Phorepoc). */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "phe_respath.h"
#include "phe_theme.h"
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
static void on_sound (GtkButton *b, gpointer d) { (void) b; (void) d; hildon_play_system_sound ("z:/resource/phehildonwin/beep.wav"); LOG ("[phehildonwin] sound requested"); }
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

static gboolean
report_topmost (gpointer d)
{
  (void) d;
  LOG ("[phehildonwin] HildonProgram is-topmost: %d, window is-topmost: %d", hildon_program_get_is_topmost (program), hildon_window_get_is_topmost (HILDON_WINDOW (window)));
  return FALSE;
}

/* direct cairo drawing on a window must not paint over a window stacked above it */
static gboolean
clip_test (gpointer d)
{
  GdkWindowAttr a; GdkWindow *lower, *upper, *root = gdk_get_default_root_window (); cairo_t *cr; GdkImage *im; guint covered, free_;
  (void) d;
  memset (&a, 0, sizeof a); a.wclass = GDK_INPUT_OUTPUT; a.window_type = GDK_WINDOW_TOPLEVEL; a.event_mask = GDK_EXPOSURE_MASK;
  a.x = 60; a.y = 200; a.width = 200; a.height = 200; lower = gdk_window_new (NULL, &a, GDK_WA_X | GDK_WA_Y); gdk_window_show (lower);
  a.x = 100; a.y = 300; a.width = 120; a.height = 120; upper = gdk_window_new (NULL, &a, GDK_WA_X | GDK_WA_Y); gdk_window_show (upper);
  gdk_window_process_all_updates ();
  cr = gdk_cairo_create (upper); cairo_set_source_rgb (cr, 0, 0, 1); cairo_paint (cr); cairo_destroy (cr);
  cr = gdk_cairo_create (lower); cairo_set_source_rgb (cr, 1, 0, 0); cairo_paint (cr); cairo_destroy (cr);          /* the whole lower window, directly: it must stop at the upper one */
  gdk_display_sync (gdk_display_get_default ());
  im = gdk_drawable_get_image (GDK_DRAWABLE (root), 150, 350, 1, 1); covered = gdk_image_get_pixel (im, 0, 0) & 0xffffff; g_object_unref (im);
  im = gdk_drawable_get_image (GDK_DRAWABLE (root), 70, 210, 1, 1); free_ = gdk_image_get_pixel (im, 0, 0) & 0xffffff; g_object_unref (im);
  LOG ("[phehildonwin] direct clip: pixel under the window above %06x (want 0000ff), free pixel of the lower window %06x (want ff0000)", covered, free_);
  gdk_window_destroy (upper); gdk_window_destroy (lower); gtk_widget_queue_draw (window);
  return FALSE;
}

static gboolean
screenshot_test (gpointer d)
{
  gchar *path = g_build_filename (g_getenv ("PHE_LAUNCH_CACHE") ? g_getenv ("PHE_LAUNCH_CACHE") : "c:/data/launch", "phehildonwin.png", NULL); gsize len = 0; gchar *data = NULL;
  (void) d;
  hildon_gtk_window_take_screenshot_sync (GTK_WINDOW (window), TRUE);
  { gboolean ok = g_file_get_contents (path, &data, &len, NULL); LOG ("[phehildonwin] take_screenshot_sync returned; picture %s readable: %d, %d bytes", path, ok, (int) len); }
  g_free (data); g_free (path);
  return FALSE;
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
  if (!phe_theme_load (phe_log)) gtk_rc_parse ("z:/resource/phehildonwin/phehildon.rc");                          /* the Phorepoc Hildon look (Etesian tokens) */
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
  b = hildon_gtk_button_new (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH); gtk_button_set_label (GTK_BUTTON (b), "Sound"); g_signal_connect (b, "clicked", G_CALLBACK (on_sound), NULL); gtk_box_pack_start (GTK_BOX (vbox), b, FALSE, FALSE, 0);
  b = hildon_gtk_button_new (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH); gtk_button_set_label (GTK_BUTTON (b), "Note"); g_signal_connect (b, "clicked", G_CALLBACK (on_note), NULL); gtk_box_pack_start (GTK_BOX (vbox), b, FALSE, FALSE, 0);
  status = gtk_label_new (NULL); gtk_box_pack_start (GTK_BOX (vbox), status, FALSE, FALSE, 8); update_status ();
  hildon_pannable_area_add_with_viewport (HILDON_PANNABLE_AREA (pannable), vbox);
  gtk_container_add (GTK_CONTAINER (window), pannable);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_widget_show_all (window);
  LOG ("[phehildonwin] window shown (mapped %d)", gtk_widget_get_mapped (window));
  g_timeout_add (2500, report_topmost, NULL); g_timeout_add (4000, screenshot_test, NULL); g_timeout_add (7000, clip_test, NULL);
  LOG ("[phehildonwin] loop running");
  gtk_main ();
  return 0;
}
