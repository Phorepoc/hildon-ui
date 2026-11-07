#!/usr/bin/env bash
# build_cairo_x32.sh -- IN phorepoc-x86 (volume at /wk, repo :ro at /repo, this repo at /h): cairo 1.16.0 (LGPL 2.1 / MPL 1.1) for x32 over Open C, image backend
# with the FreeType font backend (cairo-ft; fontconfig, PNG, zlib off: a fixed font map replaces fontconfig). Needs build_freetype_x32.sh before. Needs build_pixman_x32.sh first (headers and the pixman objects).
# Same method as pixman: native configure (host) for config.h / cairo-features.h / the object list -> gen_config.py filter -> pp-gcc objects -> libphecairo.a.
set -eu
. /h/recipes/pins.sh
CC=/wk/lsc/bin/pp-gcc
cd /wk; [ -d zlib-1.3.1 ] || tar xf /h/dl/zlib-1.3.1.tar.xz; [ -d cairo-$CAIRO_VER ] || tar xf /h/dl/cairo-$CAIRO_VER.tar.xz
rm -rf cairo-nat cairo-x32; mkdir -p cairo-nat && cd cairo-nat
PXI="-I/wk/pixman-0.42.2/pixman -I/wk/pixman-nat/pixman"
FONTCONFIG_CFLAGS="-I/wk/fontconfig-2.14.2" FONTCONFIG_LIBS="-L/wk/fc-nat/src/.libs -lfontconfig" FREETYPE_CFLAGS="-I/wk/freetype-2.13.3/include" FREETYPE_LIBS="-L/wk/ft-nat -lfreetype" pixman_CFLAGS="$PXI" pixman_LIBS="-L/wk/pixman-nat/pixman/.libs -lpixman-1" /wk/cairo-$CAIRO_VER/configure --disable-shared --enable-static \
  --disable-xlib --disable-xcb --disable-win32 --disable-quartz --enable-ft --enable-fc --disable-png --disable-svg --enable-pdf --enable-ps --disable-script \
  --disable-interpreter --disable-trace --disable-gobject --disable-gtk-doc --disable-valgrind --disable-symbol-lookup --disable-full-testing --without-x > cfg.log 2>&1 \
  || { tail -20 cfg.log; exit 1; }
cd /wk; cp -a cairo-nat cairo-x32; grep -rIl '/wk/cairo-nat' cairo-x32 --include=Makefile --include=libtool --include=config.status 2>/dev/null | xargs -r sed -i 's|/wk/cairo-nat|/wk/cairo-x32|g'
cd /wk/cairo-x32
python3 /h/recipes/gen_config.py config.h - /wk/libc_syms.txt /repo/.epoc32/include/stdapis /wk/cairo-cfg | cut -c1-260
cp /wk/cairo-cfg/config.h config.h
touch config.status; find . -name Makefile -exec touch {} +; sleep 1; touch config.h stamp-h1 src/cairo-features.h src/cairo-supported-features.h 2>/dev/null
find src \( -name '*.o' -o -name '*.lo' -o -name '*.la' -o -name .libs \) -prune -exec rm -rf {} + 2>/dev/null
FF="-I/wk/zlib-1.3.1 -I/h/overlay -include /h/overlay/phe_glib_compat.h -DHAVE_CONFIG_H $PXI -I/wk/pixman-x32/pixman -I/wk/freetype-2.13.3/include -I/wk/fontconfig-2.14.2"
o=$(make -s -C src -f Makefile -f /h/recipes/print.mk P-libcairo_la_OBJECTS)
make -k -j${JOBS:-8} -C src CC=$CC "CFLAGS=-O2 -g0 -w -fno-pic $FF" $o > /wk/cairo-x32/objs.log 2>&1 || true
echo "cairo: $(echo $o | wc -w) objects, errors: $(grep -ac ' error' /wk/cairo-x32/objs.log)"; grep -a ' error' /wk/cairo-x32/objs.log | sed 's/.*cairo-1.16.0.//' | cut -c1-150 | sort | uniq -c | sort -rn | head -12
mkdir -p /wk/out; rm -f /wk/out/libphecairo.a; ar rcs /wk/out/libphecairo.a src/*.o; ar t /wk/out/libphecairo.a | wc -l
