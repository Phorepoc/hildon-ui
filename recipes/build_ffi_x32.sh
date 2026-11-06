#!/usr/bin/env bash
# build_ffi_x32.sh -- IN phorepoc-x86 (/wk): libffi 3.4.6 for x32 by hand with pp-gcc -> /wk/out/libffi.a. GLib only needs ffi_call
# (g_cclosure_marshal_generic), so closures.c / tramp.c are left out. ffi.h/fficonfig.h come from native_build.sh's configure.
set -u
CC=/wk/lsc/bin/pp-gcc; S=/wk/libffi-3.4.6; B=/wk/ffi-nat; O=/wk/out/ffi; mkdir -p $O
mkdir -p /wk/ffi-x32; sed "s|^#define HAVE_ALLOCA_H 1|/* no alloca.h */|; s|^#define HAVE_MMAP.*||" $B/fficonfig.h > /wk/ffi-x32/fficonfig.h
I="-I/wk/ffi-x32 -I$B/include -I$B -I$S/include -I$S/src/x86"
for f in src/prep_cif src/types src/raw_api src/x86/ffi64; do
  $CC -O2 -g0 -w -fno-pic $I -DHAVE_CONFIG_H -c $S/$f.c -o $O/$(basename $f).o || echo "FAIL $f"; done
for f in unix64; do $CC -O2 -g0 -w -fno-pic $I -DHAVE_CONFIG_H -x assembler-with-cpp -c $S/src/x86/$f.S -o $O/$f.o || echo "FAIL $f.S"; done
rm -f /wk/out/libffi.a; ar rcs /wk/out/libffi.a $O/*.o; ar t /wk/out/libffi.a | tr '\n' ' '
