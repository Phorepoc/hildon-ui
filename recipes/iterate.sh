#!/usr/bin/env bash
# iterate.sh -- host (Git Bash), from the Symbian-ReverseEngineered checkout: repackage the test, relink, rebuild the ROM, boot it
# once in QEMU and print the [pheglib] lines. Needs the GLib objects from build_x32.sh in the hildon-wk volume.
set -u
H=$(cd "$(dirname "$0")/.." && pwd -W); R=${REPO:-C:/Users/memph/Git/Symbian-ReverseEngineered}
cd "$R"
MSYS_NO_PATHCONV=1 docker.exe run --rm --name hildon-pk -e ONLY="${ONLY:-}" -e ONLY_LIMIT_MS="${ONLY_LIMIT_MS:-}" -v "$H":/h -v "$R":/repo:ro -v "$R/.phorepoc-x86-build/hildon":/o -v "${VOL:-hildon-wk}":/wk phorepoc-x86 bash /h/recipes/package_x32.sh | tail -1; [ ${PIPESTATUS[0]} = 0 ] || { echo PACKAGE FAILED; exit 1; }
./ph build pheglib phegobject phegmodule phegthread phepng phegio phepixman phefreetype phehb phepng phefontconfig phecairo phepango phepixbuf phecairowin pheglibtest > .hildon-build.log 2>&1 || { tail -5 .hildon-build.log; exit 1; }
ROM_EXCLUDE=pheget,phestore,gsaccessoryplugin FAST=1 BOOT="!${BOOT_EXE:-pheglibtest}" SECONDARY_IMAGE=efile.exe bash tools/make_phorepoc_rom.sh > .hildon-rom.log 2>&1 || { echo ROM FAILED; exit 1; }
# the timing lock (VMLOCK=<path of vmlock.sh>) is held ONLY around the QEMU boot, released on any exit
if [ -n "${VMLOCK:-}" ]; then sh "$VMLOCK" acquire hildon; trap 'sh "$VMLOCK" release hildon' EXIT; fi
MEM=512 ./ph boot ${SECS:-45} > .hildon-boot.log 2>&1
if [ -n "${VMLOCK:-}" ]; then sh "$VMLOCK" release hildon; trap - EXIT; fi
tr -d '\000' < .phorepoc-x86-build/ekern/run.log | grep -a "pheglib\|phecairowin\|pango\|pixbuf\|harfbuzz\|CRASH\|PANIC\|Panic\|FAULT" | head -${N:-60} | cut -c1-230
