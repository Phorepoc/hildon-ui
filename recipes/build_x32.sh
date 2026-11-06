#!/usr/bin/env bash
# build_x32.sh -- IN phorepoc-x86 (volumes hildon-wk:/wk, repo:/repo:ro, this repo:/h): GLib objects for x32/Open C with pp-gcc
# (the wrappers must exist: mk_pure_wrappers.sh into /wk/lsc). Needs native_build.sh + gen_config.py output in /wk/x32cfg.
# Copies the native build tree to /wk/x32 (generated sources, Makefiles), swaps in the x32 config, rebuilds only objects.
set -u
CC=/wk/lsc/bin/pp-gcc
bash /h/recipes/patch_glib.sh
if [ ! -d /wk/x32 ]; then
  cp -a /wk/nat /wk/x32 && grep -rIl '/wk/nat' /wk/x32 --include=Makefile --include=libtool --include=config.status --include=*.h 2>/dev/null | xargs -r sed -i 's|/wk/nat|/wk/x32|g'
fi
cd /wk/x32
find . \( -name '*.o' -o -name '*.lo' -o -name '*.la' -o -name .libs \) -prune -exec rm -rf {} + 2>/dev/null
cp /wk/x32cfg/config.h config.h; cp /wk/x32cfg/glibconfig.h glib/glibconfig.h
touch config.status; find . -name Makefile -exec touch {} +; sleep 1; touch config.h glib/glibconfig.h stamp-h1 glib/stamp-gc-h glib/libcharset/config.h 2>/dev/null
FF="-I/wk/ffi-x32/include -I/h/overlay -include /h/overlay/phe_glib_compat.h"
CF="CFLAGS=-O2 -g0 -w -fno-pic $FF"
: > /wk/x32/objs.log
objs() { make -s -C $1 -f Makefile -f /h/recipes/print.mk P-$2; }
build() { # dir var
  local o; o=$(objs $1 $2)
  make -k -j${JOBS:-8} -C $1 CC=$CC "$CF" $o >> /wk/x32/objs.log 2>&1
  echo "$1/$2: $(echo $o | wc -w) objects, rc=$?"
}
build glib/libcharset libcharset_la_OBJECTS
build glib/gnulib libgnulib_la_OBJECTS
build glib/pcre libpcre_la_OBJECTS
build glib libglib_2_0_la_OBJECTS
build gobject libgobject_2_0_la_OBJECTS
build gmodule libgmodule_2_0_la_OBJECTS
build gthread libgthread_2_0_la_OBJECTS
grep -c 'rror' /wk/x32/objs.log
