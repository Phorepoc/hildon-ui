#!/usr/bin/env bash
# libc_syms.sh -- IN phorepoc-x86 (/repo = Symbian-ReverseEngineered, :ro; /wk): the symbols Open C exports on x32 (libc, libm, libpthread, libdl
# of the main repo's x32 build) -> /wk/libc_syms.txt, the filter gen_config.py applies to GLib's HAVE_* macros.
set -eu
cd /repo/.phorepoc-x86-build/mmp-x32
for l in libc libm libpthread libdl; do nm --defined-only $l/$l.elf | awk '$2 ~ /[TWDBRVi]/ {print $3}'; done | sort -u > /wk/libc_syms.txt
echo "libc_syms: $(wc -l < /wk/libc_syms.txt) symbols"
