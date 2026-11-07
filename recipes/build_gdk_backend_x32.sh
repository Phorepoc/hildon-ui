#!/usr/bin/env bash
# build_gdk_backend_x32.sh -- IN phorepoc-x86 (as build_gdk_x32.sh, which must have run): compile the Symbian GDK backend (hildon-symbian/os/src/gdksymbian/*.c) against the
# GDK 2.24.33 core, check that no symbol is defined twice (core vs backend), and print what the core + backend still leave undefined. Out: /wk/gdk-x32/be/*.o
set -eu
. /h/recipes/pins.sh
CC=/wk/lsc/bin/pp-gcc; G=/wk/gtk+-$GTK_VER; B=/wk/gdk-x32/be; mkdir -p $B; rm -f $B/*.o $B/errors.txt
X32="-I/wk/x32 -I/wk/x32/glib -I/wk/glib-2.56.4 -I/wk/glib-2.56.4/glib -I/wk/glib-2.56.4/gobject -I/wk/glib-2.56.4/gmodule -I/wk/glib-2.56.4/gio -I/wk/ffi-x32/include"
FF="-O2 -g0 -w -fno-pic -I/h/overlay -include /h/overlay/phe_glib_compat.h -include /wk/gdk-x32/gtkcompat.h -DHAVE_CONFIG_H -DGDK_COMPILATION -DG_LOG_DOMAIN=\"Gdk\" -DDISABLE_VISIBILITY -DGDK_DISABLE_DEPRECATION_WARNINGS \
 -I/h/os/src/gdksymbian -I/wk/gdk-x32 -I$G -I$G/gdk $X32 -I/wk/cairo-1.16.0/src -I/wk/cairo-x32/src -I/wk/pixman-0.42.2/pixman -I/wk/pango-1.40.14 -I/wk/pango-x32 -I/wk/harfbuzz-$HARFBUZZ_VER/src -I/wk/fontconfig-2.14.2 -I/wk/freetype-2.13.3/include -I/wk/gdk-pixbuf-2.36.12 -I/wk/pixbuf-x32"
fails=0
for f in /h/os/src/gdksymbian/*.c; do n=$(basename $f .c); $CC $FF -c $f -o $B/$n.o 2>> $B/errors.txt || { echo "FAIL $n"; fails=$((fails+1)); }; done
echo "gdk backend: $(ls $B/*.o 2>/dev/null | wc -l) objects, $fails failed"
grep -a "error" $B/errors.txt | sed 's|.*/gdksymbian/||; s|.*/gdk/||' | cut -c1-190 | head -25
[ $fails -eq 0 ] || exit 0
# duplicates between core and backend
nm --defined-only -g /wk/gdk-x32/*.o | awk 'NF==3 && $2 ~ /[TDBR]/{print $3}' | sort > /tmp/core_def.txt
nm --defined-only -g $B/*.o | awk 'NF==3 && $2 ~ /[TDBR]/{print $3}' | sort > /tmp/be_def.txt
echo "defined in both core and backend: $(comm -12 /tmp/core_def.txt /tmp/be_def.txt | tr '\n' ' ')"
nm -u /wk/gdk-x32/*.o $B/*.o | grep " U " | awk '{print $2}' | sort -u > /tmp/u_all.txt
sort -u /tmp/core_def.txt /tmp/be_def.txt > /tmp/d_all.txt
comm -23 /tmp/u_all.txt /tmp/d_all.txt | grep -E "^_?gdk_" | grep -v "gdk_pixbuf" > /wk/gdk-x32/undefined-now.txt || true
echo "gdk_* still undefined (core + backend): $(wc -l < /wk/gdk-x32/undefined-now.txt)"
