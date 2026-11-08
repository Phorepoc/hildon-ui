> libhildon, libhildondesktop and a reduced Home screen: the Maemo-style touch UI libraries on GTK.

The Hildon touch widgets (libhildon), the desktop plug-in framework (libhildondesktop) and `phehome.exe`, a reduced Home screen built on hildon-home's applet manager, with an example home applet. Home owns `com.nokia.HildonHome` on the session bus and is started on demand. It needs the GLib, GTK and D-Bus packages.

The libraries are LGPL and the like, kept in DLLs of their own that you can replace. The package installs every licence text and the source offer in `\resource\hildon-ui\licences\`. Needs: GLib (glib-symbian), GTK (gtk-symbian), D-Bus (dbus-symbian).
