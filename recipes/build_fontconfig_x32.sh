#!/usr/bin/env bash
# build_fontconfig_x32.sh -- IN phorepoc-x86 (volume at /wk, repo :ro at /repo, this repo at /h): expat 2.5.0 (MIT) and fontconfig 2.14.2 (MIT) for x32 over Open C, each from its own pinned
# tarball. Pango's PangoFT2/pangocairo-fc backends are written against fontconfig, so the real fontconfig is built (not a stand-in); the "fixed ROM font map" is a fonts.conf
# that lists only the ROM font directory (os/src/pheglibtest/data/fonts.conf, FONTCONFIG_FILE at run time). Method as pixman/cairo: native configure -> config.h filtered by gen_config.py
# -> pp-gcc objects. Out: /wk/out/libpheexpat.a, /wk/out/libphefc.a
set -eu
. /h/recipes/pins.sh
CC=/wk/lsc/bin/pp-gcc; mkdir -p /wk/out /wk/fc-work/e
cd /wk; [ -d expat-$EXPAT_VER ] || tar xf /h/dl/expat-$EXPAT_VER.tar.xz; [ -d fontconfig-$FONTCONFIG_VER ] || tar xf /h/dl/fontconfig-$FONTCONFIG_VER.tar.xz
E=/wk/expat-$EXPAT_VER/lib
cat > /wk/fc-work/expat_config.h <<'EOC'
#define BYTEORDER 1234
#define HAVE_MEMMOVE 1
#define STDC_HEADERS 1
#define XML_CONTEXT_BYTES 1024
#define XML_DTD 1
#define XML_NS 1
#define XML_POOR_ENTROPY 1
EOC
for f in xmlparse xmltok xmlrole; do $CC -O2 -g0 -w -fno-pic -DHAVE_EXPAT_CONFIG_H -I/wk/fc-work -I$E -c $E/$f.c -o /wk/fc-work/e/$f.o || echo "FAIL expat $f"; done
mkdir -p /wk/fc-work/en; for f in xmlparse xmltok xmlrole; do gcc -O1 -w -DHAVE_EXPAT_CONFIG_H -I/wk/fc-work -I$E -c $E/$f.c -o /wk/fc-work/en/$f.o; done; ar rcs /wk/fc-work/libexpat.a /wk/fc-work/en/*.o   # host copy: fontconfig's configure links its expat check against it
rm -f /wk/out/libpheexpat.a; ar rcs /wk/out/libpheexpat.a /wk/fc-work/e/*.o
# fontconfig: native configure for config.h and the object list
rm -rf fc-nat fc-x32; mkdir fc-nat; cd fc-nat
FREETYPE_CFLAGS="-I/wk/freetype-2.13.3/include" FREETYPE_LIBS="-L/wk/ft-nat -lfreetype" EXPAT_CFLAGS="-I$E" EXPAT_LIBS="-L/wk/fc-work -lexpat" \
 /wk/fontconfig-$FONTCONFIG_VER/configure --disable-shared --enable-static --disable-docs --disable-nls --disable-cache-build --with-default-fonts=z:/resource/fonts \
 --with-add-fonts="" --sysconfdir=z:/resource/fonts --localstatedir=c:/data --with-cache-dir=c:/data/fc-cache > cfg.log 2>&1 || { tail -15 cfg.log; exit 1; }
(cd /wk/fc-nat; make -k -j${JOBS:-8} -C fc-case; make -k -j${JOBS:-8} -C fc-lang; make -k -j${JOBS:-8} -C src) > /wk/fc-nat/nat-make.log 2>&1 || true   # native build: generates fcalias.h, fcftalias.h, the case/lang tables
cd /wk; cp -a fc-nat fc-x32; grep -rIl '/wk/fc-nat' fc-x32 --include=Makefile --include=libtool --include=config.status 2>/dev/null | xargs -r sed -i 's|/wk/fc-nat|/wk/fc-x32|g'
cd fc-x32
python3 /h/recipes/gen_config.py config.h - /wk/libc_syms.txt /repo/.epoc32/include/stdapis /wk/fc-cfg | cut -c1-240
cp /wk/fc-cfg/config.h config.h
touch config.status; find . -name Makefile -exec touch {} +; sleep 1; touch config.h stamp-h1
find src \( -name '*.o' -o -name '*.lo' -o -name '*.la' -o -name .libs \) -prune -exec rm -rf {} + 2>/dev/null
FF="-I/h/overlay -include /h/overlay/phe_glib_compat.h -DHAVE_CONFIG_H -I/wk/freetype-2.13.3/include -I$E -I/wk/fc-x32 -I/wk/fontconfig-$FONTCONFIG_VER"
o=$(make -s -C src -f Makefile -f /h/recipes/print.mk P-libfontconfig_la_OBJECTS)
make -k -j${JOBS:-8} -C src CC=$CC "CFLAGS=-O2 -g0 -w -fno-pic $FF" $o > /wk/fc-x32/objs.log 2>&1 || true
echo "fontconfig: $(echo $o | wc -w) objects, errors: $(grep -ac ' error' /wk/fc-x32/objs.log)"; grep -a ' error' /wk/fc-x32/objs.log | sed 's/.*fontconfig-2.14.2.//' | cut -c1-160 | sort | uniq -c | sort -rn | head -10
rm -f /wk/out/libphefc.a; ar rcs /wk/out/libphefc.a src/*.o; ar t /wk/out/libphefc.a | wc -l
