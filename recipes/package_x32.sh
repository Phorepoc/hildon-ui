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
# GDK 2.24.33 core + the Symbian backend (build_gdk_x32.sh, build_gdk_backend_x32.sh) as libphegdk.a, and the demo application (public GDK API) as libphegdkwin.a
if ls /wk/gdk-x32/be/*.o >/dev/null 2>&1; then
  rm -f $O/libphegdk.a; ar rcs $O/libphegdk.a /wk/gdk-x32/*.o /wk/gdk-x32/be/*.o; echo "libphegdk.a: $(ar t $O/libphegdk.a | wc -l) members"
  GT=/wk/gtk+-2.24.33
  GD="-I/h/os/src/phegdkwin -I/wk/gdk-x32 -I$GT -I$GT/gdk -I/wk/pango-1.40.14 -I/wk/pango-x32 -I/wk/fontconfig-2.14.2 -I/wk/freetype-2.13.3/include -I/wk/cairo-1.16.0/src -I/wk/cairo-x32/src -I/wk/harfbuzz-15.1.0/src -I/wk/gdk-pixbuf-2.36.12 -I/wk/pixbuf-x32 -I/wk/glib-2.56.4/gio"
  mkdir -p /wk/out/gd; rm -f /wk/out/gd/*.o $O/libphegdkwin.a
  $CC $CF $INC $GD -include /wk/gdk-x32/gtkcompat.h -c /h/os/src/phegdkwin/phegdkwin_demo.c -o /wk/out/gd/phegdkwin_demo.o || echo "GDK DEMO COMPILE FAIL"
  ar rcs $O/libphegdkwin.a /wk/out/gd/*.o; cp /h/os/src/phegdkwin/fonts.conf $O/testdata/phegdkwin_fonts.conf; echo "libphegdkwin.a built"
fi
# ATK (build_atk_x32.sh) and GTK 2.24.33 (build_gtk_x32.sh) as libpheatk.a / libphegtk.a, and the GTK demo (public GTK API) as libphegtkwin.a
if ls /wk/gtk-x32/gtkwindow.o >/dev/null 2>&1; then
  cp /wk/out/libpheatk.a $O/libpheatk.a; rm -f $O/libphegtk.a; ar rcs $O/libphegtk.a /wk/gtk-x32/*.o; echo "libphegtk.a: $(ar t $O/libphegtk.a | wc -l) members"
  GT=/wk/gtk+-2.24.33; GK="-I/h/os/src/phegtkwin -I/wk/gtk-x32 -I/wk/gtk-x32/inc -I$GT -I/wk/atk-2.38.0 -I/wk/gdk-x32 -I$GT/gdk -I/wk/pango-1.40.14 -I/wk/pango-x32 -I/wk/fontconfig-2.14.2 -I/wk/freetype-2.13.3/include -I/wk/cairo-1.16.0/src -I/wk/cairo-x32/src -I/wk/harfbuzz-15.1.0/src -I/wk/gdk-pixbuf-2.36.12 -I/wk/pixbuf-x32 -I/wk/glib-2.56.4/gio"
  mkdir -p /wk/out/gk; rm -f /wk/out/gk/*.o $O/libphegtkwin.a
  $CC $CF $INC $GK -include /wk/gdk-x32/gtkcompat.h -c /h/os/src/phegtkwin/phegtkwin_demo.c -o /wk/out/gk/phegtkwin_demo.o || echo "GTK DEMO COMPILE FAIL"
  ar rcs $O/libphegtkwin.a /wk/out/gk/*.o; cp /h/os/src/phegtkwin/fonts.conf $O/testdata/phegtkwin_fonts.conf; echo "libphegtkwin.a built"
fi
# libhildon (build_hildon_x32.sh) as libphehildon.a, and its demo (public libhildon API) as libphehildonwin.a
if [ -f /wk/out/libphehildon.a ]; then
  cp /wk/out/libphehildon.a $O/libphehildon.a; echo "libphehildon.a: $(ar t $O/libphehildon.a | wc -l) members"
  GT=/wk/gtk+-2.24.33; HI="-I/h/os/src/phehildonwin -I/wk/hildon-x32 -I/wk/hildon-x32/inc -I/h/dl/libhildon -I/h/os/src/phehildon/shims -I/wk/gtk-x32 -I/wk/gtk-x32/inc -I$GT -I/wk/atk-2.38.0 -I/wk/gdk-x32 -I$GT/gdk -I/wk/pango-1.40.14 -I/wk/pango-x32 -I/wk/fontconfig-2.14.2 -I/wk/freetype-2.13.3/include -I/wk/cairo-1.16.0/src -I/wk/cairo-x32/src -I/wk/harfbuzz-15.1.0/src -I/wk/gdk-pixbuf-2.36.12 -I/wk/pixbuf-x32 -I/wk/glib-2.56.4/gio"
  mkdir -p /wk/out/hi; rm -f /wk/out/hi/*.o $O/libphehildonwin.a
  $CC $CF $INC $HI -include /wk/gdk-x32/gtkcompat.h -c /h/os/src/phehildonwin/phehildonwin_demo.c -o /wk/out/hi/phehildonwin_demo.o || echo "HILDON DEMO COMPILE FAIL"
  ar rcs $O/libphehildonwin.a /wk/out/hi/*.o; cp /h/os/src/phehildonwin/fonts.conf $O/testdata/phehildonwin_fonts.conf; echo "libphehildonwin.a built"
fi
mk libpheglibtest.a /wk/out/t/*.o
