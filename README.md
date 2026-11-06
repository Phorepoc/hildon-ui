# hildon-symbian

Groundwork for the Hildon UI libraries on Phorepoc OS (PC image, x86-64 / x32 ABI). Plan and findings:
`docs/phorepoc/hildon-port.md` in the Symbian-ReverseEngineered repo (section 7). Nothing here is in an OS image; Hildon is
meant to be a Catalogue download. Licences: our recipes BSD-2-Clause; GLib/libhildon LGPL-2.1+, libffi MIT.

## Decision
GLib **2.56.4** (+ libffi 3.4.6), to be split into app-private DLLs with new names (`phegl*`) so the ROM's old ossrv glib is never
touched. Evidence: GTK+ 2.24.33 needs GLib >= 2.28, Leste's libhildon code needs >= 2.32; 2.56 is the last autotools release.

## Status (2026-11-06): GLib 2.56.4 RUNS on the x32 PC image (static archives, console test EXE)
Proof: `test/boot-log-2026-11-06.txt` (QEMU boot log of `pheglibtest.exe`): g_print, GString, GMainLoop with a timeout source
(3 ticks), a GObject subclass with a signal, a GThread, GKeyFile, and GLib's own `glib/tests/hash.c` (first 13 cases OK).
Known open problems: (1) `g_regex_match` returns FALSE (bundled PCRE; config of glib/pcre not checked); (2) the test panics
KERN-EXEC 3 in hash.c `/hash/recursive-remove-all` (not the stack: 1 MB gives the same); (3) NOT yet split into DLLs (needs `.def`
files with frozen ordinals generated from the archives, registry entries `pheglib`...`phegthread`); (4) GIO is not built.
Open C facts found: `pipe()` fails with ENOENT, `socketpair()` EOPNOTSUPP, `clock_gettime()` fails at run time, no `poll.h`,
`alloca.h`, `libintl.h`, `pthread_rwlock_t`, `sig_atomic_t`. Workarounds: `overlay/` (compat header, libintl stub, an in-process
GWakeup + g_poll wrapper) and `recipes/patch_glib.sh` (x32 negative-addend fix in guniprop.c).

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
