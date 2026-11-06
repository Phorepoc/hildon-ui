#!/usr/bin/env bash
# build_gio_x32.sh -- IN phorepoc-x86 (volumes hildon-wk:/wk, repo :ro at /repo, this repo at /h): GIO of GLib 2.56.4 for x32/Open C.
# Needs build.sh's steps up to `objects` (the native tree /wk/nat, the x32 tree /wk/x32, the wrappers). 1) native `make -C gio` for the generated
# sources (gdbus-codegen needs overlay/pyshim: no distutils in Python 3.12; CFLAGS=-w: gcc 13 -Werror=format-overflow), 2) copy gio into the x32
# tree with the x32 config, 3) compile every libgio object with pp-gcc.
set -u
CC=/wk/lsc/bin/pp-gcc
FF="-I/wk/ffi-x32/include -I/h/overlay -include /h/overlay/phe_glib_compat.h"
cd /wk/nat; export PYTHONPATH=/h/overlay/pyshim
make -k -j${JOBS:-8} -C gio CFLAGS="-O0 -w" CPPFLAGS=-I/wk/ffi-inst/include LDFLAGS=-L/wk/ffi-inst/lib > /wk/nat/gio-native.log 2>&1
echo "native gio: $(ls /wk/nat/gio/*.lo 2>/dev/null | wc -l) objects"
rm -rf /wk/x32/gio; cp -a /wk/nat/gio /wk/x32/gio
grep -rIl '/wk/nat' /wk/x32/gio --include=Makefile --include=*.h 2>/dev/null | xargs -r sed -i 's|/wk/nat|/wk/x32|g'
cd /wk/x32; find gio \( -name '*.o' -o -name '*.lo' -o -name '*.la' -o -name .libs \) -prune -exec rm -rf {} + 2>/dev/null
touch config.status; find gio -name Makefile -exec touch {} +; sleep 1; touch config.h glib/glibconfig.h stamp-h1 glib/stamp-gc-h
: > /wk/x32/gio-objs.log
for v in libgio_2_0_la_OBJECTS; do
  o=$(make -s -C gio -f Makefile -f /h/recipes/print.mk P-$v)
  make -k -j${JOBS:-8} -C gio CC=$CC "CFLAGS=-O2 -g0 -w -fno-pic $FF" $o >> /wk/x32/gio-objs.log 2>&1
  echo "gio/$v: $(echo $o | wc -w) objects"
done
echo "errors: $(grep -ac ' error' /wk/x32/gio-objs.log)"; grep -a ' error' /wk/x32/gio-objs.log | sed 's/.*glib-2.56.4.//' | cut -c1-150 | sort | uniq -c | sort -rn | head -15
