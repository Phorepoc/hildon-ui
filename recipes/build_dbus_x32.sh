#!/usr/bin/env bash
# build_dbus_x32.sh -- IN phorepoc-x86 (volume at /wk, repo :ro at /repo, this repo at /h): libdbus 1.16.2 (the client library plus the utility sources the daemon shares) for x32 over Open C, from the
# pinned tarball. The release is Meson-only: there is no configure, so the source lists are those of dbus/meson.build (unix branch, no systemd / selinux / launchd / epoll / traditional activation)
# and config.h is written here from what Open C provides. Transport on this system: TCP on the loopback (Open C has no unix domain sockets); authentication without peer credentials.
# Out: /wk/dbus-x32/*.o, /wk/out/libphedbus.a, /wk/dbus-x32/errors.txt, /wk/dbus-x32/undefined.txt (symbols the objects need that Open C / their own objects do not define)
set -eu
. /h/recipes/pins.sh
CC=/wk/lsc/bin/pp-gcc
cd /wk; [ -d dbus-$DBUS_VER ] || tar xf /h/dl/dbus-$DBUS_VER.tar.xz
S=/wk/dbus-$DBUS_VER; O=/wk/dbus-x32; mkdir -p $O/dbus
MAJ=${DBUS_VER%%.*}; R=${DBUS_VER#*.}; MIN=${R%%.*}; MIC=${R#*.}
cat > $O/config.h <<EOC
#define VERSION "$DBUS_VER"
#define DBUS_UNIX 1
#define DBUS_COMPILATION 1
#define DBUS_DAEMON_NAME "dbus-daemon"
#define DBUS_EXPORT
#define DBUS_PRIVATE_EXPORT
#define DBUS_DISABLE_ASSERT 1
#define DBUS_DISABLE_CHECKS 1
#define G_DISABLE_CHECKS 1
#define DBUS_PREFIX "z:/resource/dbus"
#define DBUS_BINDIR "z:/sys/bin"
#define DBUS_DATADIR "z:/resource/dbus"
#define DBUS_RUNSTATEDIR "c:/data/dbus"
#define DBUS_MACHINE_UUID_FILE "c:/data/dbus/machine-id"
#define DBUS_SYSTEM_CONFIG_FILE "z:/resource/dbus/system.conf"
#define DBUS_SESSION_CONFIG_FILE "z:/resource/dbus/session.conf"
#define DBUS_SYSTEM_BUS_DEFAULT_ADDRESS "tcp:host=127.0.0.1,port=5370"
#define DBUS_SESSION_BUS_CONNECT_ADDRESS "tcp:host=127.0.0.1,port=5371"
#define DBUS_SESSION_SOCKET_DIR "c:/data/dbus"
#define DBUS_USER "root"
#define DBUS_TEST_USER "root"
#define DBUS_DAEMONDIR "z:/sys/bin"
#define DBUS_EXEEXT ".exe"
#define TEST_LISTEN "tcp:host=127.0.0.1"
#define DBUS_TEST_SOCKET_DIR "c:/data/dbus"
#define HAVE_ERRNO_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_LOCALE_H 1
#define HAVE_SIGNAL_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_UNISTD_H 1
#define HAVE_DIRENT_H 1
#define HAVE_RAISE 1
#define HAVE_SETENV 1
#define HAVE_UNSETENV 1
#define HAVE_SETLOCALE 1
#define HAVE_LOCALECONV 1
#define HAVE_USLEEP 1
#define HAVE_SOCKLEN_T 1
#define HAVE_WRITEV 1
#define HAVE_DECL_MSG_NOSIGNAL 0
#define HAVE_DECL_LOG_PERROR 0
EOC
sed -e "s/@DBUS_INT64_TYPE@/long long/; s/@DBUS_INT64_MODIFIER@/ll/; s/@DBUS_INT64_CONSTANT@/val##LL/; s/@DBUS_UINT64_CONSTANT@/val##ULL/; s/@DBUS_INT32_TYPE@/int/; s/@DBUS_INT16_TYPE@/short/" \
    -e "s/@DBUS_SIZEOF_VOID_P@/4/; s/@DBUS_MAJOR_VERSION@/$MAJ/g; s/@DBUS_MINOR_VERSION@/$MIN/g; s/@DBUS_MICRO_VERSION@/$MIC/g; s/@DBUS_VERSION@/$DBUS_VER/g" $S/dbus/dbus-arch-deps.h.in > $O/dbus/dbus-arch-deps.h
LIB="dbus-address dbus-auth dbus-bus dbus-connection dbus-credentials dbus-errors dbus-keyring dbus-marshal-byteswap dbus-marshal-header dbus-marshal-recursive dbus-marshal-validate dbus-message dbus-misc dbus-nonce dbus-object-tree dbus-pending-call dbus-resources dbus-server-debug-pipe dbus-server-socket dbus-server dbus-sha dbus-signature dbus-syntax dbus-threads dbus-timeout dbus-transport-socket dbus-transport dbus-watch dbus-uuidgen dbus-server-unix"
SHARED="dbus-dataslot dbus-file dbus-hash dbus-internals dbus-list dbus-marshal-basic dbus-memory dbus-mempool dbus-pipe dbus-string dbus-sysdeps dbus-file-unix dbus-pipe-unix dbus-sysdeps-pthread dbus-sysdeps-unix dbus-transport-unix dbus-userdb"
UTIL="dbus-asv-util dbus-mainloop dbus-message-util dbus-pollable-set-poll dbus-pollable-set dbus-shell dbus-string-util dbus-sysdeps-util dbus-sysdeps-util-unix dbus-userdb-util"
FF="-O2 -g0 -w -fno-pic -I/h/overlay -include /h/overlay/phe_glib_compat.h -include /h/os/src/phedbus/phedbus_compat.h -DHAVE_CONFIG_H -DDBUS_COMPILATION -Ddbus_1_EXPORTS -I$O -I$O/dbus -I$S -I$S/dbus"
# (the build host defines __linux__; dbus-sysdeps-unix.c then insists on a Linux credentials mechanism, which a TCP-only system cannot have)
rm -f $O/*.o $O/errs.*; export CC FF O S
one() { f=$1; x=""; [ $f = dbus-sysdeps-unix ] && x="-U__linux__"; $CC $FF $x -c $S/dbus/$f.c -o $O/$f.o 2> $O/errs.$f || echo "FAIL $f"; }
export -f one
fails=$(echo $LIB $SHARED $UTIL | tr ' ' '\n' | xargs -P "${JOBS:-8}" -I{} bash -c 'one {}' | tee $O/failed.txt | wc -l)
cat $O/errs.* > $O/errors.txt 2>/dev/null || true; rm -f $O/errs.*
echo "libdbus: $(ls $O/*.o 2>/dev/null | wc -l) objects, $fails failed"; head -30 $O/failed.txt | tr '\n' ' '; echo
grep -a "error" $O/errors.txt | sed "s|$S/dbus/||" | cut -c1-190 | head -${ERRN:-30}
$CC $FF -c /h/os/src/phedbus/phedbus_posix.c -o $O/phedbus_posix.o 2>> $O/errors.txt || echo "FAIL phedbus_posix"
mkdir -p /wk/out; rm -f /wk/out/libphedbus.a; ar rcs /wk/out/libphedbus.a $O/*.o; echo "libphedbus.a: $(ar t /wk/out/libphedbus.a | wc -l) members"
nm -u $O/*.o | grep " U " | awk '{print $2}' | sort -u > $O/und_all.txt
nm --defined-only $O/*.o | awk 'NF==3{print $3}' | sort -u > $O/def_all.txt
comm -23 $O/und_all.txt $O/def_all.txt | grep -vx -f /wk/libc_syms.txt > $O/undefined.txt || true
echo "symbols needed that neither libdbus nor Open C define: $(wc -l < $O/undefined.txt)"; head -50 $O/undefined.txt | tr '\n' ' '
