#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: BSD-2-Clause
# build_hcp_x32.sh -- IN phorepoc-x86 (volume at /wk, this repo at /h, siblings at /glib /gtk /dbus): the Hildon control panel, its applets and the clock home widget.
#  libphehcp.a = the REAL hildon-control-panel (github.com/maemo-leste/hildon-control-panel at the commit pinned in pins.sh, LGPL-2.1, cloned into dl/hildon-control-panel),
#    compiled unmodified with MAEMO_TOOLS off (no codelockui / operator tools), + our libosso calls and start (os/src/phehcp/phehcp_osso.c; shims/libosso.h, shims/dlfcn.h over GModule)
#  libphehcpdatetime.a, libphehcpregion.a = the two stand-in applets (os/src/phehcp*), libphehomeclock.a = the clock home widget (os/src/phehomeclock)
# Needs the libhildon / libhildondesktop build (build_hildon_x32.sh, build_hildondesktop_x32.sh) in the same volume.
# Out: /wk/hcp-x32/*.o, /wk/out/libphehcp.a, /wk/out/libphehcpdatetime.a, /wk/out/libphehcpregion.a, /wk/out/libphehomeclock.a
set -eu
. /glib/recipes/pins.sh; . /gtk/recipes/pins.sh; . /dbus/recipes/pins.sh; . /h/recipes/pins.sh
CC=/wk/lsc/bin/pp-gcc
CP=/h/dl/hildon-control-panel/src; HD=/h/dl/libhildondesktop; O=/wk/hcp-x32; mkdir -p $O /wk/out; SH=/h/os/src/phehildon; G=/wk/gtk+-$GTK_VER
[ -d $CP ] || { echo "no $CP: run recipes/fetch.sh"; exit 1; }
printf '#define PACKAGE "hildon-control-panel"\n#define GETTEXT_PACKAGE "hildon-control-panel"\n#define VERSION "2.2.27"\n' > $O/config.h
X32="-I/wk/x32 -I/wk/x32/glib -I/wk/glib-2.56.4 -I/wk/glib-2.56.4/glib -I/wk/glib-2.56.4/gobject -I/wk/glib-2.56.4/gmodule -I/wk/glib-2.56.4/gio -I/wk/ffi-x32/include"
FF="-O2 -g0 -w -fno-pic -I/h/os/src/phehcp/shims -I$SH/shims -I/glib/overlay -include /glib/overlay/phe_glib_compat.h -include /glib/overlay/phe_respath.h -include /wk/gdk-x32/gtkcompat.h -include $SH/shims/phe_locale.h \
 -include /dbus/os/src/phedbus/phedbus_compat.h -DHAVE_CONFIG_H -DMAEMO_GTK -DDBUS_API_SUBJECT_TO_CHANGE -DLOCALEDIR=\"z:/resource/locale\" -DPREFIXDIR=\"z:\" \
 -DCONTROLPANEL_ENTRY_DIR=phe_res(\"resource/hildon-control-panel\") -DHCP_PLUGIN_DIR=\"z:/sys/bin\" \
 -I$O -I$CP -I$HD -I/wk/hdesktop-x32/inc -I$SH/shims -I/wk/hildon-x32 -I/wk/hildon-x32/inc -I/h/dl/libhildon -I/wk/dbusglib-x32/inc -I/wk/dbus-glib-$DBUSGLIB_VER -I/wk/dbus-glib-$DBUSGLIB_VER/dbus \
 -I/wk/dbus-x32 -I/wk/dbus-$DBUS_VER -I$G -I$G/gdk -I/wk/gdk-x32 -I/wk/atk-$ATK_VER -I/wk/gtk-x32 -I/wk/gtk-x32/inc $X32 -I/wk/cairo-1.16.0/src -I/wk/cairo-x32/src -I/wk/pixman-0.42.2/pixman \
 -I/wk/pango-1.40.14 -I/wk/pango-x32 -I/wk/harfbuzz-$HARFBUZZ_VER/src -I/wk/fontconfig-2.14.2 -I/wk/freetype-2.13.3/include -I/wk/gdk-pixbuf-2.36.12 -I/wk/pixbuf-x32"
rm -f $O/*.o $O/errs.*
one() { $CC $FF "${@:3}" -c $1 -o $O/$2.o 2> $O/errs.$2 || echo "FAIL $2"; }
# the marshallers, from GLib's own glib-genmarshal (Python, in the GLib source of the volume)
sed -e 's|@PYTHON@|python3|' -e 's|@VERSION@|2.56.4|' -e 's|@GLIB_MAJOR_VERSION@|2|' -e 's|@GLIB_MINOR_VERSION@|56|' -e 's|@GLIB_MICRO_VERSION@|4|' /wk/glib-2.56.4/gobject/glib-genmarshal.in > $O/glib-genmarshal
python3 $O/glib-genmarshal --prefix=hcp_marshal --header $CP/hcp-marshalers.list > $O/hcp-marshalers.h
python3 $O/glib-genmarshal --prefix=hcp_marshal --body $CP/hcp-marshalers.list > $O/hcp-marshalers.c
one $O/hcp-marshalers.c hcp-marshalers
one $CP/hcp-program.c hcp-program -include /h/os/src/phehcp/shims/phe_hcp_show.h     # its gtk_widget_show_all sizes the window to the screen (phehcp_osso.c)
for f in hcp-window hcp-app hcp-app-list hcp-app-view hcp-grid; do one $CP/$f.c $f; done
one $CP/hcp-main.c hcp-main -Dmain=phe_hcp_main -Dhildon_init=phe_hcp_hildon_init
one /h/os/src/phehcp/phehcp_osso.c phehcp_osso
one /h/os/src/phehcpdatetime/phehcpdatetime.c phehcpdatetime
one /h/os/src/phehcpregion/phehcpregion.c phehcpregion
one /h/os/src/phehomeclock/phehomeclock.c phehomeclock -I/wk/hdesktop-x32/inc/libhildondesktop
cat $O/errs.* > $O/errors.txt 2>/dev/null || true; rm -f $O/errs.*
grep -a "error" $O/errors.txt | cut -c1-200 | head -30
rm -f /wk/out/libphehcp.a /wk/out/libphehcpdatetime.a /wk/out/libphehcpregion.a /wk/out/libphehomeclock.a
ar rcs /wk/out/libphehcp.a $O/hcp-marshalers.o $O/hcp-program.o $O/hcp-window.o $O/hcp-app.o $O/hcp-app-list.o $O/hcp-app-view.o $O/hcp-grid.o $O/hcp-main.o $O/phehcp_osso.o
ar rcs /wk/out/libphehcpdatetime.a $O/phehcpdatetime.o
ar rcs /wk/out/libphehcpregion.a $O/phehcpregion.o
ar rcs /wk/out/libphehomeclock.a $O/phehomeclock.o
echo "libphehcp.a: $(ar t /wk/out/libphehcp.a | wc -l) members"
GO=$(ls /wk/hildon-x32/*.o /wk/gtk-x32/*.o /wk/gdk-x32/*.o /wk/gdk-x32/be/*.o /wk/hdesktop-x32/*.o)
nm -u $O/*.o | grep " U " | awk '{print $2}' | sort -u > $O/und_all.txt
nm --defined-only $O/*.o $GO | awk 'NF==3{print $3}' | sort -u > $O/def_all.txt
comm -23 $O/und_all.txt $O/def_all.txt | grep -E "^(_?hildon_|_?hd_|gtk_|gdk_|atk_|X[A-Z]|gconf_|osso_|dl)" > $O/undefined.txt || true
echo "undefined hildon / hd / gtk / gdk / X / osso symbols: $(wc -l < $O/undefined.txt)"; head -40 $O/undefined.txt | tr '\n' ' '; echo
