#!/usr/bin/env bash
# build_hildondesktop_x32.sh -- IN phorepoc-x86 (volume at /wk, repo :ro at /repo, this repo at /h): libhildondesktop 2.1.44 (the home / status area plug-in framework: HDPluginLoader,
# HDPluginManager, HDHomePluginItem, HDStatusMenuItem, HDShortcuts ...; LGPL 2.1, github.com/maemo-leste/libhildondesktop at the commit pinned in pins.sh, cloned into dl/libhildondesktop) for
# x32 over Open C, on libhildon, GTK, libdbus + dbus-glib. No configure: the 17 sources of libhildondesktop/Makefile.am, the paths the Makefile passes as -D are those of this system's resource
# directory (z:/resource/hildondesktop/...). The platform libhildon / libhildondesktop expect is provided by os/src/phehildon (Xlib, GConf, libiphb, canberra shims).
# Out: /wk/hdesktop-x32/*.o, /wk/out/libphehildondesktop.a, /wk/hdesktop-x32/undefined.txt
set -eu
. /glib/recipes/pins.sh; . /gtk/recipes/pins.sh; . /dbus/recipes/pins.sh; . /h/recipes/pins.sh
CC=/wk/lsc/bin/pp-gcc
H=/h/dl/libhildondesktop; O=/wk/hdesktop-x32; mkdir -p $O /wk/out; S=$H/libhildondesktop; SH=/h/os/src/phehildon; G=/wk/gtk+-$GTK_VER
[ -d $S ] || { echo "no $S: git clone github.com/maemo-leste/libhildondesktop into dl/ and check out \$LIBHILDONDESKTOP"; exit 1; }
cat > $O/config.h <<EOC
#define HAVE_LIBHILDON 1
#define GETTEXT_PACKAGE "libhildondesktop"
#define VERSION "2.1.44"
EOC
SRC=$(sed -n '/^libhildondesktop_@API_VERSION_MAJOR@_la_SOURCES/,/^$/p' $S/Makefile.am | grep -o "[a-z0-9-]*\.c" | sed 's/\.c$//' | tr '\n' ' ')
echo "hildondesktop: $(echo $SRC | wc -w) sources"
X32="-I/wk/x32 -I/wk/x32/glib -I/wk/glib-2.56.4 -I/wk/glib-2.56.4/glib -I/wk/glib-2.56.4/gobject -I/wk/glib-2.56.4/gmodule -I/wk/glib-2.56.4/gio -I/wk/ffi-x32/include"
FF="-O2 -g0 -w -fno-pic -I/glib/overlay -include /glib/overlay/phe_glib_compat.h -include /glib/overlay/phe_respath.h -include /wk/gdk-x32/gtkcompat.h -include $SH/shims/phe_locale.h -include /dbus/os/src/phedbus/phedbus_compat.h -DHAVE_CONFIG_H -DMAEMO_GTK \
 -DG_LOG_DOMAIN=\"libhildondesktop\" -DLOCALEDIR=\"z:/resource/locale\" -DDATADIR=\"z:/resource\" -DHD_DESKTOP_ENTRY_PATH=\"z:/resource/hildondesktop/entries\" -DHD_DESKTOP_CONFIG_PATH=phe_res(\"resource/hildondesktop\") \
 -DHD_DESKTOP_MODULE_PATH=\"z:/sys/bin\" -DHD_DESKTOP_MENU_PATH=\"z:/resource/hildondesktop/menu\" -DHD_DESKTOP_BACKGROUNDS_PATH=\"z:/resource/hildondesktop/backgrounds\" \
 -DHD_PLUGIN_LOADER_MODULES_PATH=\"z:/sys/bin\" -DHD_UI_POLICY_MODULES_PATH=\"z:/sys/bin\" -DDBUS_API_SUBJECT_TO_CHANGE \
 -I$O -I$H -I$S -I$SH/shims -I/wk/hildon-x32 -I/wk/hildon-x32/inc -I/wk/dbusglib-x32/inc -I/wk/dbus-glib-$DBUSGLIB_VER -I/wk/dbus-glib-$DBUSGLIB_VER/dbus \
 -I$G -I$G/gdk -I/wk/gdk-x32 -I/wk/atk-$ATK_VER -I/wk/gtk-x32 -I/wk/gtk-x32/inc $X32 -I/wk/cairo-1.16.0/src -I/wk/cairo-x32/src -I/wk/pixman-0.42.2/pixman -I/wk/pango-1.40.14 -I/wk/pango-x32 \
 -I/wk/harfbuzz-$HARFBUZZ_VER/src -I/wk/fontconfig-2.14.2 -I/wk/freetype-2.13.3/include -I/wk/gdk-pixbuf-2.36.12 -I/wk/pixbuf-x32"
# <libhildondesktop/...> style includes: a symlink tree
mkdir -p $O/inc; ln -sfn $S $O/inc/libhildondesktop
rm -f $O/*.o $O/errs.*; export CC FF O S SH
one() { f=$1; $CC $FF -I$O/inc -c $S/$f.c -o $O/$f.o 2> $O/errs.$f || echo "FAIL $f"; }
export -f one
for x in phehildon_iphb; do $CC $FF -I$O/inc -c $SH/$x.c -o $O/$x.o 2> $O/errs.$x || echo "FAIL $x"; done
fails=$(echo $SRC | tr ' ' '\n' | xargs -P "${JOBS:-8}" -I{} bash -c 'one {}' | tee $O/failed.txt | wc -l)
cat $O/errs.* > $O/errors.txt 2>/dev/null || true; rm -f $O/errs.*
echo "hildondesktop: $(ls $O/*.o 2>/dev/null | wc -l) objects, $fails failed"; head -30 $O/failed.txt | tr '\n' ' '; echo
grep -a "error" $O/errors.txt | sed "s|$S/||" | cut -c1-200 | head -${ERRN:-30}
rm -f /wk/out/libphehildondesktop.a; ar rcs /wk/out/libphehildondesktop.a $O/*.o; echo "libphehildondesktop.a: $(ar t /wk/out/libphehildondesktop.a | wc -l) members"
GO=$(ls /wk/hildon-x32/*.o /wk/gtk-x32/*.o /wk/gdk-x32/*.o /wk/gdk-x32/be/*.o /wk/dbus-x32/*.o /wk/dbusglib-x32/*.o)
nm -u $O/*.o | grep " U " | awk '{print $2}' | sort -u > $O/und_all.txt
nm --defined-only $O/*.o $GO | awk 'NF==3{print $3}' | sort -u > $O/def_all.txt
comm -23 $O/und_all.txt $O/def_all.txt | grep -E "^(_?hildon_|_?hd_|gtk_|gdk_|atk_|X[A-Z]|ca_|gconf_|dbus_|iphb_|_dbus)" > $O/undefined.txt || true
echo "undefined hildon / hd / gtk / gdk / X / gconf / dbus / iphb symbols: $(wc -l < $O/undefined.txt)"; head -60 $O/undefined.txt | tr '\n' ' '
