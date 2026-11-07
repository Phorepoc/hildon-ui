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

## Sixth session (2026-11-06)
- Fatal GLib messages now reach the serial log (`phe_g_test_init` in pheglibtest.c wraps g_test_init: a log handler writes `GLOG[domain/level] message` to RDebug, then calls GLib's). Named failures: `/strfuncs/strcanon` and `/maincontext/basic` were NOT GLib bugs: the test programs must be compiled with `-DG_LOG_DOMAIN=\"GLib\"` (glib/tests/Makefile.am does) or `g_test_expect_message()` never matches the library's critical -- fixed in package_x32.sh. strcanon now passes; strfuncs then stops at `/strfuncs/strtod` (strfuncs.c:1016: `g_ascii_strtod (g_ascii_dtostr (d))` is not exactly `d`: Open C's printf/strtod double round trip -- open, needs a probe of snprintf %.17g and strtod).
- keyfile: its data files are in the test ROM (`rom_data` -> `z:/resource/pheglibtest`, `G_TEST_SRCDIR` set): the load tests now pass; `test_save` fails at `fd != -1` (g_file_open_tmp: no writable temp dir; TMPDIR=c:/ did not help -- needs the real Open C mkstemp/C: path check).
- mainloop: its run did not complete inside the 420 s boot window (not yet judged). list regressed in the first run with the wrapper (stderr of the double-free grandchild no longer matched) and was not re-run in the last, shorter window: re-run it.
- GIO: `recipes/build_gio_x32.sh` + `patch_gio.sh`: ALL 208 libgio objects now compile for x32 (errors: 0) after: `-U__linux__` (credentials code), `sys/statvfs.h` + `sys/sysmacros.h` overlay stand-ins, ENOSYS statvfs/fstatvfs, in6addr constants, empty mount-table functions, netlink monitor compiled out, `environ` macro undefined in gapplicationcommandline.c. NOT done: the GIO subdirectory libraries (xdgmime, inotify, giounix pieces of libgio_2_0_la_LIBADD), archives, DLL (`.def`), and a run.

## Seventh session (2026-11-06): GIO runs; Open C findings
- **GIO as `phegio.dll`** (217 objects incl. xdgmime, zlib linked in; 2,076 exports; registry key `phegio`, `recipes/build_gio_x32.sh` + `patch_gio.sh`, `package_x32.sh`, `gen_defs.py`): from the test EXE, `GFile` replace_contents / load_contents / query_info / delete and a `GKeyFile` saved and reloaded through a file all work:
  `gio: replace_contents=1`, `load_contents=1 len=9 [hello gio]`, `query_info name=phe_gio_test.txt size=9 type=1`, `GKeyFile through a file: load=1 key=[value] n=42`, `delete ok=1 exists_after=0` (`test/boot-log-gio.txt`).
  Needed on top of the compile fixes: `g_path_is_absolute` / `g_path_skip_root` treat a drive-letter prefix (`c:/`) as a root (PHE_DRIVE in `patch_glib.sh`; without it GFile made `/c:/data/x`: Invalid argument); `fnmatch()` (Open C has none; small one in `phe_posix_stubs.c`).
- **Open C facts from the probes:** `open()` works for `c:/` and `c:/data/` but not for `/tmp/...` or `c:/private/...` (EACCES); GLib's tmp dir is `C:Privatee7015771	mp` (does not exist) so `g_file_open_tmp` fails there; `mkstemp("c:/x_XXXXXX")` works. The strtod failure in strfuncs is a SUBNORMAL one: libc `%.17g` and GLib's own printf print identical strings for DBL_MAX, 0.1, 1e300 and they all round-trip, but `pow(2,-1024.1)` (`1.3720453440777821e-308`) does not parse back to the same double with libc `strtod` (so neither with `g_ascii_strtod`): an Open C strtod bug in the subnormal range (not fixed; a GLib-side workaround would be a correctly rounded strtod for |x| < DBL_MIN).
- Test state: queue, array, slist, hash exit 0 (some runs were cut by the 450 s boot window); list passed without the log wrapper and failed (stderr pattern) with it -- the wrapper is now installed only for strfuncs/keyfile/mainloop; strfuncs fails at `/strfuncs/strtod` (above); keyfile: TMPDIR set to `c:/data`, run not completed in the last window; mainloop does not finish within the window (order moved last).

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

## Eighth session (2026-11-06): per-program timeout, libc strtod/dtoa fix, build.sh with GIO
- **Clean sweep** (`test/boot-log-sweep.txt`; harness = `posix_spawn` + a 100 s limit per program + `RProcess::Kill` -- pheglibtest.mmp needs CAPABILITY PowerMgmt for that, without it the
  Kill is a KERN-EXEC 46 platform-security trap; the children's stdout/stderr go to `c:/data/phe_child_<name>.out`):
  `array PASS, strfuncs PASS, slist PASS, hash PASS (incl. its g_test_trap_subprocess case), keyfile FAIL (exit 1), list FAIL (exit 1), queue TIMEOUT, mainloop TIMEOUT` = `4 of 8 programs not clean`.
  queue and list passed in earlier runs with the pipe-capturing harness; new in this run are the libc fix and file-redirected children, so the TIMEOUT/FAIL verdicts are not attributed yet
  (next: print the tail of each child's .out file on FAIL/TIMEOUT).
- **strtod/printf of subnormals fixed in libc (x32).** The 2010 `arith.h` (ARM) has `#define Sudden_Underflow`; with it gdtoa's dtoa() prints every number below DBL_MIN with wrong digits
  (device: pow(2,-1024.1) printed `1.3720453440777821e-308`, correct `5.1901682964836274e-309`), so the printf/strtod round trip failed. Fix: a board `arith.h` without it
  (`toolchain/shims/libc_gdtoa_x32/` in the main repo, with byte copies of gdtoa.h and gdtoaimp.h so the quote-include finds it) through `incs_first` of the `libc` registry entry (x32 only).
  Device proof: `probe dbl pow(2,-1024.1): libc[5.1901682964836274e-309] strtod_back_eq=1 | glib[...] g_ascii_strtod_back_eq=1`, and strfuncs passes. Host test `tests/user/libc_strtod`
  (real gdtoa strtod + dtoa): 34 checks pass; with the 2010 arith.h swapped in it shows the wrong digits. Still open, reported but not counted: strtod of the largest subnormal printed with 17 digits parses to DBL_MIN.
- Gotcha: the x32 builder's object cache tracks headers through the `.d` files, so a header that newly wins the include search is NOT noticed: delete the `.o/.d/.key` of the affected objects in `.phorepoc-x86-build/mmp-x32/<key>/` (done for libc: 55 gdtoa users).
- `recipes/build.sh` has the `gio` step now (sequence: fetch tools native config objects gio archives images boot); verified from an empty volume up to the boot (the GIO tests passed there).

## Ninth session (2026-11-07): the sweep, attributed
Verdicts of the last boot (`test/boot-log-sweep.txt`; harness: pipes drained by select incl. the exception set, per-program limit 100 s, queue 500 s, child output tail on FAIL/TIMEOUT):
`queue PASS, array PASS, strfuncs PASS, keyfile PASS, slist PASS, list PASS, hash PASS, mainloop TIMEOUT` = 7 of 8 programs clean.
- **queue**: was never broken -- `/queue/random` runs 500,000 iterations with check_integrity() each (stack seen at the DUMP: `random_test` -> `check_integrity`), minutes under QEMU; the 100 s limit was the problem.
- **keyfile**: `/keyfile/save` does `g_mkstemp ("key_file_XXXXXX")` in the CURRENT directory; the child's cwd is not writable on Open C. The harness now chdirs to the writable dir (`c:/data`); `TMPDIR` also points there (the default tmp dir `C:\Private\<sid>\tmp` does not exist).
- **list**: failed in 3 earlier runs (`/list/double-free`: the grandchild's stderr did not match `*corrupted double-linked list detected*`) and passed in the last two with identical code: an intermittent result, cause not found (suspect the race between the grandchild's last stderr write and its exit; open).
- **mainloop**: runs 20+ cases, then stops in `/mainloop/unix-fd`, which fills a pipe, then pushes 128 MiB through it with `g_unix_fd_add` on both ends (G_IO_OUT on the write end). Open C's kernel pipe is 512 bytes (`KDefaultPipeSz`), so that is 260,000+ select round trips; I believe it is plain slowness, not a hang, but it was not run long enough to show (not proven). Not ours to change in GLib; a longer limit (and a quiet QEMU) would tell.
- **build.sh from an empty volume** (final libc + harness, `VOL=hildon-wk4 CLEAN=1 SECS=1000 bash recipes/build.sh`): EXIT=0 and a boot with verdicts `queue/array/strfuncs/slist/list/hash PASS, keyfile FAIL (cwd, since fixed), mainloop TIMEOUT`; the keyfile fix was then verified with `iterate.sh` on the same volume.

## Tenth session (2026-11-07): the pipe size, mainloop, final run
- **The 512 bytes are Open C's, not the kernel pipe LDD's**: `backend/src/corebackend/ftable.cpp` has `static const TInt KDefaultPipeSz = 512;` and passes it to `RPipe::Create` (and `RPipe::Define`); `dpipe.cpp` allocates the buffer with `Kern::AllocZ(aSize)` for any size. The x32 build takes a board copy of that file (`core/os/toolchain/shims/backend_board/ftable.cpp`); core still has 512. Private copy with `65536` (the Linux default) in the main repo (`toolchain/shims/backend_board/ftable.cpp`, `backend` registry entry): **core needs the same one-line change.**
- **Result**: with 64 KiB pipes `/mainloop/unix-fd` (128 MiB through a pipe) completes; the whole mainloop program takes ~150 s instead of not finishing in 300 s. It now fails one case: `/mainloop/unix-file-poll` does `open ("/dev/null", O_RDONLY)` and asserts `fd >= 0`: Open C here has no usable `/dev/null` path (`mainloop.c:1492`).
- **Final run from an empty volume** (`VOL=hildon-wk5 CLEAN=1 SECS=1300 bash recipes/build.sh`, final libc + 64 KiB pipes + harness): EXIT=0; `queue PASS (300 s), array PASS, strfuncs PASS, keyfile PASS, slist PASS, list PASS, hash PASS, mainloop FAIL (unix-file-poll: /dev/null)` = 7 of 8, `test/boot-log-sweep.txt`.
- **list flake**: did not occur again in 6 consecutive runs (the harness now keeps the grandchild's exact stderr bytes in `c:/data/phe_trap_stderr.bin` and logs them if `list` or any program fails: `patch_glib.sh` PHE_TRAPDUMP); so no bytes captured, cause still unknown.

## Eleventh session (2026-11-07): /dev/null, the list flake
- **/dev/null** (main repo, `toolchain/shims/backend_board/fdesc.cpp`, a board copy of the 2010 file used by the `backend` registry entry): `CFileDescBase::Open` knew only `NUL:` as the null device (a plain `CFileDescBase` is the null device: read = EOF, write completes, poll ready, ioctl ok); `open("/dev/null")` was resolved on C: and failed. Now `/dev/null` maps to it. Second fix in the same file: the base `WriteCompletion` returned the status (0), so `write()` on the null device reported 0 bytes; it returns the byte count now. Probe on the device: `probe /dev/null: fd=5 read=0 write=5 select=2 readable=1 writable=1`.
  There is no host test: the backend classes need the Symbian user library (CFileDescBase is an abstract class over RFs/TRequestStatus); the on-device probe is the test.
- **mainloop now PASSES** (all 22 cases incl. `/mainloop/unix-fd` and `/mainloop/unix-file-poll`; ~152 s). Full sweep (`test/boot-log-sweep.txt`): queue (312 s), array, strfuncs, keyfile, slist, hash, mainloop PASS; list intermittently FAIL.
- **list flake, cause found**: in a failing run the harness kept the trapped grandchild's stderr bytes: **0 bytes** (`[trap stderr: 0 bytes]`): the grandchild's last stderr write (the "corrupted double-linked list detected" warning) was lost when the process aborted right after it (the write goes through Open C's stdio, buffered when stderr is a pipe, and the abort is an int3 that skips any flush). Fix: `setvbuf (stderr, NULL, _IONBF, 0)` in the test child entry. After the fix `list` passed 10 of 10 runs in one boot (before: failures in 5 of the last 12 runs); that is evidence, not proof.

## Twelfth session (2026-11-07): text and images
- Pins: all 12 verified against independent sources (record in `recipes/pins.sh`); zlib is the `.tar.xz`. HarfBuzz 15.1.0 (newest; every `hb_*` call of Pango 1.40.14 is still declared).
- New DLLs `phehb phepng phefontconfig phepango phepixbuf` (`os/src/*`), recipes `build_cxxheaders/harfbuzz/zlib_png/fontconfig/pango/pixbuf_x32.sh`, `build.sh` steps `cxxhdr hb zlibpng fontconfig pango pixbuf defs`.
- Verified on the PC image (QEMU): HarfBuzz shaping; gdk-pixbuf + PNG 7/7 (needed `gio_can_sniff=yes`: no MIME database on the device); fontconfig finds the 5 ROM fonts (needed the drive-letter patches).
- Open: Pango's font creation hits `g_object_new_internal: assertion failed: (nqueue != NULL)`; see `docs/phorepoc/hildon-port.md` 7.7.1 in the main repo. Next: trace it, then the GDK backend.
- Pango draws: root cause was `malloc()` returning 4-mod-8 blocks (RHybridHeap slab cells grow in 4-byte steps) while GLib's GData uses the low 3 bits of its pointer for lock flags. x32 fix in the main repo (`toolchain/shims/libc_alloc8/ualloc.cpp`, rounds requests to 8; needs a core port, and the ARM image has the same allocator). `pango/harfbuzz: 12 checks, 0 failures`, a 164x33 px layout, 989 dark pixels. See `docs/phorepoc/hildon-port.md` 7.7.4.
