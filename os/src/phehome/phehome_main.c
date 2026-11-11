/* Copyright The Phorepoc Authors */
/* SPDX-License-Identifier: BSD-2-Clause */
/* phehome_main.c -- the Home process of the Phorepoc Hildon desktop (a reduced Home: see docs hildon-port.md 7.23 for what the real hildon-home has that this one does not).
 * BSD-2-Clause (Phorepoc). It replaces hildon-home.c (which needs hildon-fm, hildon-thumbnail, osso, mce, osso-bookmark-engine) and keeps what makes a Home: the REAL applet manager
 * of hildon-home (hd-applet-manager.c, hd-widgets.c), which loads the home plug-ins through libhildondesktop's plug-in manager (home.conf, the .desktop entries, GModule), and
 * shows each applet as a window of its own over a Home background window. Hildon-desktop positions applet windows on a real device; here this process does (no layout mode). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include "phe_respath.h"
#include "phe_theme.h"
#include <hildon/hildon.h>
#include <libhildondesktop/libhildondesktop.h>

#include "hd-applet-manager.h"
#include "hd-widgets.h"
#include "hd-install-widgets-dialog.h"

extern void phe_log (const char *msg);
#define LOG(...) do { char m_[300]; g_snprintf (m_, sizeof m_, __VA_ARGS__); phe_log (m_); } while (0)

static GtkWidget *home_window;
static int placed;
static GdkPixbuf *home_bg;                 /* the Home background image (SetBackgroundImage on the D-Bus service), NULL = the theme's */

void phehome_dbus_start (void);

/* the Home background: the image scaled to cover the window, or the theme's background */
static gboolean
home_expose (GtkWidget *w, GdkEventExpose *e)
{
  if (!home_bg) {
    gtk_paint_flat_box (w->style, w->window, GTK_STATE_NORMAL, GTK_SHADOW_NONE, &e->area, w, NULL, 0, 0, -1, -1);
  } else {
    cairo_t *cr = gdk_cairo_create (w->window);
    double sx = (double) w->allocation.width / gdk_pixbuf_get_width (home_bg), sy = (double) w->allocation.height / gdk_pixbuf_get_height (home_bg), sc = sx > sy ? sx : sy;
    gdk_cairo_rectangle (cr, &e->area); cairo_clip (cr);
    cairo_scale (cr, sc, sc); gdk_cairo_set_source_pixbuf (cr, home_bg, 0, 0); cairo_paint (cr);
    cairo_destroy (cr);
  }
  return FALSE;
}

/* file:///z:/path/image.png or a plain path */
gboolean
phehome_set_background (const gchar *uri)
{
  const gchar *path = uri; GdkPixbuf *pb; GError *err = NULL;
  if (g_str_has_prefix (path, "file:///")) path += 8; else if (g_str_has_prefix (path, "file://")) path += 7;
  pb = gdk_pixbuf_new_from_file (path, &err);
  LOG ("[phehome] background image %s: %s", path, pb ? "loaded" : (err ? err->message : "failed"));
  g_clear_error (&err);
  if (!pb) return FALSE;
  if (home_bg) g_object_unref (home_bg);
  home_bg = pb;
  if (home_window) gtk_widget_queue_draw (home_window);
  return TRUE;
}

static void sync_edit_mode (GList *tops);

/* once the plug-in manager has loaded the applets, put their windows on the home view: one below the other from the top left */
static gboolean
place_applets (gpointer data)
{
  GList *l, *tops = gtk_window_list_toplevels ();
  int y = 90, n = 0;
  (void) data;
  for (l = tops; l; l = l->next) {
    GtkWidget *w = l->data;
    if (HD_IS_HOME_PLUGIN_ITEM (w) && GTK_WIDGET_VISIBLE (w)) {       /* a removed applet is hidden at once and destroyed when the plug-in manager has seen the change */
      gchar *id = hd_plugin_item_get_plugin_id (HD_PLUGIN_ITEM (w));
      gboolean fresh = n >= placed;
      if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (w), "phe-placed-y")) != y + 1 && GTK_WIDGET_REALIZED (w)) {      /* only an applet that is not at its place yet is moved and raised */
        g_object_set_data (G_OBJECT (w), "phe-placed-y", GINT_TO_POINTER (y + 1));
        gtk_window_move (GTK_WINDOW (w), 24, y);
        gdk_window_raise (w->window);
      }
      if (FALSE) gdk_window_raise (w->window);          /* the applets belong on top of the Home background (hildon-desktop stacks them there on a real device) */
      if (fresh) LOG ("[phehome] home applet \"%s\" (%s): window at 24,%d, visible %d, realized %d", id ? id : "?", G_OBJECT_TYPE_NAME (w), y, GTK_WIDGET_VISIBLE (w) ? 1 : 0, GTK_WIDGET_REALIZED (w) ? 1 : 0);
      g_free (id);
      y += 130; n++;
    }
  }
  sync_edit_mode (tops);
  g_list_free (tops);
  if (n != placed) LOG ("[phehome] %d home applet window(s) placed", n);
  placed = n;
  return TRUE;                   /* keep watching: applets come and go with the configuration */
}

/* ---- edit mode, following hildon-home's own flow as far as a Home without hildon-desktop can: a long press on the Home enters it; each applet gets a Remove button that sends the
 * applet window the delete-event which hd-applet-manager.c turns into hd_applet_manager_remove_applet (the instance leaves home.plugins for good); "Add widget" opens hildon-home's
 * HDInstallWidgetsDialog on the applet manager's list of installed widgets, whose choice is written to home.plugins the same way. */
static gboolean edit_mode;
static GtkWidget *edit_bar;
static GList *close_wins;                                   /* the Remove windows; each carries its applet in the data "phe-applet" */

static void
remove_clicked (GtkWidget *b, gpointer win)
{
  GtkWidget *applet = g_object_get_data (G_OBJECT (win), "phe-applet");
  GdkEvent *ev;
  gboolean handled = FALSE;
  (void) b;
  if (!applet || !applet->window) return;
  ev = gdk_event_new (GDK_DELETE);
  ev->any.window = g_object_ref (applet->window); ev->any.send_event = TRUE;
  g_signal_emit_by_name (applet, "delete-event", ev, &handled);
  gdk_event_free (ev);
  LOG ("[phehome] edit mode: applet removed");
}

static void
add_clicked (GtkWidget *b, gpointer data)
{
  GtkWidget *dialog = hd_install_widgets_dialog_new (HD_WIDGETS (hd_applet_manager_get ()));
  (void) b; (void) data;
  gtk_widget_show (dialog);
  LOG ("[phehome] edit mode: widget list opened");
}

static void
leave_edit_mode (void)
{
  GList *l;
  edit_mode = FALSE;
  for (l = close_wins; l; l = l->next) gtk_widget_destroy (GTK_WIDGET (l->data));
  g_list_free (close_wins); close_wins = NULL;
  if (edit_bar) { gtk_widget_destroy (edit_bar); edit_bar = NULL; }
  LOG ("[phehome] edit mode: left");
}

static void done_clicked (GtkWidget *b, gpointer data) { (void) b; (void) data; leave_edit_mode (); }

static void
enter_edit_mode (void)
{
  GtkWidget *box, *add, *done;
  int sw = gdk_screen_get_width (gdk_screen_get_default ()), sh = gdk_screen_get_height (gdk_screen_get_default ());
  if (edit_mode) return;
  edit_mode = TRUE;
  edit_bar = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_decorated (GTK_WINDOW (edit_bar), FALSE);
  gtk_window_set_default_size (GTK_WINDOW (edit_bar), sw, 72);
  box = gtk_hbox_new (TRUE, 6);
  add = hildon_button_new_with_text (HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_VERTICAL, "Add widget", NULL);
  done = hildon_button_new_with_text (HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_VERTICAL, "Done", NULL);
  g_signal_connect (add, "clicked", G_CALLBACK (add_clicked), NULL);
  g_signal_connect (done, "clicked", G_CALLBACK (done_clicked), NULL);
  gtk_box_pack_start (GTK_BOX (box), add, TRUE, TRUE, 6); gtk_box_pack_start (GTK_BOX (box), done, TRUE, TRUE, 6);
  gtk_container_add (GTK_CONTAINER (edit_bar), box);
  gtk_widget_show_all (edit_bar);
  gtk_window_move (GTK_WINDOW (edit_bar), 0, sh - 72);
  LOG ("[phehome] edit mode: entered");
}

/* every tick: in edit mode each shown applet has its Remove window at its top right edge, and a Remove window whose applet is gone goes too */
static void
sync_edit_mode (GList *tops)
{
  GList *l, *n;
  if (!edit_mode) return;
  for (l = close_wins; l; l = n) {
    GtkWidget *a = g_object_get_data (G_OBJECT (l->data), "phe-applet");
    n = l->next;
    if (!g_list_find (tops, a) || !GTK_WIDGET_VISIBLE (a)) { gtk_widget_destroy (GTK_WIDGET (l->data)); close_wins = g_list_delete_link (close_wins, l); }
  }
  for (l = tops; l; l = l->next) {
    GtkWidget *w = l->data, *c = NULL;
    GList *k;
    if (!HD_IS_HOME_PLUGIN_ITEM (w) || !GTK_WIDGET_VISIBLE (w) || !GTK_WIDGET_REALIZED (w)) continue;
    for (k = close_wins; k; k = k->next) if (g_object_get_data (G_OBJECT (k->data), "phe-applet") == w) c = k->data;
    if (!c) {
      GtkWidget *b = hildon_button_new_with_text (HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_VERTICAL, "Remove", NULL);
      c = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_decorated (GTK_WINDOW (c), FALSE);
      gtk_window_set_default_size (GTK_WINDOW (c), 130, 64);
      g_object_set_data (G_OBJECT (c), "phe-applet", w);
      g_signal_connect (b, "clicked", G_CALLBACK (remove_clicked), c);
      gtk_container_add (GTK_CONTAINER (c), b);
      gtk_widget_show_all (c);
      close_wins = g_list_prepend (close_wins, c);
    }
    { int x = 24 + w->allocation.width - 130, y = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (w), "phe-placed-y")) - 1;
      gtk_window_move (GTK_WINDOW (c), x < 0 ? 0 : x, y < 0 ? 0 : y);
      if (c->window) gdk_window_raise (c->window); }
  }
  if (edit_bar && edit_bar->window) gdk_window_raise (edit_bar->window);
}

/* a press that stays put on the Home for 800 ms enters edit mode */
static guint hold_id;
static double hold_x, hold_y;
static gboolean hold_fired (gpointer d) { (void) d; hold_id = 0; enter_edit_mode (); return FALSE; }
static gboolean
home_press (GtkWidget *w, GdkEventButton *e, gpointer d)
{
  (void) w; (void) d;
  if (e->button != 1 || edit_mode) return FALSE;
  hold_x = e->x; hold_y = e->y;
  if (hold_id) g_source_remove (hold_id);
  hold_id = g_timeout_add (800, hold_fired, NULL);
  return FALSE;
}
static gboolean
home_release (GtkWidget *w, GdkEventButton *e, gpointer d)
{
  (void) w; (void) e; (void) d;
  if (hold_id) { g_source_remove (hold_id); hold_id = 0; }
  return FALSE;
}
static gboolean
home_motion (GtkWidget *w, GdkEventMotion *e, gpointer d)
{
  (void) w; (void) d;
  if (hold_id && (e->x - hold_x > 12 || hold_x - e->x > 12 || e->y - hold_y > 12 || hold_y - e->y > 12)) { g_source_remove (hold_id); hold_id = 0; }
  return FALSE;
}

/* test only (a marker file z:/resource/phehome/alpha.demo in the test ROM): a window of the RGBA colormap that paints a half transparent rounded rectangle, to see per-pixel alpha working */
static gboolean
alpha_demo_expose (GtkWidget *w, GdkEventExpose *e)
{
  cairo_t *cr = gdk_cairo_create (w->window);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE); cairo_set_source_rgba (cr, 0, 0, 0, 0); cairo_paint (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER); cairo_set_source_rgba (cr, 0.11, 0.32, 0.69, 0.5);
  cairo_arc (cr, 40, 40, 36, 0, 6.2832); cairo_fill (cr);
  cairo_set_source_rgba (cr, 0.8, 0.1, 0.1, 1.0); cairo_rectangle (cr, 90, 20, 60, 40); cairo_fill (cr);
  cairo_destroy (cr);
  return FALSE;
}

/* test only (marker z:/resource/phehome/livecfg.test): some seconds after start the user's home.plugins is rewritten with a second applet instance; the plug-in manager watches the configuration (GIO file monitor) and must create the applet without a restart */
static gboolean
livecfg_write (gpointer data)
{
  const gchar *cfg = "[phehomeexample-0]\nX-Desktop-File=z:/resource/hildondesktop/entries/home/phehomeexample.desktop\n\n[phehomeexample-1]\nX-Desktop-File=z:/resource/hildondesktop/entries/home/phehomeexample.desktop\n";
  gboolean ok = g_file_set_contents ("c:/data/phehome/config/hildon-desktop/home.plugins", cfg, -1, NULL);
  (void) data;
  LOG ("[phehome] live config: wrote user home.plugins with a second applet instance: %s", ok ? "ok" : "FAILED");
  return FALSE;
}

static gboolean
tap_seen (GtkWidget *w, GdkEventButton *e, gpointer name)
{
  (void) w;
  LOG ("[phehome] tap: button press on %s at %d,%d", (const char *) name, (int) e->x, (int) e->y);
  return FALSE;
}

static void
alpha_demo (void)
{
  GtkWidget *w; GdkScreen *screen;
  if (!g_file_test ("z:/resource/phehome/alpha.demo", G_FILE_TEST_EXISTS)) return;
  w = gtk_window_new (GTK_WINDOW_TOPLEVEL); screen = gtk_widget_get_screen (w);
  LOG ("[phehome] alpha demo: rgba colormap %p, screen composited %d", (void *) gdk_screen_get_rgba_colormap (screen), gdk_screen_is_composited (screen) ? 1 : 0);
  gtk_widget_set_colormap (w, gdk_screen_get_rgba_colormap (screen));
  gtk_widget_set_app_paintable (w, TRUE); gtk_window_set_decorated (GTK_WINDOW (w), FALSE); gtk_window_set_default_size (GTK_WINDOW (w), 170, 80);
  g_signal_connect (w, "expose-event", G_CALLBACK (alpha_demo_expose), NULL);
  { GtkWidget *l = gtk_label_new ("Alpha label"); gtk_misc_set_alignment (GTK_MISC (l), 0.0, 1.0); gtk_misc_set_padding (GTK_MISC (l), 6, 6); gtk_container_add (GTK_CONTAINER (w), l); gtk_widget_show (l); }
  gtk_widget_show (w); gtk_window_move (GTK_WINDOW (w), 24, 300);
  gtk_widget_add_events (w, GDK_BUTTON_PRESS_MASK); g_signal_connect (w, "button-press-event", G_CALLBACK (tap_seen), (gpointer) "the alpha window");
  gtk_widget_add_events (home_window, GDK_BUTTON_PRESS_MASK); g_signal_connect (home_window, "button-press-event", G_CALLBACK (tap_seen), (gpointer) "the Home window");
}

/* The shipped home.conf / home.plugins name their files on Z: (where a ROM build has them). Installed from a package they are on the install drive: write the user's copies with
 * that drive (home.conf every start, it holds nothing of the user's; home.plugins only the first time, it is the list of applet instances the user edits). */
static void
prepare_config (void)
{
  gchar *dir = g_strdup (phe_res ("resource/hildondesktop")); static const char *files[] = { "home.conf", "home.plugins", NULL }; int i;
  if (dir[0] != 'z' && dir[0] != 'Z') {
    gchar *repl = g_strdup_printf ("%c:/resource", dir[0]);
    for (i = 0; files[i]; i++) {
      gchar *src = g_build_filename (dir, files[i], NULL), *dst = g_build_filename ("c:/data/phehome/config/hildon-desktop", files[i], NULL), *text = NULL;
      if ((i == 0 || !g_file_test (dst, G_FILE_TEST_EXISTS)) && g_file_get_contents (src, &text, NULL, NULL)) {
        gchar **parts = g_strsplit (text, "z:/resource", -1), *out = g_strjoinv (repl, parts);
        g_file_set_contents (dst, out, -1, NULL);
        LOG ("[phehome] configuration %s of the install drive: %s", files[i], dst);
        g_strfreev (parts); g_free (out);
      }
      g_free (text); g_free (src); g_free (dst);
    }
    g_free (repl);
  }
  g_free (dir);
}

static gboolean
stamp_done (gpointer d)
{
  (void) d;
  hd_stamp_file_finalize ("c:/data/phehome/home.stamp");
  return FALSE;
}

int
phe_home_main (void)
{
  GtkWidget *label, *box;
  g_setenv ("XDG_CONFIG_HOME", "c:/data/phehome/config", TRUE);       /* the user part of the plug-in configuration (the applet instances are stored there) */
  g_mkdir_with_parents ("c:/data/phehome/config/hildon-desktop", 0755);
  if (!gtk_init_check (NULL, NULL)) return 1;
  hildon_init ();
  if (!phe_theme_load (phe_log)) gtk_rc_parse (phe_res ("resource/phehome/phehildon.rc"));            /* the Hildon theme (alpha, the Maemo Leste default) when its package is installed, else our own look */
  LOG ("[phehome] gtk + hildon init ok, screen %dx%d", gdk_screen_get_width (gdk_screen_get_default ()), gdk_screen_get_height (gdk_screen_get_default ()));

  /* the Home background window */
  home_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (home_window), "Home");
  gtk_window_set_default_size (GTK_WINDOW (home_window), gdk_screen_get_width (gdk_screen_get_default ()), gdk_screen_get_height (gdk_screen_get_default ()));
  box = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (home_window), box);
  gtk_widget_set_app_paintable (home_window, TRUE);
  g_signal_connect (home_window, "expose-event", G_CALLBACK (home_expose), NULL);
  { const char *wp = phe_res ("resource/gtk/themes/alpha/backgrounds/wallpaper1.png");                      /* the theme's first wallpaper (Leste's default background) */
    if (access (wp, F_OK) == 0) phehome_set_background (wp); }
  gtk_widget_add_events (home_window, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
  g_signal_connect (home_window, "button-press-event", G_CALLBACK (home_press), NULL);
  g_signal_connect (home_window, "button-release-event", G_CALLBACK (home_release), NULL);
  g_signal_connect (home_window, "motion-notify-event", G_CALLBACK (home_motion), NULL);
  gtk_widget_show_all (home_window);

  /* the real applet manager: loads home.conf, finds the plug-in entries, loads the plug-in modules and creates the applets */
  g_timeout_add_seconds (8, stamp_done, NULL);          /* the plug-ins loaded without a crash: remove the stamp (hildon-home does it when it quits, which this process, killed with its console, never does; the next start would be a safe-mode start with no applets) */
  hd_stamp_file_init ("c:/data/phehome/home.stamp");     /* what the plug-in manager uses to see that a plug-in loaded without crashing the process (hildon-home does the same at start-up) */
  prepare_config ();
  hd_applet_manager_get ();
  LOG ("[phehome] applet manager created");
  phehome_dbus_start ();
  alpha_demo ();
  if (g_file_test ("z:/resource/phehome/livecfg.test", G_FILE_TEST_EXISTS)) g_timeout_add_seconds (12, livecfg_write, NULL);
  g_timeout_add (500, place_applets, NULL);
  gtk_main ();
  return 0;
}

/* the resource of an installed package, wherever it was installed (phe_respath.h); the C++ entry point asks for the font configuration through this */
const char *
phehome_res (const char *rel)
{
  return phe_res (rel);
}
