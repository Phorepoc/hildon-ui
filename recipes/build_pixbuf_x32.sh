#!/usr/bin/env bash
# build_pixbuf_x32.sh -- IN phorepoc-x86 (volume at /wk, repo :ro at /repo, this repo at /h): gdk-pixbuf 2.36.12 (LGPL 2+, the last autotools release; GTK 2.24 needs >= 2.21) for x32 over Open C,
# with the PNG loader BUILT IN (no loader modules: --with-included-loaders=png), libpng/zlib from the zlib/png recipe. Same method as pango. Out: /wk/out/libphepixbuf.a
set -eu
. /h/recipes/pins.sh
CC=/wk/lsc/bin/pp-gcc; mkdir -p /wk/out
cd /wk; [ -d gdk-pixbuf-$GDKPIXBUF_VER ] || tar xf /h/dl/gdk-pixbuf-$GDKPIXBUF_VER.tar.xz
printf 'Name: libpng16\nDescription: png\nVersion: %s\nRequires:\nCflags: -I/wk/libpng-%s -I/wk/png-work -I/wk/zlib-%s\nLibs: -L/wk/png-work/nat -lpng16 -lz -lm\n' $LIBPNG_VER $LIBPNG_VER $ZLIB_VER > /wk/pc/libpng16.pc
# gio_can_sniff=yes (set on the configure line below): otherwise configure turns GDK_PIXBUF_USE_GIO_MIME on, i.e. format sniffing through the shared-mime-info DATABASE
# (g_content_type_guess), which the device does not have: every untyped load then fails with "Unrecognized image file format". The built-in signature check is wanted.
rm -rf pixbuf-nat pixbuf-x32; mkdir pixbuf-nat; cd pixbuf-nat
gio_can_sniff=yes PKG_CONFIG_PATH=/wk/pc PKG_CONFIG_LIBDIR=/wk/pc LDFLAGS="-L/wk/png-work/nat" CFLAGS="-I/wk/png-work -I/wk/libpng-$LIBPNG_VER -I/wk/zlib-$ZLIB_VER" /wk/gdk-pixbuf-$GDKPIXBUF_VER/configure --disable-shared --enable-static --disable-introspection --disable-gtk-doc \
  --disable-man --without-libtiff --without-libjpeg --without-libjasper --with-included-loaders=png --disable-modules --disable-glibtest --without-x11 > cfg.log 2>&1 || { tail -25 cfg.log; exit 1; }
(cd /wk/pixbuf-nat; PKG_CONFIG_PATH=/wk/pc make -k -j${JOBS:-8} -C gdk-pixbuf CFLAGS="-O0 -w -I/wk/png-work -I/wk/libpng-$LIBPNG_VER -I/wk/zlib-$ZLIB_VER" > nat-make.log 2>&1) || true
cd /wk; cp -a pixbuf-nat pixbuf-x32; grep -rIl '/wk/pixbuf-nat' pixbuf-x32 --include=Makefile --include=libtool --include=config.status 2>/dev/null | xargs -r sed -i 's|/wk/pixbuf-nat|/wk/pixbuf-x32|g'
cd pixbuf-x32; find . -name Makefile -exec sed -i "s|-I/wk/nat/glib|-I/wk/x32/glib|g; s|-I/wk/nat/gio|-I/wk/x32/gio|g" {} +
python3 /h/recipes/gen_config.py config.h - /wk/libc_syms.txt /repo/.epoc32/include/stdapis /wk/pixbuf-cfg | cut -c1-200
cp /wk/pixbuf-cfg/config.h config.h
touch config.status; find . -name Makefile -exec touch {} +; sleep 1; touch config.h stamp-h1
find gdk-pixbuf \( -name '*.o' -o -name '*.lo' -o -name '*.la' -o -name .libs \) -prune -exec rm -rf {} + 2>/dev/null
X32="-I/wk/x32 -I/wk/x32/glib -I/wk/glib-2.56.4 -I/wk/glib-2.56.4/glib -I/wk/glib-2.56.4/gobject -I/wk/glib-2.56.4/gmodule -I/wk/glib-2.56.4/gio -I/wk/ffi-x32/include"
FF="-I/h/overlay -include /h/overlay/phe_glib_compat.h -DHAVE_CONFIG_H $X32 -I/wk/png-work -I/wk/libpng-$LIBPNG_VER -I/wk/zlib-$ZLIB_VER -I/wk/pixbuf-x32 -I/wk/pixbuf-x32/gdk-pixbuf"
: > /wk/pixbuf-x32/objs.log
for v in libgdk_pixbuf_2_0_la_OBJECTS libpixops_la_OBJECTS libpixbufloader_static_png_la_OBJECTS; do
  for d in gdk-pixbuf gdk-pixbuf/pixops; do
    o=$(make -s -C $d -f Makefile -f /h/recipes/print.mk P-$v 2>/dev/null) || continue; [ -n "$o" ] || continue
    make -k -j${JOBS:-8} -C $d CC=$CC "CFLAGS=-O2 -g0 -w -fno-pic $FF" $o >> /wk/pixbuf-x32/objs.log 2>&1 || true
    echo "$d/$v: $(echo $o | wc -w) objects"
  done
done
echo "errors: $(grep -ac ' error' /wk/pixbuf-x32/objs.log)"; grep -a ' error' /wk/pixbuf-x32/objs.log | sed 's/.*gdk-pixbuf-2.36.12.//' | cut -c1-160 | sort | uniq -c | sort -rn | head -10
make -k -C gdk-pixbuf CC=$CC "CFLAGS=-O2 -g0 -w -fno-pic $FF -DGDK_PIXBUF_ENABLE_BACKEND" io-png.o >> /wk/pixbuf-x32/objs.log 2>&1 || $CC -O2 -g0 -w -fno-pic $FF -DHAVE_CONFIG_H -c gdk-pixbuf/io-png.c -o gdk-pixbuf/io-png.o >> /wk/pixbuf-x32/objs.log 2>&1 || true
rm -f /wk/out/libphepixbuf.a; ar rcs /wk/out/libphepixbuf.a gdk-pixbuf/*.o gdk-pixbuf/pixops/*.o 2>/dev/null; ar t /wk/out/libphepixbuf.a | wc -l
