#!/usr/bin/env bash
# build_freetype_x32.sh -- IN phorepoc-x86 (volume at /wk, repo :ro at /repo, this repo at /h): FreeType 2.13.3 (FTL) for x32 over Open C, from the release tarball
# (NOT the web stack's archive). Compiled by the module list (the amalgamated src/<module>/<module>.c files + the base files), ANSI stdio ftsystem.c (no mmap on Open C),
# no zlib/LZW (a custom ftoption.h), no PNG/HarfBuzz/Brotli. Also builds a host (native) libfreetype.a that cairo's configure links its FT feature checks against.
# Out: /wk/out/libphefreetype.a (x32), /wk/ft-nat/libfreetype.a (host).
set -eu
. /h/recipes/pins.sh
CC=/wk/lsc/bin/pp-gcc
cd /wk; [ -d freetype-$FREETYPE_VER ] || tar xf /h/dl/freetype-$FREETYPE_VER.tar.xz
S=/wk/freetype-$FREETYPE_VER; W=/wk/ft-work; rm -rf $W /wk/ft-nat; mkdir -p $W/x32 $W/nat /wk/ft-nat /wk/out
sed 's|^#define FT_CONFIG_OPTION_USE_ZLIB|/* PHE: off */ /* #define FT_CONFIG_OPTION_USE_ZLIB */|; s|^#define FT_CONFIG_OPTION_USE_LZW|/* PHE: off */ /* #define FT_CONFIG_OPTION_USE_LZW */|' $S/include/freetype/config/ftoption.h > $W/phe_ftoption.h
SRC="base/ftbase base/ftbbox base/ftbdf base/ftbitmap base/ftcid base/ftfstype base/ftgasp base/ftglyph base/ftgxval base/ftinit base/ftmm base/ftotval base/ftpatent base/ftpfr base/ftstroke base/ftsynth base/ftsystem base/fttype1 base/ftwinfnt base/ftdebug
 autofit/autofit bdf/bdf cff/cff cid/type1cid pcf/pcf pfr/pfr psaux/psaux pshinter/pshinter psnames/psmodule raster/raster sdf/sdf svg/svg sfnt/sfnt smooth/smooth truetype/truetype type1/type1 type42/type42 winfonts/winfnt"
DEF="-DFT2_BUILD_LIBRARY -DFT_CONFIG_OPTIONS_H=<phe_ftoption.h> -I$W -I$S/include"
for f in $SRC; do
  $CC -O2 -g0 -w -fno-pic $DEF -c $S/src/$f.c -o $W/x32/$(basename $f).o || echo "FAIL x32 $f"
  gcc -O1 -w $DEF -c $S/src/$f.c -o $W/nat/$(basename $f).o || echo "FAIL nat $f"
done
rm -f /wk/out/libphefreetype.a /wk/ft-nat/libfreetype.a; ar rcs /wk/out/libphefreetype.a $W/x32/*.o; ar rcs /wk/ft-nat/libfreetype.a $W/nat/*.o
echo "freetype: $(ar t /wk/out/libphefreetype.a | wc -l) x32 objects, $(ar t /wk/ft-nat/libfreetype.a | wc -l) host objects"
