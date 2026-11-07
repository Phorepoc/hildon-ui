#!/usr/bin/env bash
# build_cxxheaders_x32.sh -- IN phorepoc-x86 (volume at /wk, the MAIN repo mounted READ-WRITE at /repo: openc_cflags.py writes its cache there):
# the libstdc++ HEADERS (GCC 13.3 source, configured for x32 over Open C by the main repo's tools/phorepoc-x86/cxxrt/build_libstdcxx.sh -- its first 41 lines: unpack, the ph-g++ wrapper,
# configure, `make -C include`) into /wk/lsc/build/include. Only the header tree is wanted: HarfBuzz >= 4 (std::unique_ptr / std::function / <atomic> ...) and Pango's C++-free
# code do not need the libstdc++ ARCHIVE. Needs third_party/gcc/gcc-13.3.0.tar.xz in the main repo (gitignored there).
set -eu
mkdir -p /wk/lsc /wk/out/cxxrt
head -41 /repo/tools/phorepoc-x86/cxxrt/build_libstdcxx.sh | sed '1d' > /wk/ph_cxxhdr_part.sh
W=/wk/lsc OUT=/wk/out/cxxrt bash /wk/ph_cxxhdr_part.sh
cd /wk/lsc/build; make -C include > /wk/out/cxxrt/make-inc.log 2>&1 || tail -5 /wk/out/cxxrt/make-inc.log
ls /wk/lsc/build/include | head -3; ls /wk/lsc/build/include/x86_64-unknown-elf/bits | head -3
