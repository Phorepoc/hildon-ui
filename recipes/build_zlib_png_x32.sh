#!/usr/bin/env bash
# build_zlib_png_x32.sh -- IN phorepoc-x86 (volume at /wk, this repo at /h): zlib 1.3.1 (zlib licence) and libpng 1.6.43 (libpng licence) for x32 over Open C, each from its own pinned tarball,
# compiled file by file with pp-gcc (no SIMD; the prebuilt pnglibconf.h). Out: /wk/out/libphezlib.a, /wk/out/libphepng.a
set -eu
. /h/recipes/pins.sh
CC=/wk/lsc/bin/pp-gcc; mkdir -p /wk/out /wk/png-work/z /wk/png-work/p
cd /wk; [ -d zlib-$ZLIB_VER ] || tar xf /h/dl/zlib-$ZLIB_VER.tar.gz; [ -d libpng-$LIBPNG_VER ] || tar xf /h/dl/libpng-$LIBPNG_VER.tar.xz
Z=/wk/zlib-$ZLIB_VER; P=/wk/libpng-$LIBPNG_VER
for f in adler32 compress crc32 deflate gzclose gzlib gzread gzwrite infback inffast inflate inftrees trees uncompr zutil; do
  $CC -O2 -g0 -w -fno-pic -I$Z -c $Z/$f.c -o /wk/png-work/z/$f.o || echo "FAIL zlib $f"; done
rm -f /wk/out/libphezlib.a; ar rcs /wk/out/libphezlib.a /wk/png-work/z/*.o
cp $P/scripts/pnglibconf.h.prebuilt /wk/png-work/pnglibconf.h
for f in png pngerror pngget pngmem pngpread pngread pngrio pngrtran pngrutil pngset pngtrans pngwio pngwrite pngwtran pngwutil; do
  $CC -O2 -g0 -w -fno-pic -DPNG_INTEL_SSE_OPT=0 -I/wk/png-work -I$P -I$Z -c $P/$f.c -o /wk/png-work/p/$f.o || echo "FAIL png $f"; done
rm -f /wk/out/libphepng.a; ar rcs /wk/out/libphepng.a /wk/png-work/p/*.o
echo "zlib: $(ar t /wk/out/libphezlib.a | wc -l) objects, libpng: $(ar t /wk/out/libphepng.a | wc -l) objects"
# host (native) copies: gdk-pixbuf's configure links its libpng check against them
mkdir -p /wk/png-work/nat; for f in adler32 compress crc32 deflate gzclose gzlib gzread gzwrite infback inffast inflate inftrees trees uncompr zutil; do gcc -O1 -w -I$Z -c $Z/$f.c -o /wk/png-work/nat/z_$f.o; done
for f in png pngerror pngget pngmem pngpread pngread pngrio pngrtran pngrutil pngset pngtrans pngwio pngwrite pngwtran pngwutil; do gcc -O1 -w -DPNG_INTEL_SSE_OPT=0 -I/wk/png-work -I$P -I$Z -c $P/$f.c -o /wk/png-work/nat/p_$f.o; done
rm -f /wk/png-work/nat/libpng16.a /wk/png-work/nat/libz.a; ar rcs /wk/png-work/nat/libz.a /wk/png-work/nat/z_*.o; ar rcs /wk/png-work/nat/libpng16.a /wk/png-work/nat/p_*.o; ls /wk/png-work/nat/*.a
