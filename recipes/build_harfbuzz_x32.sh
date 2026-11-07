#!/usr/bin/env bash
# build_harfbuzz_x32.sh -- IN phorepoc-x86 (volume at /wk, this repo at /h): HarfBuzz 15.1.0 (MIT) for x32 over Open C, from its own release tarball, as the amalgamated src/harfbuzz.cc
# (shaper core + OpenType layout + the AAT/Indic/... shapers + the hb-ft and hb-glib glue Pango 1.40 calls; no ICU). Needs build_cxxheaders_x32.sh (libstdc++ headers).
# C++ without exceptions/RTTI/threads (HB_NO_MT: the face and font caches are not locked; Hildon runs one UI thread). Out: /wk/out/libphehb.a
set -eu
. /h/recipes/pins.sh
cd /wk; [ -d harfbuzz-$HARFBUZZ_VER ] || tar xf /h/dl/harfbuzz-$HARFBUZZ_VER.tar.xz
S=/wk/harfbuzz-$HARFBUZZ_VER; LT=x86_64-unknown-elf; mkdir -p /wk/hb-work /wk/out
HB="-U__linux__ -U__linux -DPHOREPOC=1 -O2 -g0 -w -fno-pic -std=gnu++17 -fno-exceptions -fno-rtti -fno-threadsafe-statics -DHB_NO_MT -DHAVE_FREETYPE -DHAVE_GLIB -I/wk/freetype-2.13.3/include -I/wk/x32 -I/wk/x32/glib -I/wk/glib-2.56.4 -I/wk/glib-2.56.4/glib -I/wk/glib-2.56.4/gobject -I/wk/ffi-x32/include -DHB_NO_PRAGMA_GCC_DIAGNOSTIC_ERROR
 -I/wk/lsc/build/include/$LT -I/wk/lsc/build/include -I/wk/lsc/gcc-13.3.0/libstdc++-v3/libsupc++"
/wk/lsc/bin/pp-g++ $HB -c $S/src/harfbuzz.cc -o /wk/hb-work/harfbuzz.o 2>&1 | grep -a "error" | head -12 || true
rm -f /wk/out/libphehb.a; ar rcs /wk/out/libphehb.a /wk/hb-work/harfbuzz.o; ls -l /wk/out/libphehb.a | cut -c20-80; nm /wk/out/libphehb.a | grep -c " T hb_"
