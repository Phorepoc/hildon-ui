#!/usr/bin/env bash
# native_build.sh -- IN phorepoc-x86 (volumes hildon-wk:/wk, repo:/h): build libffi + GLib NATIVELY to get the generated sources
# (gmarshal.c, enum types, glib-genmarshal...) and ffi.h. Needs native_config.sh run first.
set -u
. /h/recipes/pins.sh
cd /wk
[ -d libffi-$FFI_VER ] || tar xf /h/dl/libffi-$FFI_VER.tar.gz
mkdir -p ffi-nat && cd ffi-nat
[ -f Makefile ] || /wk/libffi-$FFI_VER/configure --prefix=/wk/ffi-inst --disable-docs > cfg.log 2>&1
make -j8 > make.log 2>&1 && make install >> make.log 2>&1
echo "ffi: $(ls /wk/ffi-inst/include 2>&1 | tr '\n' ' ')"
cd /wk/nat; : > build.log
FF="CPPFLAGS=-I/wk/ffi-inst/include LDFLAGS=-L/wk/ffi-inst/lib"
for d in glib gobject gmodule gthread; do make -k -j8 -C $d $FF >> build.log 2>&1; done; echo
echo "make rc=$?"
ls glib/.libs/libglib-2.0.a gobject/gmarshal.c gobject/gobject-enum-types.c 2>&1 | head -5
grep -n ' Error \| error:' build.log | head -8
