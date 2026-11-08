/* phehome_main.c -- the Home process of the Phorepoc Hildon desktop (a reduced Home: see docs hildon-port.md 7.23 for what the real hildon-home has that this one does not).
 * BSD-2-Clause (Phorepoc). It replaces hildon-home.c (which needs hildon-fm, hildon-thumbnail, osso, mce, osso-bookmark-engine) and keeps what makes a Home: the REAL applet manager
 * of hildon-home (hd-applet-manager.c, hd-widgets.c), which loads the home plug-ins through libhildondesktop's plug-in manager (home.conf, the .desktop entries, GModule), and
 * shows each applet as a window of its own over a Home background window. Hildon-desktop positions applet windows on a real device; here this process does (no layout mode). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <hildon/hildon.h>
#include <libhildondesktop/libhildondesktop.h>

#include "hd-applet-manager.h"

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

/* once the plug-in manager has loaded the applets, put their windows on the home view: one below the other from the top left */
static gboolean
place_applets (gpointer data)
{
  GList *l, *tops = gtk_window_list_toplevels ();
  int y = 90, n = 0;
  (void) data;
  for (l = tops; l; l = l->next) {
    GtkWidget *w = l->data;
    if (HD_IS_HOME_PLUGIN_ITEM (w)) {
      gchar *id = hd_plugin_item_get_plugin_id (HD_PLUGIN_ITEM (w));
      gtk_window_move (GTK_WINDOW (w), 24, y);
      if (GTK_WIDGET_REALIZED (w)) gdk_window_raise (w->window);          /* the applets belong on top of the Home background (hildon-desktop stacks them there on a real device) */
      LOG ("[phehome] home applet \"%s\" (%s): window at 24,%d, visible %d, realized %d", id ? id : "?", G_OBJECT_TYPE_NAME (w), y, GTK_WIDGET_VISIBLE (w) ? 1 : 0, GTK_WIDGET_REALIZED (w) ? 1 : 0);
      g_free (id);
      y += 130; n++;
    }
  }
  g_list_free (tops);
  if (n) placed = n;
  LOG ("[phehome] %d home applet window(s) placed", n);
  return n == 0 && !placed;      /* try again until the applets are there */
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
}

int
phe_home_main (void)
{
  GtkWidget *label, *box;
  g_setenv ("XDG_CONFIG_HOME", "c:/data/phehome/config", TRUE);       /* the user part of the plug-in configuration (the applet instances are stored there) */
  g_mkdir_with_parents ("c:/data/phehome/config/hildon-desktop", 0755);
  if (!gtk_init_check (NULL, NULL)) return 1;
  hildon_init ();
  gtk_rc_parse ("z:/resource/phehome/phehildon.rc");
  LOG ("[phehome] gtk + hildon init ok, screen %dx%d", gdk_screen_get_width (gdk_screen_get_default ()), gdk_screen_get_height (gdk_screen_get_default ()));

  /* the Home background window */
  home_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (home_window), "Home");
  gtk_window_set_default_size (GTK_WINDOW (home_window), gdk_screen_get_width (gdk_screen_get_default ()), gdk_screen_get_height (gdk_screen_get_default ()));
  box = gtk_vbox_new (FALSE, 0);
  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), "<span size='x-large' weight='bold'>Home</span>");
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0); gtk_misc_set_padding (GTK_MISC (label), 24, 24);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (home_window), box);
  gtk_widget_set_app_paintable (home_window, TRUE);
  g_signal_connect (home_window, "expose-event", G_CALLBACK (home_expose), NULL);
  gtk_widget_show_all (home_window);

  /* the real applet manager: loads home.conf, finds the plug-in entries, loads the plug-in modules and creates the applets */
  hd_stamp_file_init ("c:/data/phehome/home.stamp");     /* what the plug-in manager uses to see that a plug-in loaded without crashing the process (hildon-home does the same at start-up) */
  hd_applet_manager_get ();
  LOG ("[phehome] applet manager created");
  phehome_dbus_start ();
  alpha_demo ();
  g_timeout_add (500, place_applets, NULL);
  gtk_main ();
  return 0;
}
