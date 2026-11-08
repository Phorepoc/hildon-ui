#!/usr/bin/env bash
# build_sapwood_x32.sh -- IN phorepoc-x86 (volume at /wk, this repo at /h, siblings at /glib /gtk /dbus): the sapwood GTK theme engine (github.com/maemo-leste/sapwood, LGPL-2.1, pinned in pins.sh,
# cloned into dl/sapwood) as libsapwood.a -> sapwood.dll. The engine of the Hildon (Nokia Nseries / Fremantle) themes. Upstream talks to a separate sapwood-server for X pixmaps: that one file
# (engine/sapwood-pixmap.c + its header) is replaced by overlay/sapwood/ (in-process tiles drawn with cairo); theme-pixbuf.c and sapwood-style.c get small patches (sapwood_patch.py). The engine
# is loaded by GTK through GModule like any module (sapwood.dll, symbols by name through resource/gmodule/sapwood.sym).
# Out: /wk/sapwood-x32/*.o, /wk/out/libsapwood.a
set -eu
. /glib/recipes/pins.sh; . /gtk/recipes/pins.sh; . /h/recipes/pins.sh
CC=/wk/lsc/bin/pp-gcc
S=/h/dl/sapwood/engine; O=/wk/sapwood-x32; mkdir -p $O /wk/out; G=/wk/gtk+-$GTK_VER; SH=/h/os/src/phehildon
[ -d $S ] || { echo "no $S: git clone github.com/maemo-leste/sapwood into dl/ and check out SAPWOOD from pins.sh"; exit 1; }
rm -rf $O/src; mkdir -p $O/src
cp $S/*.c $S/*.h $O/src/
cp /h/overlay/sapwood/sapwood-pixmap.c /h/overlay/sapwood/sapwood-pixmap.h $O/src/
rm -f $O/src/sapwood-client.c $O/src/sapwood-client.h $O/src/sapwood-pixmap-priv.h
echo '#define VERSION "3.1.1"' > $O/config.h
python3 /h/recipes/sapwood_patch.py $O/src
X32="-I/wk/x32 -I/wk/x32/glib -I/wk/glib-2.56.4 -I/wk/glib-2.56.4/glib -I/wk/glib-2.56.4/gobject -I/wk/glib-2.56.4/gmodule -I/wk/glib-2.56.4/gio -I/wk/ffi-x32/include"
FF="-O2 -g0 -w -fno-pic -I/glib/overlay -include /glib/overlay/phe_glib_compat.h -include /wk/gdk-x32/gtkcompat.h -include $SH/shims/phe_locale.h -DHAVE_CONFIG_H -I$O -I$O/src \
 -I$G -I$G/gdk -I/wk/gdk-x32 -I/wk/atk-$ATK_VER -I/wk/gtk-x32 -I/wk/gtk-x32/inc $X32 -I/wk/cairo-1.16.0/src -I/wk/cairo-x32/src -I/wk/pixman-0.42.2/pixman -I/wk/pango-1.40.14 -I/wk/pango-x32 \
 -I/wk/harfbuzz-$HARFBUZZ_VER/src -I/wk/fontconfig-2.14.2 -I/wk/freetype-2.13.3/include -I/wk/gdk-pixbuf-2.36.12 -I/wk/pixbuf-x32"
rm -f $O/*.o $O/errs.*
for f in sapwood-main sapwood-rc-style sapwood-style theme-pixbuf sapwood-pixmap; do $CC $FF -c $O/src/$f.c -o $O/$f.o 2> $O/errs.$f || echo "FAIL $f"; done
cat $O/errs.* > $O/errors.txt 2>/dev/null || true
grep -a "error" $O/errors.txt | cut -c1-200 | head -20
rm -f /wk/out/libsapwood.a; ar rcs /wk/out/libsapwood.a $O/*.o
echo "libsapwood.a: $(ar t /wk/out/libsapwood.a | wc -l) members"
nm -u $O/*.o | grep " U " | awk '{print $2}' | sort -u | grep -E "^(gdk_x11|X[A-Z])" | head || true
