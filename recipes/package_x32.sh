#!/usr/bin/env bash
# package_x32.sh -- IN phorepoc-x86 (/wk, repo:/repo:ro, this repo:/h, output dir :/o): archive the GLib objects + libffi + the test
# into /o/{libpheglib,libphegobject,libphegmodule,libphegthread,libpheffi,libpheglibtest}.a
set -u
CC=/wk/lsc/bin/pp-gcc; X=/wk/x32; S=/wk/glib-2.56.4; O=/o; mkdir -p $O /wk/out/t
INC="-I$X -I$X/glib -I$S -I$S/glib -I$S/gobject -I$S/gmodule -I/wk/ffi-x32 -I/h/overlay -include /h/overlay/phe_glib_compat.h"
CF="-O2 -g0 -w -fno-pic -DHAVE_CONFIG_H -DGLIB_STATIC_COMPILATION -DGOBJECT_STATIC_COMPILATION"
$CC $CF $INC -c /h/overlay/phe_posix_stubs.c -o /wk/out/phe_posix_stubs.o || exit 1
$CC $CF $INC -c /h/os/src/pheglibtest/pheglibtest.c -o /wk/out/t/pheglibtest.o || exit 1
for t in hash:hash list:list slist:slist queue:queue array:array-test strfuncs:strfuncs keyfile:keyfile mainloop:mainloop; do
  n=${t%%:*}; f=${t##*:}; $CC $CF $INC -I$S/glib/tests -Dmain=phe_glib_test_$n -Dg_test_init=phe_g_test_init -DG_LOG_DOMAIN=\"GLib\" -c $S/glib/tests/$f.c -o /wk/out/t/t_$n.o 2>&1 | grep -a " error" | head -3; [ -f /wk/out/t/t_$n.o ] || echo "NOT BUILT: $n"
done
mkdir -p $O/testdata; cp $S/glib/tests/keyfiletest.ini $S/glib/tests/pages.ini $S/glib/tests/keyfile.c $O/testdata/   # data files of keyfile.c (rom_data of pheglibtest)
mk() { local a=$1; shift; rm -f $O/$a; ar rcs $O/$a "$@"; echo "$a: $(ar t $O/$a | wc -l) members"; }
mk libpheglib.a /wk/out/phe_posix_stubs.o $X/glib/.libs/*.o $X/glib/deprecated/.libs/*.o $X/glib/libcharset/.libs/*.o $X/glib/gnulib/.libs/*.o $X/glib/pcre/.libs/*.o
mk libphegobject.a $X/gobject/.libs/*.o
mk libphegmodule.a $X/gmodule/.libs/*.o
mk libphegthread.a $X/gthread/.libs/*.o
mk libpheffi.a $(ls /wk/out/ffi/*.o | grep -v win64)
[ -d $X/gio/.libs ] && mk libphegio.a $X/gio/.libs/*.o $X/gio/xdgmime/.libs/*.o $X/gio/gvdb/.libs/*.o
mk libpheglibtest.a /wk/out/t/*.o
