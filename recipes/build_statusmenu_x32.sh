#!/usr/bin/env bash
# build_statusmenu_x32.sh -- IN phorepoc-x86 (volume at /wk, repo :ro at /repo, this repo at /h, siblings at /glib /gtk /dbus): hildon-status-menu (github.com/maemo-leste/hildon-status-menu at the commit pinned
# in pins.sh, LGPL-2.1, cloned into dl/hildon-status-menu) UNMODIFIED, over libhildon / libhildondesktop. It is the status area (clock and the permanent items along the top of the screen) and the status
# menu (the pull-down of plug-ins). What it talks to is provided, not stubbed: GConf (shims/gconf, the in-process store), X properties (shims, over GDK window properties) and MCE's display-state
# D-Bus interface (the mce/dbus-names.h and mode-names.h constants come from maemo-leste/mce, dl/mce; the service itself is a hildon-bridges job).
# Out: /wk/statusmenu-x32/*.o, /wk/out/libphestatusmenu.a (its main is renamed phe_status_menu_main: os/src/phestatusmenu is the entry point)
set -eu
. /glib/recipes/pins.sh; . /gtk/recipes/pins.sh; . /dbus/recipes/pins.sh; . /h/recipes/pins.sh
CC=/wk/lsc/bin/pp-gcc
H=/h/dl/hildon-status-menu/src; HD=/h/dl/libhildondesktop; O=/wk/statusmenu-x32; mkdir -p $O /wk/out; SH=/h/os/src/phehildon; G=/wk/gtk+-$GTK_VER; MCE=/h/dl/mce/src/include
[ -d $H ] || { echo "no $H: git clone github.com/maemo-leste/hildon-status-menu into dl/ (STATUS_MENU in pins.sh)"; exit 1; }
cat > $O/config.h <<EOC
#define GETTEXT_PACKAGE "maemo-af-desktop"
#define VERSION "0"
#define HD_DESKTOP_ENTRY_PATH "z:/resource/hildondesktop/status-menu"
EOC
X32="-I/wk/x32 -I/wk/x32/glib -I/wk/glib-2.56.4 -I/wk/glib-2.56.4/glib -I/wk/glib-2.56.4/gobject -I/wk/glib-2.56.4/gmodule -I/wk/glib-2.56.4/gio -I/wk/ffi-x32/include"
FF="-O2 -g0 -w -fno-pic -I/glib/overlay -include /glib/overlay/phe_glib_compat.h -include /glib/overlay/phe_respath.h -include /wk/gdk-x32/gtkcompat.h -include $SH/shims/phe_locale.h -include /dbus/os/src/phedbus/phedbus_compat.h -DHAVE_CONFIG_H -DMAEMO_GTK \
 -DG_LOG_DOMAIN=\"hildon-status-menu\" -DLOCALEDIR=\"z:/resource/locale\" -DHD_DESKTOP_CONFIG_PATH=phe_res(\"resource/hildondesktop\") -DDBUS_API_SUBJECT_TO_CHANGE \
 -I$O -I$H -I$HD -I/wk/hdesktop-x32/inc -I$SH/shims -I$MCE -I/wk/hildon-x32 -I/wk/hildon-x32/inc -I/wk/dbusglib-x32/inc -I/wk/dbus-glib-$DBUSGLIB_VER -I/wk/dbus-glib-$DBUSGLIB_VER/dbus \
 -I$G -I$G/gdk -I/wk/gdk-x32 -I/wk/atk-$ATK_VER -I/wk/gtk-x32 -I/wk/gtk-x32/inc $X32 -I/wk/cairo-1.16.0/src -I/wk/cairo-x32/src -I/wk/pixman-0.42.2/pixman -I/wk/pango-1.40.14 -I/wk/pango-x32 \
 -I/wk/harfbuzz-$HARFBUZZ_VER/src -I/wk/fontconfig-2.14.2 -I/wk/freetype-2.13.3/include -I/wk/gdk-pixbuf-2.36.12 -I/wk/pixbuf-x32"
rm -f $O/*.o $O/errs.*
one() { $CC $FF "${@:3}" -c $1 -o $O/$2.o 2> $O/errs.$2 || echo "FAIL $2"; }
one $H/hildon-status-menu.c hildon-status-menu -Dmain=phe_status_menu_main
for f in hd-status-area hd-status-area-box hd-status-menu hd-status-menu-box hd-desktop hd-display; do one $H/$f.c $f; done
cat $O/errs.* > $O/errors.txt 2>/dev/null || true; rm -f $O/errs.*
grep -a "error" $O/errors.txt | cut -c1-220 | head -30
rm -f /wk/out/libphestatusmenu.a; ar rcs /wk/out/libphestatusmenu.a $O/*.o
echo "libphestatusmenu.a: $(ar t /wk/out/libphestatusmenu.a | wc -l) members"
GO=$(ls /wk/hildon-x32/*.o /wk/gtk-x32/*.o /wk/gdk-x32/*.o /wk/gdk-x32/be/*.o /wk/hdesktop-x32/*.o)
nm -u $O/*.o | grep " U " | awk '{print $2}' | sort -u > $O/und_all.txt
nm --defined-only $O/*.o $GO | awk 'NF==3{print $3}' | sort -u > $O/def_all.txt
comm -23 $O/und_all.txt $O/def_all.txt | grep -E "^(_?hildon_|_?hd_|gtk_|gdk_|atk_|X[A-Z]|gconf_|dbus_)" > $O/undefined.txt || true
echo "undefined hildon / hd / gtk / gdk / X / gconf / dbus symbols: $(wc -l < $O/undefined.txt)"; head -30 $O/undefined.txt | tr '\n' ' '
