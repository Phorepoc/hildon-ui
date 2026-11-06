#!/usr/bin/env bash
# iterate.sh -- host (Git Bash), from the Symbian-ReverseEngineered checkout: repackage the test, relink, rebuild the ROM, boot it
# once in QEMU and print the [pheglib] lines. Needs the GLib objects from build_x32.sh in the hildon-wk volume.
set -u
H=$(cd "$(dirname "$0")/.." && pwd -W); R=${REPO:-C:/Users/memph/Git/Symbian-ReverseEngineered}
cd "$R"
MSYS_NO_PATHCONV=1 docker.exe run --rm --name hildon-pk -v "$H":/h -v "$R":/repo:ro -v "$R/.phorepoc-x86-build/hildon":/o -v hildon-wk:/wk phorepoc-x86 bash /h/recipes/package_x32.sh | tail -1; [ ${PIPESTATUS[0]} = 0 ] || { echo PACKAGE FAILED; exit 1; }
./ph build pheglib phegobject phegmodule phegthread phegio pheglibtest > .hildon-build.log 2>&1 || { tail -5 .hildon-build.log; exit 1; }
ROM_EXCLUDE=pheget,phestore,gsaccessoryplugin FAST=1 BOOT='!pheglibtest' SECONDARY_IMAGE=efile.exe bash tools/make_phorepoc_rom.sh > .hildon-rom.log 2>&1 || { echo ROM FAILED; exit 1; }
MEM=512 ./ph boot ${SECS:-45} > .hildon-boot.log 2>&1
tr -d '\000' < .phorepoc-x86-build/ekern/run.log | grep -a "pheglib\|CRASH\|PANIC\|Panic\|FAULT" | head -${N:-60} | cut -c1-230
