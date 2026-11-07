/* phegtkwin_demo.c -- the GTK demo, written against the PUBLIC GTK 2.24 API only: a real GtkWindow with a box of widgets (labels with Pango markup, an entry, a check button,
 * a progress bar, buttons, a frame, a separator), signals, a timeout, and a start-up self test of the widget machinery. Runs on phegtk.dll over the Symbian GDK backend. BSD-2-Clause (Phorepoc). */
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

extern void phe_log (const char *msg);
#define LOG(...) do { char m_[240]; g_snprintf (m_, sizeof m_, __VA_ARGS__); phe_log (m_); } while (0)

static GtkWidget *window, *status, *progress, *entry, *check, *button;
static gint ticks, clicks;
static int st_ok, st_fail;
#define CHECK(c, what) do { if (c) st_ok++; else { st_fail++; LOG ("[phegtkwin] selftest FAIL: %s (line %d)", what, __LINE__); } } while (0)

static void
update_status (void)
{
  gchar *t = g_strdup_printf ("<b>ticks</b> %d    <b>clicks</b> %d    <b>check</b> %s", ticks, clicks, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check)) ? "on" : "off");
  gtk_label_set_markup (GTK_LABEL (status), t); g_free (t);
}

static void on_clicked (GtkButton *b, gpointer d) { (void) b; (void) d; clicks++; update_status (); LOG ("[phegtkwin] button clicked (%d)", clicks); }
static void on_toggled (GtkToggleButton *b, gpointer d) { (void) b; (void) d; update_status (); LOG ("[phegtkwin] check toggled: %d", gtk_toggle_button_get_active (b)); }
static void on_changed (GtkEditable *e, gpointer d) { (void) d; LOG ("[phegtkwin] entry text: %s", gtk_entry_get_text (GTK_ENTRY (e))); }

static gboolean
on_tick (gpointer d)
{
  (void) d; ticks++;
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress), (ticks % 11) / 10.0);
  update_status ();
  return TRUE;
}

/* F6 (the OK key of the device) presses the button, F5 toggles the check button: widget level keyboard use */
static gboolean
on_key (GtkWidget *w, GdkEventKey *e, gpointer d)
{
  (void) w; (void) d;
  LOG ("[phegtkwin] key press keyval=0x%x", e->keyval);
  if (e->keyval == GDK_KEY_F6 || e->keyval == GDK_KEY_F5) { gtk_button_clicked (GTK_BUTTON (button)); return TRUE; }
  if (e->keyval == GDK_KEY_F4) { gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check))); return TRUE; }
  return FALSE;
}

static void
selftest (void)
{
  GtkWidget *l = gtk_label_new ("x"), *w2, *box; GtkRequisition rq; GtkAllocation al;
  CHECK (GTK_IS_LABEL (l) && GTK_IS_WIDGET (l), "label type checks");
  g_object_ref_sink (l);
  gtk_label_set_text (GTK_LABEL (l), "hello"); CHECK (!strcmp (gtk_label_get_text (GTK_LABEL (l)), "hello"), "label text round trip");
  gtk_widget_size_request (l, &rq); CHECK (rq.width > 10 && rq.height > 5, "label size request from Pango metrics");
  g_object_unref (l);
  box = gtk_vbox_new (FALSE, 3); w2 = gtk_button_new_with_label ("b"); gtk_box_pack_start (GTK_BOX (box), w2, FALSE, FALSE, 0);
  CHECK (gtk_widget_get_parent (w2) == box, "box parent"); gtk_widget_destroy (box);
  CHECK (gtk_major_version == 2 && gtk_minor_version == 24, "GTK version 2.24");
  CHECK (gtk_check_version (2, 24, 0) == NULL, "gtk_check_version");
  al.x = 0; al.y = 0; al.width = 50; al.height = 20;
  CHECK (gtk_window_get_type () != 0 && gtk_entry_get_type () != 0 && gtk_progress_bar_get_type () != 0, "widget types register");
  { GtkAdjustment *a = (GtkAdjustment *) gtk_adjustment_new (5, 0, 10, 1, 2, 0); CHECK (gtk_adjustment_get_value (a) == 5, "adjustment value"); g_object_ref_sink (a); g_object_unref (a); }
  { GtkSettings *s = gtk_settings_get_default (); gchar *fn = NULL; g_object_get (s, "gtk-font-name", &fn, NULL); CHECK (fn && *fn, "gtk-font-name setting"); g_free (fn); }
  { GtkAccelGroup *g = gtk_accel_group_new (); CHECK (gtk_accel_group_get_is_locked (g) == FALSE, "accel group"); g_object_unref (g); }
  { GdkWindowAttr a; GdkWindow *io; memset (&a, 0, sizeof a); a.window_type = GDK_WINDOW_TEMP; a.wclass = GDK_INPUT_ONLY; a.x = a.y = -100; a.width = a.height = 10;
    io = gdk_window_new (gdk_screen_get_root_window (gdk_screen_get_default ()), &a, GDK_WA_X | GDK_WA_Y);
    CHECK (gdk_drawable_get_colormap (GDK_DRAWABLE (io)) != NULL, "an input-only window reports a colormap"); gdk_window_destroy (io); }
  LOG ("[phegtkwin] selftest done: %d passed, %d failed", st_ok, st_fail);
}

int
phe_gtk_demo_main (void)
{
  GtkWidget *vbox, *title, *sub, *frame, *fbox, *hbox, *sep;
  if (!gtk_init_check (NULL, NULL)) { LOG ("[phegtkwin] gtk_init_check failed"); return 1; }
  LOG ("[phegtkwin] gtk_init_check ok: GTK %d.%d.%d, screen %dx%d", gtk_major_version, gtk_minor_version, gtk_micro_version,
       gdk_screen_get_width (gdk_screen_get_default ()), gdk_screen_get_height (gdk_screen_get_default ()));
  selftest ();
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "phegtkwin");
  gtk_window_set_default_size (GTK_WINDOW (window), gdk_screen_get_width (gdk_screen_get_default ()), gdk_screen_get_height (gdk_screen_get_default ()));
  gtk_window_move (GTK_WINDOW (window), 0, 0);
  gtk_container_set_border_width (GTK_CONTAINER (window), 12);
  vbox = gtk_vbox_new (FALSE, 10); gtk_container_add (GTK_CONTAINER (window), vbox);
  title = gtk_label_new (NULL); gtk_label_set_markup (GTK_LABEL (title), "<span size='xx-large' weight='bold'>Hildon</span>");
  gtk_box_pack_start (GTK_BOX (vbox), title, FALSE, FALSE, 4);
  sub = gtk_label_new (NULL); gtk_label_set_markup (GTK_LABEL (sub), "a real <b>GtkWindow</b>\nGTK 2.24 on the Symbian GDK backend");
  gtk_label_set_justify (GTK_LABEL (sub), GTK_JUSTIFY_CENTER); gtk_box_pack_start (GTK_BOX (vbox), sub, FALSE, FALSE, 0);
  sep = gtk_hseparator_new (); gtk_box_pack_start (GTK_BOX (vbox), sep, FALSE, FALSE, 2);
  frame = gtk_frame_new ("Widgets"); gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  fbox = gtk_vbox_new (FALSE, 8); gtk_container_set_border_width (GTK_CONTAINER (fbox), 10); gtk_container_add (GTK_CONTAINER (frame), fbox);
  entry = gtk_entry_new (); gtk_entry_set_text (GTK_ENTRY (entry), "an entry"); gtk_box_pack_start (GTK_BOX (fbox), entry, FALSE, FALSE, 0);
  check = gtk_check_button_new_with_label ("a check button (F5)"); gtk_box_pack_start (GTK_BOX (fbox), check, FALSE, FALSE, 0);
  progress = gtk_progress_bar_new (); gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progress), "a progress bar"); gtk_box_pack_start (GTK_BOX (fbox), progress, FALSE, FALSE, 0);
  hbox = gtk_hbox_new (TRUE, 8); gtk_box_pack_start (GTK_BOX (fbox), hbox, FALSE, FALSE, 0);
  button = gtk_button_new_with_label ("Press (F6)"); gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), gtk_button_new_with_label ("Another"), TRUE, TRUE, 0);
  status = gtk_label_new (NULL); gtk_box_pack_start (GTK_BOX (vbox), status, FALSE, FALSE, 6);
  update_status ();
  g_signal_connect (button, "clicked", G_CALLBACK (on_clicked), NULL);
  g_signal_connect (check, "toggled", G_CALLBACK (on_toggled), NULL);
  g_signal_connect (entry, "changed", G_CALLBACK (on_changed), NULL);
  g_signal_connect (window, "key-press-event", G_CALLBACK (on_key), NULL);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  g_timeout_add (400, on_tick, NULL);
  gtk_widget_show_all (window);
  LOG ("[phegtkwin] window shown (mapped %d)", gtk_widget_get_mapped (window));
  LOG ("[phegtkwin] loop running");
  gtk_main ();
  return 0;
}
