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
  gtk_widget_show_all (home_window);

  /* the real applet manager: loads home.conf, finds the plug-in entries, loads the plug-in modules and creates the applets */
  hd_stamp_file_init ("c:/data/phehome/home.stamp");     /* what the plug-in manager uses to see that a plug-in loaded without crashing the process (hildon-home does the same at start-up) */
  hd_applet_manager_get ();
  LOG ("[phehome] applet manager created");
  g_timeout_add (500, place_applets, NULL);
  gtk_main ();
  return 0;
}
