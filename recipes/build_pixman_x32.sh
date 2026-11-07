#!/usr/bin/env bash
# build_pixman_x32.sh -- IN phorepoc-x86 (volume at /wk, repo :ro at /repo, this repo at /h): pixman 0.42.2 (MIT) for x32 over Open C.
# 1) native configure (host) with every SIMD/asm backend off -> config.h, pixman-version.h, the object list; 2) gen_config.py filters config.h by Open C's
# exports (+ SIZEOF_LONG 4); no TLS (an E32 image has no ELF TLS): PIXMAN_NO_TLS -> pthread keys; 3) every object with pp-gcc -> /wk/out/libphepixman.a.
set -eu
. /h/recipes/pins.sh
CC=/wk/lsc/bin/pp-gcc
cd /wk; [ -d pixman-$PIXMAN_VER ] || tar xf /h/dl/pixman-$PIXMAN_VER.tar.gz
rm -rf pixman-nat pixman-x32; mkdir -p pixman-nat && cd pixman-nat
/wk/pixman-$PIXMAN_VER/configure --disable-shared --enable-static --disable-openmp --disable-loongson-mmi --disable-mmx --disable-sse2 --disable-ssse3 \
  --disable-vmx --disable-arm-simd --disable-arm-neon --disable-arm-iwmmxt --disable-arm-a64-neon --disable-mips-dspr2 --disable-gcc-inline-asm \
  --disable-timers --disable-gtk --disable-libpng > cfg.log 2>&1
cd /wk; cp -a pixman-nat pixman-x32; grep -rIl '/wk/pixman-nat' pixman-x32 --include=Makefile --include=libtool --include=config.status 2>/dev/null | xargs -r sed -i 's|/wk/pixman-nat|/wk/pixman-x32|g'
cd /wk/pixman-x32
python3 /h/recipes/gen_config.py config.h - /wk/libc_syms.txt /repo/.epoc32/include/stdapis /wk/pixman-cfg | cut -c1-200
cp /wk/pixman-cfg/config.h config.h
printf '\n#ifndef PIXMAN_NO_TLS\n#define PIXMAN_NO_TLS 1\n#endif\n#undef TLS\n#define TLS\n' >> config.h
touch config.status; find . -name Makefile -exec touch {} +; sleep 1; touch config.h stamp-h1
find . \( -name '*.o' -o -name '*.lo' -o -name '*.la' -o -name .libs \) -prune -exec rm -rf {} + 2>/dev/null
FF="-I/h/overlay -include /h/overlay/phe_glib_compat.h -DPIXMAN_NO_TLS -DHAVE_CONFIG_H"
o=$(make -s -C pixman -f Makefile -f /h/recipes/print.mk P-libpixman_1_la_OBJECTS)
make -k -j${JOBS:-8} -C pixman CC=$CC "CFLAGS=-O2 -g0 -w -fno-pic $FF" $o > /wk/pixman-x32/objs.log 2>&1 || true
echo "pixman: $(echo $o | wc -w) objects, errors: $(grep -ac ' error' /wk/pixman-x32/objs.log)"; grep -a ' error' /wk/pixman-x32/objs.log | sed 's/.*pixman-0.42.2.//' | cut -c1-150 | sort | uniq -c | head
mkdir -p /wk/out; rm -f /wk/out/libphepixman.a; ar rcs /wk/out/libphepixman.a pixman/*.o; ar t /wk/out/libphepixman.a | wc -l
