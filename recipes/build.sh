#!/usr/bin/env bash
# build.sh -- libhildon 2.2 + libhildondesktop 2.1 on GTK (needs glib-symbian, gtk-symbian and dbus-symbian built into the same VOL). Host side, Git Bash, Docker Desktop up.
#
#   REPO=<Symbian-ReverseEngineered checkout>  bash recipes/build.sh [steps]
# steps (default: all, in order): hildon hildondesktop archives defs images boot
# The sibling repositories (glib-symbian, gtk-symbian, dbus-symbian, hildon-ui) are expected next to this one; they are mounted at /glib /gtk /dbus (read-only) in every container,
# this repository at /h. The docker volume (VOL) is the shared build tree of the whole stack; OUTD (the archives) is $REPO/.phorepoc-x86-build/hildon.
set -eu
HERE=$(cd "$(dirname "$0")/.." && pwd -W); REPO=${REPO:-C:/Users/memph/Git/Symbian-ReverseEngineered}; VOL=${VOL:-hildon-wk}; PARENT=${HERE%/*}; SIB="-v $PARENT/glib-symbian:/glib:ro -v $PARENT/gtk-symbian:/gtk:ro -v $PARENT/dbus-symbian:/dbus:ro"
STEPS=${*:-hildon hildondesktop hildonhome archives defs images boot}
OUTD=$REPO/.phorepoc-x86-build/hildon; mkdir -p "$OUTD"
IMG=phorepoc-x86
dr() { MSYS_NO_PATHCONV=1 docker.exe run --rm --name "hildon-$1" -e JOBS="${JOBS:-8}" -v "$HERE":/h $SIB -v "$REPO":/repo:ro -v "$OUTD":/o -v "$VOL":/wk $IMG "${@:2}"; }
has() { case " $STEPS " in *" $1 "*) return 0;; esac; return 1; }
[ "${CLEAN:-0}" = 1 ] && { docker.exe volume rm "$VOL" >/dev/null 2>&1 || true; }
docker.exe volume create "$VOL" >/dev/null
has hildon    && { echo "== hildon";     dr hl bash /h/recipes/build_hildon_x32.sh | tail -12; }
has hildondesktop && { echo "== hildondesktop"; dr hd bash /h/recipes/build_hildondesktop_x32.sh | tail -10; }
has hildonhome && { echo "== hildonhome"; dr hh bash /h/recipes/build_hildonhome_x32.sh | tail -8; }
has archives && { echo "== archives"; dr a1 bash /h/recipes/package_x32.sh; }
has defs     && { echo "== defs";     dr d1 python3 /h/recipes/gen_defs.py /o /h/os/src; }
has boot     && { echo "== boot";     ( cd "$REPO" && bash "$HERE/recipes/iterate.sh" ); }
has images   && { echo "== images";   ( cd "$REPO" && ./ph build phehildon phehildondesktop phehildonwin phehildondesktoptest ) | tail -3; }
echo "build.sh: done ($STEPS)"
