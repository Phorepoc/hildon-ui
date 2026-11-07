#!/usr/bin/env bash
# build_dbusglib_x32.sh -- IN phorepoc-x86 (volume at /wk, repo :ro at /repo, this repo at /h): dbus-glib 0.114 (the GObject / GLib main loop binding of libdbus: DBusGConnection, DBusGProxy,
# dbus_g_bus_get, dbus_g_connection_setup_with_main_context ...) for x32 over Open C, from the pinned tarball, on the libdbus of build_dbus_x32.sh. No configure: the source list is the one of
# dbus/Makefile.am + dbus-gmain/Makefile.am (the generated dbus-gmarshal.[ch] ship in the tarball; the binding tool and the tests are not built).
# Out: /wk/dbusglib-x32/*.o, /wk/out/libphedbusglib.a, /wk/dbusglib-x32/undefined.txt
set -eu
. /h/recipes/pins.sh
CC=/wk/lsc/bin/pp-gcc
cd /wk; [ -d dbus-glib-$DBUSGLIB_VER ] || tar xzf /h/dl/dbus-glib-$DBUSGLIB_VER.tar.gz
S=/wk/dbus-glib-$DBUSGLIB_VER; O=/wk/dbusglib-x32; mkdir -p $O /wk/out
# <dbus/dbus.h> style includes of libdbus: its public headers and the generated dbus-arch-deps.h in one directory
mkdir -p $O/inc/dbus; cp /wk/dbus-1.16.2/dbus/dbus*.h /wk/dbus-x32/dbus/dbus-arch-deps.h $O/inc/dbus/
cat > $O/config.h <<EOC
#define HAVE_SOCKLEN_T 1
#define G_DISABLE_ASSERT 1
#define G_DISABLE_CHECKS 1
#define DBUS_COMPILATION 1
#define VERSION "$DBUSGLIB_VER"
#define PACKAGE_VERSION "$DBUSGLIB_VER"
#define GLIB_VERSION_MIN_REQUIRED GLIB_VERSION_2_32
#define GLIB_VERSION_MAX_ALLOWED GLIB_VERSION_2_40
EOC
SRC="dbus/dbus-glib dbus/dbus-gmarshal dbus/dbus-gobject dbus/dbus-gproxy dbus/dbus-gvalue dbus/dbus-gvalue-parse-variant dbus/dbus-gthread dbus/dbus-gtype-specialized dbus/dbus-gutils dbus/dbus-gsignature dbus/dbus-gvalue-utils dbus-gmain/dbus-gmain"
X32="-I/wk/x32 -I/wk/x32/glib -I/wk/glib-2.56.4 -I/wk/glib-2.56.4/glib -I/wk/glib-2.56.4/gobject -I/wk/glib-2.56.4/gmodule -I/wk/glib-2.56.4/gio -I/wk/ffi-x32/include"
FF="-O2 -g0 -w -fno-pic -I/h/overlay -include /h/overlay/phe_glib_compat.h -include /h/os/src/phedbus/phedbus_compat.h -DHAVE_CONFIG_H -DDBUS_API_SUBJECT_TO_CHANGE -Ddbus_gmain_set_up_connection=_dbus_g_set_up_connection -Ddbus_gmain_set_up_server=_dbus_g_set_up_server -I$O -I$O/inc -I$S -I$S/dbus -I$S/dbus-gmain $X32"
rm -f $O/*.o $O/errs.*; export CC FF O S
one() { f=$1; b=$(basename $f); $CC $FF -c $S/$f.c -o $O/$b.o 2> $O/errs.$b || echo "FAIL $b"; }
export -f one
fails=$(echo $SRC | tr ' ' '\n' | xargs -P "${JOBS:-8}" -I{} bash -c 'one {}' | tee $O/failed.txt | wc -l)
cat $O/errs.* > $O/errors.txt 2>/dev/null || true; rm -f $O/errs.*
echo "dbus-glib: $(ls $O/*.o 2>/dev/null | wc -l) objects, $fails failed"; head -30 $O/failed.txt | tr '\n' ' '; echo
grep -a "error" $O/errors.txt | sed "s|$S/||" | cut -c1-190 | head -${ERRN:-30}
rm -f /wk/out/libphedbusglib.a; ar rcs /wk/out/libphedbusglib.a $O/*.o; echo "libphedbusglib.a: $(ar t /wk/out/libphedbusglib.a | wc -l) members"
GO=$(ls /wk/dbus-x32/*.o)
nm -u $O/*.o | grep " U " | awk '{print $2}' | sort -u > $O/und_all.txt
nm --defined-only $O/*.o $GO | awk 'NF==3{print $3}' | sort -u > $O/def_all.txt
comm -23 $O/und_all.txt $O/def_all.txt | grep -E "^(_?dbus_|dbus_g|_dbus)" > $O/undefined.txt || true
echo "undefined dbus symbols: $(wc -l < $O/undefined.txt)"; head -40 $O/undefined.txt | tr '\n' ' '
