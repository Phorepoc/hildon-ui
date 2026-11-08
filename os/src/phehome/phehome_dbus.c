/* phehome_dbus.c -- the Home's D-Bus service com.nokia.HildonHome on the session bus. BSD-2-Clause (Phorepoc).
 * The interface is hildon-home's own: hd-hildon-home-dbus.xml of the pinned hildon-home, turned into dbus-glib server glue by the REAL dbus-binding-tool of the pinned dbus-glib tarball
 * (dbus-symbian/recipes/gen_glue.sh, native build; the generated hd-hildon-home-dbus-glue.h is included below). hildon-home's own implementation of the object (hd-hildon-home-dbus.c) needs
 * libosso, the edit-mode menu and the backgrounds subsystem (hildon-thumbnail), which this reduced Home does not have, so the object is ours:
 *   SetBackgroundImage (uri)   works: the image becomes the Home background (phehome_set_background in phehome_main.c)
 *   ShowEditMenu (view)        answers org.freedesktop.DBus.Error.NotSupported: the reduced Home has no edit mode (docs hildon-port.md 7.23) */
#include <string.h>
#include <spawn.h>
#include <gtk/gtk.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

extern void phe_log (const char *msg);
extern gboolean phehome_set_background (const gchar *uri);
#define LOG(...) do { char m_[300]; g_snprintf (m_, sizeof m_, __VA_ARGS__); phe_log (m_); } while (0)

typedef struct { GObject parent; } HDHildonHomeDBus;
typedef struct { GObjectClass parent_class; } HDHildonHomeDBusClass;
GType hd_hildon_home_dbus_get_type (void);
G_DEFINE_TYPE (HDHildonHomeDBus, hd_hildon_home_dbus, G_TYPE_OBJECT)
static void hd_hildon_home_dbus_init (HDHildonHomeDBus *d) { (void) d; }
static void hd_hildon_home_dbus_class_init (HDHildonHomeDBusClass *c) { (void) c; }

gboolean hd_hildon_home_dbus_show_edit_menu (HDHildonHomeDBus *dbus, guint current_view, DBusGMethodInvocation *context);
gboolean hd_hildon_home_dbus_set_background_image (HDHildonHomeDBus *dbus, const gchar *uri, DBusGMethodInvocation *context);

#include "hd-hildon-home-dbus-glue.h"

gboolean
hd_hildon_home_dbus_show_edit_menu (HDHildonHomeDBus *dbus, guint current_view, DBusGMethodInvocation *context)
{
  GError *e = g_error_new (DBUS_GERROR, DBUS_GERROR_NOT_SUPPORTED, "the reduced Home has no edit mode");
  (void) dbus;
  LOG ("[phehome] D-Bus ShowEditMenu (%u): not supported (no edit mode in this Home)", current_view);
  dbus_g_method_return_error (context, e);
  g_error_free (e);
  return TRUE;
}

gboolean
hd_hildon_home_dbus_set_background_image (HDHildonHomeDBus *dbus, const gchar *uri, DBusGMethodInvocation *context)
{
  (void) dbus;
  LOG ("[phehome] D-Bus SetBackgroundImage (%s)", uri);
  if (phehome_set_background (uri)) dbus_g_method_return (context);
  else {
    GError *e = g_error_new (DBUS_GERROR, DBUS_GERROR_FILE_NOT_FOUND, "cannot load the image %s", uri);
    dbus_g_method_return_error (context, e); g_error_free (e);
  }
  return TRUE;
}

extern int phehome_spawn (const char *exe, const char *args);

/* test ROM: z:/resource/phehome/dbus.selftest holds the arguments of a phebuscall run, started once the service is up (a second process calling the Home's methods over the bus) */
static void
selftest (void)
{
  gchar *args = NULL;
  if (g_file_get_contents ("z:/resource/phehome/dbus.selftest", &args, NULL, NULL)) {
    LOG ("[phehome] D-Bus self test: phebuscall %s", args);
    LOG ("[phehome] spawn -> %d", phehome_spawn ("phebuscall.exe", args));
    g_free (args);
  }
}

static int tries;

static gboolean
register_service (gpointer data)
{
  GError *err = NULL; DBusGConnection *c; HDHildonHomeDBus *obj; guint r;
  (void) data;
  c = dbus_g_bus_get (DBUS_BUS_SESSION, &err);
  if (!c) {
    if (tries == 0) {                                             /* no session bus: start phedbusd on demand, as every client of this system does (dbus-symbian README) */
      pid_t pid; char *dargv[] = { (char *) "phedbusd.exe", (char *) "--config-file=z:/resource/dbus/session.conf", (char *) "--nofork", (char *) "--nopidfile", NULL };
      int sr = posix_spawn (&pid, "phedbusd.exe", NULL, NULL, dargv, NULL);
      LOG ("[phehome] no session bus: starting phedbusd on demand -> %d", sr);
    }
    if (++tries == 1 || tries % 10 == 0) LOG ("[phehome] session bus not there yet (%s), asking again", err ? err->message : "?");
    g_clear_error (&err);
    return tries < 120;                                           /* the bus starts in the background: try again every second for a while */
  }
  dbus_g_object_type_install_info (hd_hildon_home_dbus_get_type (), &dbus_glib_hd_hildon_home_dbus_object_info);
  obj = g_object_new (hd_hildon_home_dbus_get_type (), NULL);
  dbus_g_connection_register_g_object (c, "/com/nokia/HildonHome", G_OBJECT (obj));
  r = dbus_bus_request_name (dbus_g_connection_get_connection (c), "com.nokia.HildonHome", 0, NULL);
  LOG ("[phehome] D-Bus service com.nokia.HildonHome on the session bus: request_name -> %u (1 = primary owner), %d attempt(s)", r, tries + 1);
  selftest ();
  return FALSE;
}

void
phehome_dbus_start (void)
{
  g_timeout_add_seconds (1, register_service, NULL);
}
