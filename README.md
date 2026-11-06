# hildon-symbian

Groundwork for the Hildon UI libraries on Phorepoc OS (PC image, x86-64 / x32 ABI). Plan and findings:
`docs/phorepoc/hildon-port.md` in the Symbian-ReverseEngineered repo (section 7). Nothing here is in an OS image; Hildon is
meant to be a Catalogue download. Licences: our recipes BSD-2-Clause; GLib/libhildon LGPL-2.1+, libffi MIT.

## Decision
GLib **2.56.4** (+ libffi 3.4.6), to be split into app-private DLLs with new names (`phegl*`) so the ROM's old ossrv glib is never
touched. Evidence: GTK+ 2.24.33 needs GLib >= 2.28, Leste's libhildon code needs >= 2.32; 2.56 is the last autotools release.

## Status (2026-11-06, second session): GLib 2.56.4 runs from FOUR DLLs on the x32 PC image
`pheglib.dll`, `phegobject.dll` (+libffi), `phegmodule.dll`, `phegthread.dll` (new names, frozen-ordinal `.def` from `recipes/gen_defs.py`;
the ROM's old ossrv glib is untouched) and the console EXE `pheglibtest` linking them. Proof: `test/boot-log-2026-11-06.txt`: g_print, GString,
GMainLoop + timeout, GObject subclass + signal, GThread, GKeyFile, GRegex (PCRE works; the earlier "failure" was a backslash lost in the test source),
then glib/tests/hash.c: 12 cases OK, then `/hash/recursive-remove-all` aborts.
That abort is a REAL limit, not a bug: `g_test_trap_subprocess()` needs pipe() + process spawn. Open C pipe() = `RPipe::Create` (kernel pipe
device) fails ENOENT on our kernel, socketpair() EOPNOTSUPP; fork/exec do not exist (stubbed to ENOSYS in `overlay/phe_posix_stubs.c`; `posix_spawn`
is exported by libc and could back a g_spawn replacement once pipes exist). Starting the user-side LibCIpcServer does not help (tried).
Fixes found by running: `STRERROR_R_CHAR_P` (glibc-only) made g_strerror return NULL; guniprop.c negative reloc addends (2 places);
Open C `clock_gettime()` supports ONLY CLOCK_REALTIME and fills tv_nsec with microseconds (libc/src/clock_funcs.cpp), so GLib uses gettimeofday via the compat header.
Open: pipe()/spawn (kernel RPipe device), GIO, SIS packaging (`package.conf` is a draft; needs the pdk to resolve `.hildon/` and merge `registry/components_hildon.py`).

## Build (all in the phorepoc-x86 image, private volume `hildon-wk` at /wk; see each script's header)
1. `recipes/fetch.sh` (host) -- pinned tarballs into dl/.
2. `mk_pure_wrappers.sh` of the main repo (W=/wk/lsc) -- the x32 pure-Open-C `pp-gcc`.
3. `recipes/native_config.sh`, `recipes/native_build.sh` -- native GLib/libffi build: generated sources + ffi.h.
4. `gen_config.py` -- native config.h/glibconfig.h -> x32 (filtered by the Open C export list /wk/libc_syms.txt, nm of libc/libm/libpthread/libdl).
5. `recipes/build_x32.sh` (patches GLib, copies the tree to /wk/x32, compiles every object with pp-gcc), `build_ffi_x32.sh`, `package_x32.sh` -> `.phorepoc-x86-build/hildon/*.a`.
6. Registry entry `pheglibtest` (emitter/components.py of Symbian-ReverseEngineered) + `recipes/iterate.sh` (link, ROM, QEMU boot, print log).
   Note: in this checkout the ROM build needed `ROM_EXCLUDE=pheget,phestore,gsaccessoryplugin` (registry points at components core moved away /
   a missing acccontrol.dll) and the boot `MEM=512` (the ROM is too big for GRUB at 128 MB).

## Next steps
1. Find the hash.c crash (log the faulting address: `./ph sym`), fix PCRE/regex, run more of glib/tests (list, queue, array, strfuncs, keyfile, mainloop).
2. DLL split: generate .def (nm of the archives) -> `pheglib.dll`, `phegobject.dll`, `phegmodule.dll`, `phegthread.dll`; the registry side
   (`x32_archives` + `def`) is the `pheffmpeg_x32` pattern; the layer/pdk side (components_hildon.py in this repo + package.conf) mirrors codecpack.
3. GIO (GFile etc.: libhildon/home use `g_file_*`), then step 2 of the plan (pixman, cairo, pango).
