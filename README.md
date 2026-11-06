# hildon-symbian

Groundwork for the Hildon UI libraries on Phorepoc OS (PC image, x86-64 / x32 ABI). Plan and findings:
`docs/phorepoc/hildon-port.md` in the Symbian-ReverseEngineered repo (section 7). Nothing here is in an OS image; Hildon is
meant to be a Catalogue download. Licences: our recipes BSD-2-Clause; GLib/libhildon LGPL-2.1+, libffi MIT.

## Decision
GLib **2.56.4** (+ libffi 3.4.6), built as app-private DLLs with new names (`phegl*`) so the ROM's old ossrv glib is never
touched. Evidence: GTK+ 2.24.33 needs GLib >= 2.28, Leste's libhildon code needs >= 2.32; 2.56 is the last autotools release.

## Status (2026-11-06, end of the first session): step 1 NOT finished
Done: sources pinned and verified (`recipes/pins.sh`, `fetch.sh`); GLib configured natively in the build image
(`recipes/native_config.sh`, works); the x32 pure-Open-C compiler wrappers (`pp-gcc`) generated and working in a private volume;
Open C's libc export list extracted (1,500 symbols) to drive the HAVE_* filtering.
NOT done: the x32 `config.h`/`glibconfig.h` (ILP32 sizes, gint64 = long long, HAVE_* filtered by Open C's exports, NLS off),
libffi x32, compiling the objects, the DLLs (.def with frozen ordinals), the test EXE, the QEMU boot log.
Next steps, in order:
1. `recipes/gen_config.py`: native config.h -> x32 (SIZEOF_* = 4 for long/size_t/void*, drop HAVE_* whose function is not in
   libc/libm/libpthread/libdl of `.phorepoc-x86-build/mmp-x32`, no `ENABLE_NLS`, no HAVE_POLL_H/eventfd/inotify/netlink/statfs);
   a glibconfig.h with i386-style gsize/gssize/gintptr (unsigned int), gint64 = long long ("ll").
2. libffi 3.4.6: compile by hand with pp-gcc (src/prep_cif.c types.c raw_api.c x86/ffi64.c ffiw64.c unix64.S; skip closures.c,
   GLib only needs ffi_call); ffi.h/fficonfig.h from a native configure.
3. Native full build first in `/wk/nat` (generates gmarshal.c, enum types; `make -C glib` builds the libcharset/gnulib/pcre
   subdirs before libglib), then compile the same object lists with `CC=pp-gcc` into archives.
4. Components in the ROM registry (`x32_archives`, like pheffmpeg_x32): `pheglib`, `phegobject`, `phegmodule`, `phegthread`
   with generated .def files; a test EXE `pheglibtest` (g_print, GMainLoop + timeout, GObject subclass + signal, GThread).
   Boot it with `SSM_TEST=pheglibtest` (see tools/phorepoc-x86/cxxrt/run_test.sh for the pattern) and quote the log lines.
