# hildon-symbian

libhildon 2.2.30 (`phehildon.dll`) and libhildondesktop 2.1.44 (`phehildondesktop.dll`) for Phorepoc (x32), on gtk-symbian and dbus-symbian: the Maemo / Leste (LGPL) widget set
and home / status plug-in framework, with the Hildon-specific platform bits: Xlib atom/property shims, a GConf store, libcanberra over the platform's MMF audio, libintl, libiphb.
(Follow-up: the GConf and canberra shims are generic and may move to gtk-symbian.) Etesian rc theme in `os/src/phehildonwin/phehildon.rc`. hildon-desktop and the Leste themes are GPL
and are not used. Demos/tests: `phehildonwin`, `phehildondesktoptest`; boot logs in `test/`.
Requires: gtk-symbian, dbus-symbian (and through them glib-symbian).

Nothing here is in an OS image; it is a Catalogue download (SIS with licences and the source offer). Recipes are BSD-2-Clause, the libraries LGPL (see NOTICE).
Build: `REPO=<Symbian-ReverseEngineered checkout> bash recipes/build.sh [steps]` (Git Bash, Docker up). The four repositories sit side by side and share one docker
volume (`VOL`, default hildon-wk) as the build tree and `$REPO/.phorepoc-x86-build/hildon/` as the archive directory; the sibling repositories are mounted read-only at
/glib /gtk /dbus in every container. Plan and findings: `docs/phorepoc/hildon-port.md` in Symbian-ReverseEngineered.

Dependency graph:  glib-symbian  <-  gtk-symbian  <-  hildon-symbian  ->  dbus-symbian  ->  glib-symbian
