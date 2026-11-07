#!/usr/bin/env bash
# build_gdk_x32.sh -- IN phorepoc-x86 (volume at /wk, repo :ro at /repo, this repo at /h): the GDK 2.24.33 CORE (gdk/*.c outside any backend) for x32 over Open C, from the pinned
# gtk+ tarball. No configure: gdk/ ships its generated sources (gdkenumtypes.c, gdkmarshalers.c, gdkaliasdef.c); the only missing generated file is gdkconfig.h, written here
# for the Symbian backend (GDK_WINDOWING_SYMBIAN). The symbol aliasing of gdkalias.h is off (DISABLE_VISIBILITY). The backend (gdk/symbian, hildon-symbian/os/src/gdksymbian)
# is compiled by build_gdk_backend_x32.sh. Out: /wk/gdk-x32/*.o and the list of undefined symbols the backend must provide, /wk/gdk-x32/undefined.txt
set -eu
. /h/recipes/pins.sh
CC=/wk/lsc/bin/pp-gcc
cd /wk; [ -d gtk+-$GTK_VER/gdk ] || tar xf /h/dl/gtk+-$GTK_VER.tar.xz gtk+-$GTK_VER/gdk gtk+-$GTK_VER/COPYING gtk+-$GTK_VER/gtk/gtkintl.h 2>/dev/null || tar xf /h/dl/gtk+-$GTK_VER.tar.xz gtk+-$GTK_VER/gdk
G=/wk/gtk+-$GTK_VER; mkdir -p /wk/gdk-x32
cat > $G/gdk/gdkconfig.h <<'EOC'
/* gdkconfig.h -- written by build_gdk_x32.sh for the Symbian backend */
#ifndef GDKCONFIG_H
#define GDKCONFIG_H
#if !defined(GDK_COMPILATION) && !defined(__GDK_H_INSIDE__) && !defined(GTK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif
#include <glib.h>
G_BEGIN_DECLS
#define GDK_WINDOWING_SYMBIAN
#define GDK_HAVE_WCHAR_H 1
#define GDK_HAVE_WCTYPE_H 1
G_END_DECLS
#endif
EOC
cat > /wk/gdk-x32/config.h <<'EOC'
#define GETTEXT_PACKAGE "gtk20"
#define GTK_LOCALEDIR "z:/resource/locale"
#define GDK_PIXBUF_VERSION "2.36.12"
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_UNISTD_H 1
#define HAVE_FCNTL_H 1
#define HAVE_SYS_TIME_H 1
EOC
printf '#ifndef GSEAL
#define GSEAL(ident) ident   /* GLib dropped GSEAL in 2.4x; GTK 2.24 still uses it in its public structs */
#endif
' > /wk/gdk-x32/gtkcompat.h
SRC="gdk gdkapplaunchcontext gdkcairo gdkcolor gdkcursor gdkdisplay gdkdisplaymanager gdkdnd gdkdraw gdkevents gdkfont gdkgc gdkglobals gdkimage gdkkeys gdkkeyuni gdkoffscreenwindow gdkpango gdkpixbuf-drawable gdkpixbuf-render gdkpixmap gdkpolyreg-generic gdkrectangle gdkregion-generic gdkrgb gdkscreen gdkselection gdkvisual gdkwindow gdkwindowimpl gdkenumtypes gdkmarshalers"
X32="-I/wk/x32 -I/wk/x32/glib -I/wk/glib-2.56.4 -I/wk/glib-2.56.4/glib -I/wk/glib-2.56.4/gobject -I/wk/glib-2.56.4/gmodule -I/wk/glib-2.56.4/gio -I/wk/ffi-x32/include"
FF="-O2 -g0 -w -fno-pic -I/h/overlay -include /h/overlay/phe_glib_compat.h -include /wk/gdk-x32/gtkcompat.h -DHAVE_CONFIG_H -DGDK_COMPILATION -DG_LOG_DOMAIN=\"Gdk\" -DDISABLE_VISIBILITY -DGDK_DISABLE_DEPRECATION_WARNINGS \
 -I/wk/gdk-x32 -I$G -I$G/gdk $X32 -I/wk/cairo-1.16.0/src -I/wk/cairo-x32/src -I/wk/pixman-0.42.2/pixman -I/wk/pango-1.40.14 -I/wk/pango-x32 -I/wk/harfbuzz-$HARFBUZZ_VER/src -I/wk/fontconfig-2.14.2 -I/wk/freetype-2.13.3/include -I/wk/gdk-pixbuf-2.36.12 -I/wk/pixbuf-x32"
rm -f /wk/gdk-x32/*.o /wk/gdk-x32/errors.txt; fails=0
for f in $SRC; do $CC $FF -c $G/gdk/$f.c -o /wk/gdk-x32/$f.o 2>> /wk/gdk-x32/errors.txt || { echo "FAIL $f"; fails=$((fails+1)); }; done
echo "gdk core: $(ls /wk/gdk-x32/*.o 2>/dev/null | wc -l) objects, $fails failed"
grep -a "error" /wk/gdk-x32/errors.txt | sed 's|.*/gdk/||' | cut -c1-170 | head -20
[ $fails -eq 0 ] || exit 0
nm -u /wk/gdk-x32/*.o | grep " U " | awk '{print $2}' | sort -u > /wk/gdk-x32/und_all.txt
nm --defined-only /wk/gdk-x32/*.o | awk 'NF==3{print $3}' | sort -u > /wk/gdk-x32/def_all.txt
comm -23 /wk/gdk-x32/und_all.txt /wk/gdk-x32/def_all.txt | grep -E "^(_?gdk_|gdk_)" > /wk/gdk-x32/undefined.txt
echo "undefined gdk_* symbols the backend must provide: $(wc -l < /wk/gdk-x32/undefined.txt)"
