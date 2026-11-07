#!/usr/bin/env bash
# package_x32.sh -- IN phorepoc-x86 (/wk, repo:/repo:ro, this repo:/h, output dir :/o): archive the GLib objects + libffi + the test
# into /o/{libpheglib,libphegobject,libphegmodule,libphegthread,libpheffi,libpheglibtest}.a
set -u
CC=/wk/lsc/bin/pp-gcc; X=/wk/x32; S=/wk/glib-2.56.4; O=/o; mkdir -p $O /wk/out/t
INC="-I$X -I$X/glib -I$S -I$S/glib -I$S/gobject -I$S/gmodule -I/wk/ffi-x32 -I/h/overlay -include /h/overlay/phe_glib_compat.h"
CF="-O2 -g0 -w -fno-pic -DHAVE_CONFIG_H -DGLIB_STATIC_COMPILATION -DGOBJECT_STATIC_COMPILATION"
$CC $CF $INC -c /h/overlay/phe_posix_stubs.c -o /wk/out/phe_posix_stubs.o || exit 1
CAIROFLAG=""; [ -f /wk/cairo-x32/src/cairo-features.h ] && CAIROFLAG="-DPHE_HAVE_CAIRO" && $CC $CF $INC -I/wk/cairo-1.16.0/src -I/wk/cairo-x32/src -I/wk/freetype-2.13.3/include -I/wk/fontconfig-2.14.2 -c /h/os/src/pheglibtest/phe_cairo_test.c -o /wk/out/t/phe_cairo_test.o
PGF=""; [ -f /wk/out/libphepango.a ] && PGF="-DPHE_HAVE_PANGO" && $CC $CF $INC -I/wk/pango-1.40.14 -I/wk/pango-x32 -I/wk/fontconfig-2.14.2 -I/wk/freetype-2.13.3/include -I/wk/cairo-1.16.0/src -I/wk/cairo-x32/src -I/wk/harfbuzz-15.1.0/src -c /h/os/src/pheglibtest/phe_pango_test.c -o /wk/out/t/phe_pango_test.o || PGF=""
PXF=""; [ -f /wk/out/libphepixbuf.a ] && PXF="-DPHE_HAVE_PIXBUF" && $CC $CF $INC -I/wk/gdk-pixbuf-2.36.12 -I/wk/pixbuf-x32 -I/wk/pixbuf-x32/gdk-pixbuf -I/wk/glib-2.56.4/gio -I/h/os/src/pheglibtest -c /h/os/src/pheglibtest/phe_pixbuf_test.c -o /wk/out/t/phe_pixbuf_test.o || PXF=""
PIX=""; [ -f /wk/pixman-x32/pixman/pixman-version.h ] && PIX="-DPHE_HAVE_PIXMAN" && $CC $CF $INC -I/wk/pixman-0.42.2/pixman -I/wk/pixman-x32/pixman -I/wk/pixman-x32 -c /h/os/src/pheglibtest/phe_pixman_test.c -o /wk/out/t/phe_pixman_test.o
$CC $CF $INC $PIX $CAIROFLAG $PGF $PXF ${ONLY:+-DPHE_ONLY=\"$ONLY\" -DPHE_LIMIT_MS=${ONLY_LIMIT_MS:-1500000}} -c /h/os/src/pheglibtest/pheglibtest.c -o /wk/out/t/pheglibtest.o || exit 1
for t in hash:hash list:list slist:slist queue:queue array:array-test strfuncs:strfuncs keyfile:keyfile mainloop:mainloop; do
  n=${t%%:*}; f=${t##*:}; $CC $CF $INC -I$S/glib/tests -Dmain=phe_glib_test_$n -Dg_test_init=phe_g_test_init -DG_LOG_DOMAIN=\"GLib\" -c $S/glib/tests/$f.c -o /wk/out/t/t_$n.o 2>&1 | grep -a " error" | head -3; [ -f /wk/out/t/t_$n.o ] || echo "NOT BUILT: $n"
done
mkdir -p $O/testdata; cp /h/os/src/pheglibtest/fonts.conf $O/testdata/; cp $S/glib/tests/keyfiletest.ini $S/glib/tests/pages.ini $S/glib/tests/keyfile.c $O/testdata/   # data files of keyfile.c (rom_data of pheglibtest)
mk() { local a=$1; shift; rm -f $O/$a; ar rcs $O/$a "$@"; echo "$a: $(ar t $O/$a | wc -l) members"; }
mk libpheglib.a /wk/out/phe_posix_stubs.o $X/glib/.libs/*.o $X/glib/deprecated/.libs/*.o $X/glib/libcharset/.libs/*.o $X/glib/gnulib/.libs/*.o $X/glib/pcre/.libs/*.o
mk libphegobject.a $X/gobject/.libs/*.o
mk libphegmodule.a $X/gmodule/.libs/*.o
mk libphegthread.a $X/gthread/.libs/*.o
mk libpheffi.a $(ls /wk/out/ffi/*.o | grep -v win64)
[ -d $X/gio/.libs ] && mk libphegio.a $X/gio/.libs/*.o $X/gio/xdgmime/.libs/*.o $X/gio/gvdb/.libs/*.o
[ -f /wk/out/libphefreetype.a ] && cp /wk/out/libphefreetype.a $O/libphefreetype.a && echo "libphefreetype.a copied"
[ -f /wk/out/libphecairo.a ] && cp /wk/out/libphecairo.a $O/libphecairo.a && echo "libphecairo.a copied"
[ -f /wk/out/libphepixman.a ] && cp /wk/out/libphepixman.a $O/libphepixman.a && echo "libphepixman.a copied"
for a in libphehb libphepng libphezlib libpheexpat libphefc libphepango libphepixbuf; do [ -f /wk/out/$a.a ] && cp /wk/out/$a.a $O/$a.a; done
# DLL archives: fontconfig = fc + expat, png = png + zlib (one DLL each)
if [ -f /wk/out/libphefc.a ]; then rm -f $O/libphefontconfig.a; mkdir -p /wk/out/m1; (cd /wk/out/m1; rm -f *.o; ar x /wk/out/libphefc.a; ar x /wk/out/libpheexpat.a; ar rcs $O/libphefontconfig.a *.o); fi
if [ -f /wk/out/libphepng.a ]; then mkdir -p /wk/out/m2; (cd /wk/out/m2; rm -f *.o; ar x /wk/out/libphepng.a; ar x /wk/out/libphezlib.a; ar rcs $O/libphepng.a *.o); fi
HB=/wk/harfbuzz-15.1.0/src; PG=/wk/pango-1.40.14; PX=/wk/gdk-pixbuf-2.36.12
mkdir -p $O/include/pango $O/include/hb $O/include/gdk-pixbuf $O/include/fontconfig
cp $PG/pango/*.h /wk/pango-x32/pango/*.h $O/include/pango/ 2>/dev/null; cp $HB/hb.h $HB/hb-*.h $O/include/hb/ 2>/dev/null; cp /wk/fontconfig-2.14.2/fontconfig/*.h $O/include/fontconfig/
cp $PX/gdk-pixbuf/gdk-pixbuf.h $PX/gdk-pixbuf/gdk-*.h /wk/pixbuf-x32/gdk-pixbuf/*.h $O/include/gdk-pixbuf/ 2>/dev/null
mkdir -p $O/include; cp /wk/cairo-1.16.0/src/cairo.h /wk/cairo-1.16.0/src/cairo-ft.h /wk/cairo-1.16.0/src/cairo-deprecated.h /wk/cairo-x32/src/cairo-features.h /wk/cairo-1.16.0/src/cairo-version.h $O/include/ 2>/dev/null; cp -r /wk/freetype-2.13.3/include/. $O/include/ 2>/dev/null
# the gdk-symbian skeleton (C half: phegdk_display.c = classes + WSERV main-loop source, phegdkwin_demo.c) as libphegdkwin.a
if [ -f /wk/out/libphepango.a ]; then
  GD="-I/h/os/src/phegdkwin -I/wk/pango-1.40.14 -I/wk/pango-x32 -I/wk/fontconfig-2.14.2 -I/wk/freetype-2.13.3/include -I/wk/cairo-1.16.0/src -I/wk/cairo-x32/src -I/wk/harfbuzz-15.1.0/src -I/wk/gdk-pixbuf-2.36.12 -I/wk/pixbuf-x32 -I/wk/pixbuf-x32/gdk-pixbuf -I/wk/glib-2.56.4/gio"
  mkdir -p /wk/out/gd; rm -f /wk/out/gd/*.o $O/libphegdkwin.a
  for f in phegdk_display phegdkwin_demo; do $CC $CF $INC $GD -c /h/os/src/phegdkwin/$f.c -o /wk/out/gd/$f.o || echo "GDK COMPILE FAIL $f"; done
  ar rcs $O/libphegdkwin.a /wk/out/gd/*.o; cp /h/os/src/phegdkwin/fonts.conf $O/testdata/phegdkwin_fonts.conf; echo "libphegdkwin.a: $(ar t $O/libphegdkwin.a | wc -l) members"
fi
mk libpheglibtest.a /wk/out/t/*.o
