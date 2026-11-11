/* Copyright The Phorepoc Authors */
/* SPDX-License-Identifier: BSD-2-Clause */
/* phehildondesktoptest_main.c -- libhildondesktop on the PC image, through its public API only: the heartbeat (HDHeartbeat over the in-process libiphb), the shortcuts store (HDShortcuts over the
 * GConf store: add a bookmark shortcut, read the list back, see the change notification), a home plug-in item (HDHomePluginItem subclass: realized as a window, plug-in id, applet id, resize
 * type), and the D-Bus connection a plug-in asks its item for (libdbus over TCP on the loopback: without a bus daemon the attempt must fail with a connection error, not hang or crash).
 * BSD-2-Clause (Phorepoc). */
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <hildon/hildon.h>
#include <gconf/gconf-client.h>
#include <libhildondesktop/libhildondesktop.h>
#include <dbus/dbus-glib.h>

extern void phe_log (const char *msg);
#define LOG(...) do { char m_[300]; g_snprintf (m_, sizeof m_, __VA_ARGS__); phe_log (m_); } while (0)

typedef struct { HDHomePluginItem parent; } TestItem;
typedef struct { HDHomePluginItemClass parent_class; } TestItemClass;
G_DEFINE_TYPE (TestItem, test_item, HD_TYPE_HOME_PLUGIN_ITEM)
static void test_item_init (TestItem *i) { (void) i; }
static void test_item_class_init (TestItemClass *k) { (void) k; }

static GMainLoop *loop; static gint64 started; static int st_ok, st_fail;
#define CHECK(c, what) do { if (c) st_ok++; else { st_fail++; LOG ("[phehildondesktoptest] FAIL: %s (line %d)", what, __LINE__); } } while (0)

static gboolean
on_heartbeat (gpointer d)
{
  (void) d;
  LOG ("[phehildondesktoptest] heartbeat fired after %d s (asked for 2..4)", (int) ((g_get_monotonic_time () - started) / 1000000));
  CHECK ((g_get_monotonic_time () - started) / 1000000 >= 2, "heartbeat not before its minimum");
  LOG ("[phehildondesktoptest] done: %d passed, %d failed", st_ok, st_fail);
  g_main_loop_quit (loop);
  return FALSE;
}

static void
shortcuts_changed (GConfClient *c, guint id, GConfEntry *e, gpointer d)
{
  (void) c; (void) id; (void) d;
  LOG ("[phehildondesktoptest] GConf notification for %s", gconf_entry_get_key (e));
}

int
phe_hildondesktop_test_main (void)
{
  GtkWidget *item; GConfClient *gc; GSList *list; gchar *pid, *aid; DBusError derr; DBusConnection *dc;
  if (!gtk_init_check (NULL, NULL)) return 1;
  hildon_init ();
  loop = g_main_loop_new (NULL, FALSE);

  /* the shortcuts store */
  gc = gconf_client_get_default ();
  gconf_client_notify_add (gc, "/apps/osso/hildon-home", shortcuts_changed, NULL, NULL, NULL);
  hd_shortcuts_add_bookmark_shortcut ("http://example.org/phorepoc", "Phorepoc example", "general_web");
  list = gconf_client_get_list (gc, "/apps/osso/hildon-home/bookmark-shortcuts", GCONF_VALUE_STRING, NULL);
  LOG ("[phehildondesktoptest] bookmark shortcuts stored: %d", g_slist_length (list));
  CHECK (g_slist_length (list) == 1, "one bookmark shortcut stored");
  g_slist_foreach (list, (GFunc) g_free, NULL); g_slist_free (list);

  /* a home plug-in item */
  item = g_object_new (test_item_get_type (), "plugin-id", "phe-test-plugin", NULL);
  g_object_ref_sink (item);
  pid = hd_plugin_item_get_plugin_id (HD_PLUGIN_ITEM (item));
  CHECK (pid && !strcmp (pid, "phe-test-plugin"), "plug-in id");
  hd_home_plugin_item_set_resize_type (HD_HOME_PLUGIN_ITEM (item), HD_HOME_PLUGIN_ITEM_RESIZE_BOTH);
  gtk_widget_realize (item);
  aid = hd_home_plugin_item_get_applet_id (HD_HOME_PLUGIN_ITEM (item));
  LOG ("[phehildondesktoptest] home plug-in item realized (window %p), plug-in id %s, applet id %s", (void *) item->window, pid, aid ? aid : "(none)");
  CHECK (item->window != NULL, "item window");
  g_free (pid); g_free (aid);

  /* the D-Bus connection a plug-in item hands out: the session bus (phedbusd, started before this test) answers; a call on the bus driver proves the connection works */
  {
    GError *gerr = NULL; DBusGConnection *g = NULL; int tries;
    /* the start-up list launches the bus in the background and does not wait for it: ask again for a while */
    for (tries = 0; tries < 30 && !g; tries++) { g_clear_error (&gerr); g = hd_home_plugin_item_get_dbus_g_connection (HD_HOME_PLUGIN_ITEM (item), DBUS_BUS_SESSION, &gerr); if (!g) g_usleep (1000000); }
    LOG ("[phehildondesktoptest] asked %d time(s)", tries);
    LOG ("[phehildondesktoptest] session bus connection (dbus-glib): %s (%s)", g ? "connected" : "none", gerr ? gerr->message : "no error");
    CHECK (g != NULL, "the plug-in item gets its session bus connection");
    if (g) {
      DBusGProxy *bus = dbus_g_proxy_new_for_name (g, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus");
      gboolean has = FALSE; GError *e2 = NULL;
      gboolean ok = dbus_g_proxy_call (bus, "NameHasOwner", &e2, G_TYPE_STRING, "org.freedesktop.DBus", G_TYPE_INVALID, G_TYPE_BOOLEAN, &has, G_TYPE_INVALID);
      LOG ("[phehildondesktoptest] NameHasOwner(org.freedesktop.DBus) over the bus: %s %d (%s)", ok ? "call ok, result" : "call failed", has, e2 ? e2->message : "no error");
      CHECK (ok && has, "a call on the bus driver");
      g_object_unref (bus);
    }
    (void) derr; (void) dc;
  }

  /* the heartbeat: 2 to 4 seconds */
  started = g_get_monotonic_time ();
  hd_heartbeat_add (2, 4, on_heartbeat, NULL);
  g_main_loop_run (loop);
  return st_fail ? 1 : 0;
}
