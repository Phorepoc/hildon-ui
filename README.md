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

## Build
One command, from the pins to the booted proof (host Git Bash, Docker up; `REPO` = the Symbian-ReverseEngineered checkout):

    REPO=C:/Users/memph/Git/Symbian-ReverseEngineered bash recipes/build.sh        # steps: fetch tools native config objects archives images boot

Each step is a script in `recipes/` (headers explain them); `VOL=<docker volume>` and `CLEAN=1` give a fresh, private tree. Notes: in this checkout
the ROM build needs `ROM_EXCLUDE=pheget,phestore,gsaccessoryplugin` and the boot `MEM=512` (`iterate.sh` does both). `pipe()` needs the pipe LDD in
the kernel: `tools/phorepoc-x86/ekern/xk_pipe.cpp` + `make_ekern.sh` + `xboot.cpp` case 7 + `--builtin-device pipelib.ldd=ldd:7` in the main repo.

## Third/fourth session results (2026-11-06)
- `pipe()` works (kernel pipe LDD): `probe pipe roundtrip: wrote=3 select(data)=1 read=3 [abc] select(empty)=0` (`test/boot-log-spawn.txt`).
- `g_spawn` over `posix_spawn` (`overlay/phe_fork_exec.c`, applied by `patch_glib.sh`): `posix_spawn("pheglibtest.exe", argv)` starts a real child that sees its argv
  (`child started: argc=3 argv1=--phe-run argv2=list`); the child ran glib/tests/list.c (17 cases OK) and spawned a grandchild for `/list/double-free/subprocess`,
  which ran and exited (GLib printed "corrupted double-linked list detected", `Main Killed 1`). OPEN: the waiting side never returns -- neither `waitpid()` nor
  GLib's g_spawn_sync/g_test_trap pipe-EOF + child-watch path completes after the child exited, so the spawn tests hang (the parent run is cut by the boot timeout).
  Next probe: waitpid on a finished child, and whether the parent's copies of the pipe write ends are really closed (Open C pipe handle ownership).
- The other test programs (hash, slist, queue, array, strfuncs, keyfile, mainloop) are compiled in and dispatched (`--phe-run <name>`) but have not run yet because the
  run is serial and stops at the hang. keyfile needs data files (G_TEST_SRCDIR) that are not in the ROM.

## Why a finished child is not reaped (analysis, 2026-11-06, partly from Open C source)
- Open C `waitpid(pid)` (backend ftable.cpp `CFileTable::waitpid`) is process-exit based: `RProcess::Logon` on the child, status = exit reason << 8 | exit type; WNOHANG returns 0 while the child lives. It does not depend on SIGCHLD.
- GLib's `GChildWatchSource` (gmain.c) DOES depend on SIGCHLD (dispatch_unix_signals_unlocked), which Open C never delivers: fixed in `patch_glib.sh` (prepare/check poll `waitpid(WNOHANG)` every 20 ms). That alone did not unblock the test.
- Thread dump (DUMP=90) after the hang: top-level process Main in waitpid/WaitForRequest; the list child's Main polling (alive, in a request wait); one GLib worker thread on a condition variable; the grandchild is GONE (`Main Killed 1`). So the exit is not the missing piece: `wait_for_child` (gtestutils.c) finishes only when the child exited AND both pipe read ends hit EOF. Leading hypothesis: the pipe write end is still held after the child died (Open C pipe handle duplicated into the child by posix_spawn file actions / `TransferToChild`, parent-side `close()` not releasing the kernel pipe end), so no EOF. Check next: in a standalone probe, spawn a child that exits, close the parent write end, `read()` the pipe -- EOF or hang? Derive from `RPipe` ownership in kernelhwsrv `dpipe.cpp` (a pipe end is released when its last handle closes) and `CFileTable::TransferToChild`.

## Pipe EOF -- derived from source, confirmed by one boot (2026-11-06)
- Open C `CPipeReadDesc::Poll` (backend upipedesc.cpp) reports a pipe whose write end is closed and which is empty ONLY as an exception (`EAnyException`);
  a `select()` with only readfds never wakes at EOF, and a non-blocking `read()` at EOF returns EAGAIN (`DoRead` maps 0 to KErrWouldBlock; the blocking read returns 0).
  Probe: `probe pipe-EOF: spawn=0 waitpid=38 status=0 | select(read only)=0 readable=0 | select(read+except)=1 bits=2 | read=-1 errno=35`.
- Fix (`patch_glib.sh`, PHE_XSET): GLib's select-based `g_poll` also watches every fd in the exception set and reports it as `G_IO_IN|G_IO_HUP` (a read then returns 0).
  Result: `glib/tests/list.c` now runs ALL cases in the child, including the `g_test_trap_subprocess` case (`/list/double-free`) and `/list/sort/stable`, then the list child exits.
- OPEN: the top-level `g_spawn_sync` in the test harness does not return after the list child exited (thread dump: one pheglibtest process left, its Main in a request wait). Next: log
  before/after the read loop and the final `waitpid` in `g_spawn_sync` (gspawn.c) -- one boot.

## Result of the glib test programs as child processes (2026-11-06, one boot)
```
glib/tests/queue:    exit_status=0    glib/tests/slist: exit_status=0
glib/tests/array:    exit_status=0    glib/tests/list:  exit_status=0
glib/tests/hash:     exit_status=0 (incl. /hash/recursive-remove-all, a g_test_trap_subprocess grandchild)
glib/tests/strfuncs: exit_status=770 (KERN-EXEC 3 abort during /strfuncs/strcanon -- not yet analysed)
glib/tests/keyfile:  exit_status=256 (test_load: data files (G_TEST_SRCDIR) are not in the ROM -- environment, then abort)
glib/tests/mainloop: exit_status=770 (KERN-EXEC 3 abort in /maincontext/basic -- not yet analysed)
glib tests: 3 of 8 programs not clean   DONE fails=3
```
`test/boot-log-spawn.txt` has the lines. The last blocker (`g_spawn_sync` never returning) was in gspawn.c itself: `g_spawn_sync` does a plain `select()` on the read ends with
no timeout (not `g_poll`), which at pipe EOF never wakes on Open C; `patch_glib.sh` (PHE_XSPAWN) adds the exception set. With that, hash/list/slist/queue/array pass.
GIO: `recipes/build_gio_x32.sh` is written (native gio with overlay/pyshim + CFLAGS=-w, copy into the x32 tree, pp-gcc objects) but NOT run yet.

## Next steps
1. strfuncs (/strfuncs/strcanon abort) and mainloop (/maincontext/basic abort): find the failing assertion (child stderr is captured by g_spawn_sync: the harness logs it; GLib messages in an aborting child go to the log handler first).
2. SIS packaging (recipes/sis.sh, pdk layer wiring).
3. GIO (GFile etc.: libhildon/home use `g_file_*`), then step 2 of the plan (pixman, cairo, pango).

## GIO scouting (not built yet)
Native `make -C gio` in the build tree gets 206 objects through. Two blockers, both with known fixes: `gdbus-codegen` needs Python's `distutils`
(removed in 3.12): `PYTHONPATH=overlay/pyshim` provides the one class it uses; and gcc 13's `-Werror=format-overflow` trips on gdbusauth.c /
gdbusmessage.c: pass `CFLAGS=-w`. Then the x32 pass is the same as for GLib (objects with pp-gcc from the native tree). Expect to need in the config:
no inotify/FAM (HAVE_INOTIFY dropped: file monitors fall back to none), zlib from Open C (`-lz` is in libz.a of the main repo's cxxrt/web build), GIO's
unix sockets/fd-passing code compiled but non-functional (socketpair/SCM_RIGHTS absent).
