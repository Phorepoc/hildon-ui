#!/usr/bin/env bash
# build_pango_x32.sh -- IN phorepoc-x86 (volume at /wk, repo :ro at /repo, this repo at /h): Pango 1.40.14 (LGPL 2.1+, the last autotools Pango) for x32 over Open C: libpango, PangoFT2 and
# pangocairo (fontconfig backend) in ONE archive. Needs the glib native tree (build.sh steps up to objects), harfbuzz, freetype, fontconfig, cairo recipes before.
# The configure finds its dependencies through hand-written .pc files (/wk/pc) that point at the native/x32 build trees; the object list comes from the native Makefiles.
set -eu
. /h/recipes/pins.sh
CC=/wk/lsc/bin/pp-gcc; mkdir -p /wk/out /wk/pc
cd /wk; [ -d pango-$PANGO_VER ] || tar xf /h/dl/pango-$PANGO_VER.tar.xz
GI="-I/wk/glib-2.56.4 -I/wk/glib-2.56.4/glib -I/wk/glib-2.56.4/gmodule -I/wk/nat/glib -I/wk/ffi-inst/include"
pc() { # name version requires cflags
  printf 'Name: %s\nDescription: %s\nVersion: %s\nRequires: %s\nCflags: %s\nLibs:\n' "$1" "$1" "$2" "$3" "$4" > /wk/pc/$1.pc; }
pc glib-2.0 2.56.4 "" "$GI"; pc gobject-2.0 2.56.4 glib-2.0 "-I/wk/glib-2.56.4/gobject"; pc gmodule-no-export-2.0 2.56.4 glib-2.0 ""; pc gio-2.0 2.56.4 gobject-2.0 "-I/wk/glib-2.56.4/gio -I/wk/nat/gio"
pc gio-unix-2.0 2.56.4 gio-2.0 ""; pc gthread-2.0 2.56.4 glib-2.0 ""
for v in glib_genmarshal=/wk/nat/gobject/glib-genmarshal glib_mkenums=/wk/nat/gobject/glib-mkenums gobject_query=/wk/nat/gobject/gobject-query glib_compile_resources=/bin/true; do echo "${v%%=*}=${v#*=}" >> /wk/pc/glib-2.0.pc; done
sed -i 's|^Name:|prefix=/wk\nName:|' /wk/pc/glib-2.0.pc
pc cairo 1.16.0 pixman-1 "-I/wk/cairo-1.16.0/src -I/wk/cairo-nat/src"; pc cairo-ft 1.16.0 "cairo freetype2" ""; pc cairo-png 1.16.0 cairo ""
pc pixman-1 0.42.2 "" "-I/wk/pixman-0.42.2/pixman -I/wk/pixman-nat/pixman"; pc freetype2 25.0.19 "" "-I/wk/freetype-2.13.3/include"; pc fontconfig 2.14.2 "freetype2" "-I/wk/fontconfig-2.14.2"
pc harfbuzz $HARFBUZZ_VER "" "-I/wk/harfbuzz-$HARFBUZZ_VER/src"
rm -rf pango-nat pango-x32; mkdir pango-nat; cd pango-nat
PKG_CONFIG_PATH=/wk/pc PKG_CONFIG_LIBDIR=/wk/pc /wk/pango-$PANGO_VER/configure --disable-shared --enable-static --disable-introspection --disable-gtk-doc --disable-man --with-xft=no \
  --without-dynamic-modules --disable-installed-tests > cfg.log 2>&1 || { tail -25 cfg.log; exit 1; }
# native build: generated sources (enum types, marshallers), pango-features.h ...
(cd /wk/pango-nat; PKG_CONFIG_PATH=/wk/pc make -k -j${JOBS:-8} -C pango CFLAGS="-O0 -w" > nat-make.log 2>&1) || true
cd /wk; cp -a pango-nat pango-x32; grep -rIl '/wk/pango-nat' pango-x32 --include=Makefile --include=libtool --include=config.status 2>/dev/null | xargs -r sed -i 's|/wk/pango-nat|/wk/pango-x32|g'
cd pango-x32; find . -name Makefile -exec sed -i "s|-I/wk/nat/glib|-I/wk/x32/glib|g; s|-I/wk/nat/gio|-I/wk/x32/gio|g; s|-I/wk/cairo-nat/src|-I/wk/cairo-x32/src|g; s|-I/wk/pixman-nat/pixman|-I/wk/pixman-x32/pixman|g" {} +   # the Makefiles carry the NATIVE trees (native glibconfig.h has 64-bit sizes)
python3 /h/recipes/gen_config.py config.h - /wk/libc_syms.txt /repo/.epoc32/include/stdapis /wk/pango-cfg | cut -c1-240
cp /wk/pango-cfg/config.h config.h
touch config.status; find . -name Makefile -exec touch {} +; sleep 1; touch config.h stamp-h1
find pango \( -name '*.o' -o -name '*.lo' -o -name '*.la' -o -name .libs \) -prune -exec rm -rf {} + 2>/dev/null
X32="-I/wk/x32 -I/wk/x32/glib -I/wk/glib-2.56.4 -I/wk/glib-2.56.4/glib -I/wk/glib-2.56.4/gobject -I/wk/glib-2.56.4/gmodule -I/wk/glib-2.56.4/gio -I/wk/ffi-x32/include"
FF="-I/h/overlay -include /h/overlay/phe_glib_compat.h -DHAVE_CONFIG_H $X32 -I/wk/cairo-1.16.0/src -I/wk/cairo-x32/src -I/wk/pixman-0.42.2/pixman -I/wk/freetype-2.13.3/include -I/wk/fontconfig-2.14.2 -I/wk/harfbuzz-$HARFBUZZ_VER/src -I/wk/pango-x32"
: > /wk/pango-x32/objs.log
for v in libpango_1_0_la_OBJECTS libpangoft2_1_0_la_OBJECTS libpangocairo_1_0_la_OBJECTS; do
  o=$(make -s -C pango -f Makefile -f /h/recipes/print.mk P-$v 2>/dev/null) || continue
  make -k -j${JOBS:-8} -C pango CC=$CC "CFLAGS=-O2 -g0 -w -fno-pic $FF" $o >> /wk/pango-x32/objs.log 2>&1 || true
  echo "pango/$v: $(echo $o | wc -w) objects"
done
echo "errors: $(grep -ac ' error' /wk/pango-x32/objs.log)"; grep -a ' error' /wk/pango-x32/objs.log | sed 's/.*pango-1.40.14.//' | cut -c1-160 | sort | uniq -c | sort -rn | head -10
rm -f /wk/out/libphepango.a; ar rcs /wk/out/libphepango.a pango/*.o; ar t /wk/out/libphepango.a | wc -l
