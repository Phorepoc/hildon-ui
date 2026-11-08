#!/usr/bin/env bash
# build_hildonhome_x32.sh -- IN phorepoc-x86 (volume at /wk, repo :ro at /repo, this repo at /h, siblings at /glib /gtk /dbus): the Home process of the Phorepoc Hildon desktop.
#  libphehome.a = the REAL hd-applet-manager.c and hd-widgets.c of hildon-home (github.com/maemo-leste/hildon-home at the commit pinned in pins.sh, LGPL-2.1, cloned into dl/hildon-home) + our main (os/src/phehome/phehome_main.c)
#  libphehomeexample.a = libhildondesktop's example home applet (examples/home/example-label-home-applet.c), the plug-in the Home loads
# No configure: config.h is written here. The rest of hildon-home (hildon-home.c, notifications, backgrounds, shortcuts ...) is NOT built: see docs hildon-port.md 7.23.
# Out: /wk/hhome-x32/*.o, /wk/out/libphehome.a, /wk/out/libphehomeexample.a
set -eu
. /glib/recipes/pins.sh; . /gtk/recipes/pins.sh; . /dbus/recipes/pins.sh; . /h/recipes/pins.sh
CC=/wk/lsc/bin/pp-gcc
H=/h/dl/hildon-home/src; HD=/h/dl/libhildondesktop; O=/wk/hhome-x32; mkdir -p $O /wk/out; SH=/h/os/src/phehildon; G=/wk/gtk+-$GTK_VER
[ -d $H ] || { echo "no $H: git clone github.com/maemo-leste/hildon-home into dl/ and check out \$HILDON_HOME"; exit 1; }
cat > $O/config.h <<EOC
#define GETTEXT_PACKAGE "maemo-af-desktop"
#define VERSION "0"
EOC
X32="-I/wk/x32 -I/wk/x32/glib -I/wk/glib-2.56.4 -I/wk/glib-2.56.4/glib -I/wk/glib-2.56.4/gobject -I/wk/glib-2.56.4/gmodule -I/wk/glib-2.56.4/gio -I/wk/ffi-x32/include"
FF="-O2 -g0 -w -fno-pic -I/glib/overlay -include /glib/overlay/phe_glib_compat.h -include /wk/gdk-x32/gtkcompat.h -include $SH/shims/phe_locale.h -include /dbus/os/src/phedbus/phedbus_compat.h -DHAVE_CONFIG_H \
 -DG_LOG_DOMAIN=\"phehome\" -DLOCALEDIR=\"z:/resource/locale\" -DDATADIR=\"z:/resource\" -DHD_DESKTOP_ENTRY_PATH=\"z:/resource/hildondesktop/entries\" -DHD_DESKTOP_CONFIG_PATH=\"z:/resource/hildondesktop\" \
 -DHD_DESKTOP_MODULE_PATH=\"z:/sys/bin\" -DHD_DESKTOP_MENU_PATH=\"z:/resource/hildondesktop/menu\" -DHD_DESKTOP_BACKGROUNDS_PATH=\"z:/resource/hildondesktop/backgrounds\" \
 -DHD_PLUGIN_LOADER_MODULES_PATH=\"z:/sys/bin\" -DHD_UI_POLICY_MODULES_PATH=\"z:/sys/bin\" -DDBUS_API_SUBJECT_TO_CHANGE \
 -I$O -I$H -I$HD -I/wk/hdesktop-x32/inc -I$SH/shims -I/wk/hildon-x32 -I/wk/hildon-x32/inc -I/wk/dbusglib-x32/inc -I/wk/dbus-glib-$DBUSGLIB_VER -I/wk/dbus-glib-$DBUSGLIB_VER/dbus \
 -I$G -I$G/gdk -I/wk/gdk-x32 -I/wk/atk-$ATK_VER -I/wk/gtk-x32 -I/wk/gtk-x32/inc $X32 -I/wk/cairo-1.16.0/src -I/wk/cairo-x32/src -I/wk/pixman-0.42.2/pixman -I/wk/pango-1.40.14 -I/wk/pango-x32 \
 -I/wk/harfbuzz-$HARFBUZZ_VER/src -I/wk/fontconfig-2.14.2 -I/wk/freetype-2.13.3/include -I/wk/gdk-pixbuf-2.36.12 -I/wk/pixbuf-x32"
rm -f $O/*.o $O/errs.*
one() { $CC $FF "${@:3}" -c $1 -o $O/$2.o 2> $O/errs.$2 || echo "FAIL $2"; }
# the glue of the Home D-Bus interface (hildon-home's own XML) from the REAL dbus-binding-tool, built natively (dbus-symbian/recipes/gen_glue.sh)
bash /dbus/recipes/gen_glue.sh hd_hildon_home_dbus glib-server $H/hd-hildon-home-dbus.xml $O/glue/hd-hildon-home-dbus-glue.h | tail -1
one $H/hd-applet-manager.c hd-applet-manager
one $H/hd-widgets.c hd-widgets
one /h/os/src/phehome/phehome_main.c phehome_main -I$H
one /h/os/src/phehome/phehome_dbus.c phehome_dbus -I$H -I$O/glue
one $HD/examples/home/example-label-home-applet.c example-label-home-applet -I$HD/examples/home -I/wk/hdesktop-x32/inc/libhildondesktop
cat $O/errs.* > $O/errors.txt 2>/dev/null || true; rm -f $O/errs.*
grep -a "error" $O/errors.txt | cut -c1-200 | head -30
rm -f /wk/out/libphehome.a /wk/out/libphehomeexample.a
ar rcs /wk/out/libphehome.a $O/hd-applet-manager.o $O/hd-widgets.o $O/phehome_main.o $O/phehome_dbus.o
ar rcs /wk/out/libphehomeexample.a $O/example-label-home-applet.o
echo "libphehome.a: $(ar t /wk/out/libphehome.a | wc -l) members, libphehomeexample.a: $(ar t /wk/out/libphehomeexample.a | wc -l)"
GO=$(ls /wk/hildon-x32/*.o /wk/gtk-x32/*.o /wk/gdk-x32/*.o /wk/gdk-x32/be/*.o /wk/hdesktop-x32/*.o)
nm -u $O/*.o | grep " U " | awk '{print $2}' | sort -u > $O/und_all.txt
nm --defined-only $O/*.o $GO | awk 'NF==3{print $3}' | sort -u > $O/def_all.txt
comm -23 $O/und_all.txt $O/def_all.txt | grep -E "^(_?hildon_|_?hd_|gtk_|gdk_|atk_|X[A-Z]|gconf_)" > $O/undefined.txt || true
echo "undefined hildon / hd / gtk / gdk / X symbols: $(wc -l < $O/undefined.txt)"; head -40 $O/undefined.txt | tr '
' ' '
