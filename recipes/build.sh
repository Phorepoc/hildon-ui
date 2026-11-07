#!/usr/bin/env bash
# build.sh -- everything, from the pins to the booted proof, in one run. Host side, Git Bash, Docker Desktop up.
#
#   REPO=<Symbian-ReverseEngineered checkout>  bash recipes/build.sh [steps]
#
# steps (default: all, in order):  fetch  tools  native  config  objects  gio  freetype  pixman  cxxhdr  hb  zlibpng  fontconfig  cairo  pango  pixbuf  atk  gdk  gdkbackend  gtk  archives  defs  images  boot
#   fetch    download + verify the pinned tarballs (recipes/pins.sh)               -> dl/
#   tools    the x32 pure-Open-C compiler wrappers (pp-gcc) + Open C's export list -> volume
#   native   native configure + build of libffi and GLib (generated sources, ffi.h)
#   config   gen_config.py: the x32 config.h / glibconfig.h, filtered by Open C's exports
#   objects  patch GLib (patch_glib.sh), compile every object with pp-gcc, libffi by hand
#   gio      GIO: native build for the generated sources (overlay/pyshim), then the x32 objects (patch_gio.sh); needs `objects` first (build_x32.sh wipes every .o)
#   archives package_x32.sh: libpheglib.a ... libpheglibtest.a                      -> $REPO/.phorepoc-x86-build/hildon/
#   images   ./ph build: pheglib.dll phegobject.dll phegmodule.dll phegthread.dll pheglibtest.exe  (the registry keys of the main repo)
#   boot     ROM + one QEMU boot (iterate.sh); prints the [pheglib] log lines
# Not a step: gen_defs.py -- the .def files are FROZEN (os/src/*/*.def); regenerate only for a deliberate ordinal change.
# env: REPO (default C:/Users/memph/Git/Symbian-ReverseEngineered), VOL (docker volume, default hildon-wk), CLEAN=1 (drop the volume first),
#      JOBS (default 8). Needs in $REPO: .epoc32 (stdapis), the x32 build of libc/libm/libpthread/libdl (./ph closure libc), core's registry.
set -eu
# (the tools step mounts the main repo read-WRITE: openc_cflags.py writes .mmp-build there; every other step is read-only)
HERE=$(cd "$(dirname "$0")/.." && pwd -W); REPO=${REPO:-C:/Users/memph/Git/Symbian-ReverseEngineered}; VOL=${VOL:-hildon-wk}
STEPS=${*:-fetch tools native config objects gio freetype pixman cxxhdr hb zlibpng fontconfig cairo pango pixbuf atk gdk gdkbackend gtk archives defs images boot}
OUTD=$REPO/.phorepoc-x86-build/hildon; mkdir -p "$OUTD"
IMG=phorepoc-x86
dr() { MSYS_NO_PATHCONV=1 docker.exe run --rm --name "hildon-$1" -e JOBS="${JOBS:-8}" -v "$HERE":/h -v "$REPO":/repo:ro -v "$OUTD":/o -v "$VOL":/wk $IMG "${@:2}"; }
has() { case " $STEPS " in *" $1 "*) return 0;; esac; return 1; }
[ "${CLEAN:-0}" = 1 ] && { docker.exe volume rm "$VOL" >/dev/null 2>&1 || true; }
docker.exe volume create "$VOL" >/dev/null
has fetch    && { echo "== fetch";    bash "$HERE/recipes/fetch.sh"; }
has tools    && { echo "== tools";    MSYS_NO_PATHCONV=1 docker.exe run --rm --name hildon-w1 -v "$HERE":/h -v "$REPO":/repo -v "$VOL":/wk $IMG bash -c 'W=/wk/lsc bash /repo/tools/phorepoc-x86/cxxrt/mk_pure_wrappers.sh >/dev/null; grep -q nostdinc /wk/lsc/bin/pp-gcc || { echo "pp-gcc wrapper not written"; exit 1; }; bash /h/recipes/libc_syms.sh'; }
has native   && { echo "== native";   dr n1 bash -c 'bash /h/recipes/native_config.sh && bash /h/recipes/native_build.sh'; }
has config   && { echo "== config";   dr c1 bash -c 'cd /wk/nat && cp -f config.h config.h.native; cp -f glib/glibconfig.h glibconfig.h.native; python3 /h/recipes/gen_config.py config.h.native glibconfig.h.native /wk/libc_syms.txt /repo/.epoc32/include/stdapis /wk/x32cfg | cut -c1-80'; }
has objects  && { echo "== objects";  dr o1 bash -c 'mkdir -p /wk/ffi-x32/include /wk/out; cp /wk/ffi-inst/include/*.h /wk/ffi-x32/include/; bash /h/recipes/build_x32.sh && bash /h/recipes/build_ffi_x32.sh'; }
has gio      && { echo "== gio";      dr g1 bash /h/recipes/build_gio_x32.sh | tail -6; }
has freetype && { echo "== freetype"; dr f1 bash /h/recipes/build_freetype_x32.sh | tail -3; }
has pixman   && { echo "== pixman";   dr p1 bash /h/recipes/build_pixman_x32.sh | tail -8; }
has cxxhdr   && { echo "== cxxhdr";   MSYS_NO_PATHCONV=1 docker.exe run --rm --name hildon-x1 -v "$HERE":/h -v "$REPO":/repo -v "$VOL":/wk $IMG bash /h/recipes/build_cxxheaders_x32.sh | tail -3; }
has hb       && { echo "== harfbuzz"; dr h1 bash /h/recipes/build_harfbuzz_x32.sh | tail -3; }
has zlibpng  && { echo "== zlib+png"; dr z1 bash /h/recipes/build_zlib_png_x32.sh | tail -3; }
has fontconfig && { echo "== fontconfig"; dr fc1 bash /h/recipes/build_fontconfig_x32.sh | tail -4; }
has cairo    && { echo "== cairo";    dr c2 bash /h/recipes/build_cairo_x32.sh | tail -8; }
has archives && { echo "== archives"; dr a1 bash /h/recipes/package_x32.sh; }
has pango    && { echo "== pango";    dr pg1 bash /h/recipes/build_pango_x32.sh | tail -6; }
has gdk      && { echo "== gdk core";  dr gd1 bash /h/recipes/build_gdk_x32.sh | tail -3; }
has atk       && { echo "== atk";        dr atk bash /h/recipes/build_atk_x32.sh | tail -25; }
has gdkbackend && { echo "== gdk backend"; dr gd2 bash /h/recipes/build_gdk_backend_x32.sh | tail -4; }
has gtk       && { echo "== gtk";        dr gtk bash /h/recipes/build_gtk_x32.sh | tail -12; }
has pixbuf   && { echo "== pixbuf";   dr px1 bash /h/recipes/build_pixbuf_x32.sh | tail -4; }
has defs     && { echo "== defs";     dr d1 python3 /h/recipes/gen_defs.py /o /h/os/src; }
has images   && { echo "== images";   ( cd "$REPO" && ./ph build pheglib phegobject phegmodule phegthread phepng phegio phepixman phefreetype phehb phepng phefontconfig phecairo phepango phepixbuf phegdk phegdkwin pheglibtest ) | tail -3; }
has boot     && { echo "== boot";     ( cd "$REPO" && bash "$HERE/recipes/iterate.sh" ); }
echo "build.sh: done ($STEPS)"
