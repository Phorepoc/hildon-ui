#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: BSD-2-Clause
# package_x32.sh -- IN phorepoc-x86 (/wk, repo:/repo:ro, this repo:/h, glib-symbian:/glib, gtk-symbian:/gtk, dbus-symbian:/dbus, output dir :/o): libhildon + libhildondesktop archives and their demo / test applications
set -u
CC=/wk/lsc/bin/pp-gcc; X=/wk/x32; S=/wk/glib-2.56.4; O=/o; mkdir -p $O /wk/out/t
INC="-I$X -I$X/glib -I$S -I$S/glib -I$S/gobject -I$S/gmodule -I/wk/ffi-x32 -I/glib/overlay -include /glib/overlay/phe_glib_compat.h"
CF="-O2 -g0 -w -fno-pic -DHAVE_CONFIG_H -DMAEMO_GTK -DGLIB_STATIC_COMPILATION -DGOBJECT_STATIC_COMPILATION"
# libhildon (build_hildon_x32.sh) as libphehildon.a, and its demo (public libhildon API) as libphehildonwin.a
if [ -f /wk/out/libphehildon.a ]; then
  cp /wk/out/libphehildon.a $O/libphehildon.a; echo "libphehildon.a: $(ar t $O/libphehildon.a | wc -l) members"
  GT=/wk/gtk+-2.24.33; HI="-I/h/os/src/phehildonwin -I/wk/hildon-x32 -I/wk/hildon-x32/inc -I/h/dl/libhildon -I/h/os/src/phehildon/shims -I/wk/gtk-x32 -I/wk/gtk-x32/inc -I$GT -I/wk/atk-2.38.0 -I/wk/gdk-x32 -I$GT/gdk -I/wk/pango-1.40.14 -I/wk/pango-x32 -I/wk/fontconfig-2.14.2 -I/wk/freetype-2.13.3/include -I/wk/cairo-1.16.0/src -I/wk/cairo-x32/src -I/wk/harfbuzz-15.1.0/src -I/wk/gdk-pixbuf-2.36.12 -I/wk/pixbuf-x32 -I/wk/glib-2.56.4/gio"
  mkdir -p /wk/out/hi; rm -f /wk/out/hi/*.o $O/libphehildonwin.a
  $CC $CF $INC $HI -include /wk/gdk-x32/gtkcompat.h -c /h/os/src/phehildonwin/phehildonwin_demo.c -o /wk/out/hi/phehildonwin_demo.o || echo "HILDON DEMO COMPILE FAIL"
  ar rcs $O/libphehildonwin.a /wk/out/hi/*.o; cp /h/os/src/phehildonwin/fonts.conf $O/testdata/phehildonwin_fonts.conf; cp /h/os/src/phehildonwin/phehildon.rc $O/testdata/phehildon.rc
  python3 - <<'PYEOF'
import wave, struct, math
w = wave.open("/o/testdata/phehildon_beep.wav", "wb"); w.setnchannels(1); w.setsampwidth(2); w.setframerate(22050)
w.writeframes(b"".join(struct.pack("<h", int(9000 * math.sin(2 * math.pi * 880 * i / 22050) * min(1.0, (11025 - i) / 2000.0 if i > 9000 else 1.0))) for i in range(11025))); w.close()
PYEOF
  echo "libphehildonwin.a built"
fi
if [ -f /wk/out/libphehildondesktop.a ]; then
  cp /wk/out/libphehildondesktop.a $O/libphehildondesktop.a; echo "libphehildondesktop.a: $(ar t $O/libphehildondesktop.a | wc -l) members"
  GT=/wk/gtk+-2.24.33; HD="-I/h/os/src/phehildondesktoptest -I/h/os/src/phehildon/shims -I/wk/hildon-x32 -I/wk/hildon-x32/inc -I/h/dl/libhildon -I/h/dl/libhildondesktop -I/wk/hildondesktop-x32/inc -I/wk/hdesktop-x32/inc -I/wk/dbusglib-x32/inc -I/wk/dbus-glib-0.114 -I/wk/dbus-glib-0.114/dbus -I/wk/gtk-x32 -I/wk/gtk-x32/inc -I$GT -I/wk/atk-2.38.0 -I/wk/gdk-x32 -I$GT/gdk -I/wk/pango-1.40.14 -I/wk/pango-x32 -I/wk/fontconfig-2.14.2 -I/wk/freetype-2.13.3/include -I/wk/cairo-1.16.0/src -I/wk/cairo-x32/src -I/wk/harfbuzz-15.1.0/src -I/wk/gdk-pixbuf-2.36.12 -I/wk/pixbuf-x32 -I/wk/glib-2.56.4/gio -DDBUS_API_SUBJECT_TO_CHANGE"
  mkdir -p /wk/out/hdt; rm -f /wk/out/hdt/*.o $O/libphehildondesktoptest.a
  $CC $CF $INC $HD -include /wk/gdk-x32/gtkcompat.h -c /h/os/src/phehildondesktoptest/phehildondesktoptest_main.c -o /wk/out/hdt/phehildondesktoptest_main.o || echo "HILDONDESKTOP TEST COMPILE FAIL"
  ar rcs $O/libphehildondesktoptest.a /wk/out/hdt/*.o; cp /h/os/src/phehildondesktoptest/fonts.conf $O/testdata/phehildondesktoptest_fonts.conf; echo "libphehildondesktoptest.a built"
fi
# the Home process (build_hildonhome_x32.sh): archives, and the symbol tables (GModule, glib-symbian) of this repository's DLLs
[ -f /wk/out/libphehome.a ] && cp /wk/out/libphehome.a $O/libphehome.a && cp /wk/out/libphehomeexample.a $O/libphehomeexample.a && echo "libphehome.a copied"
[ -f /wk/out/libsapwood.a ] && cp /wk/out/libsapwood.a $O/libsapwood.a && echo "libsapwood.a copied"
[ -f /wk/out/libpixmap.a ] && cp /wk/out/libpixmap.a $O/libpixmap.a && echo "libpixmap.a copied"
mkdir -p $O/testdata $O/gmodule
cp $O/testdata/phehildonwin_fonts.conf $O/testdata/phehome_fonts.conf 2>/dev/null
for d in /h/os/src/*/*.def /gtk/os/src/pheengpixmap/pixmap.def; do n=$(basename $d .def); python3 /glib/recipes/gen_symtab.py $d $O/gmodule/$n.sym > /dev/null; done; echo "symbol tables: $(ls $O/gmodule | wc -l)"
true

