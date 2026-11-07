#!/usr/bin/env python3
# gen_defs.py <archive dir> <out os/src dir> -- the frozen-ordinal .def files of this repository's DLLs, from nm of the static archives.
# A DLL exports (a) every public name (g_*, glib_*) its archive defines and (b) every other defined global that one of the OTHER
# archives (or the test) needs from it. Ordinals are assigned in sorted order: the first freeze. Data symbols get DATA.
# Run in the phorepoc-x86 image. Re-running on changed archives renumbers: from the first release on, extend the .def by hand.
import re, subprocess, sys, os
adir, out = sys.argv[1:3]
LIBS = [("phehildon", "libphehildon.a"), ("phehildondesktop", "libphehildondesktop.a")]
OTHER = ['libpheglib.a', 'libphegobject.a', 'libphegmodule.a', 'libphegthread.a', 'libphegio.a', 'libphepixman.a', 'libphecairo.a', 'libphefreetype.a', 'libphehb.a', 'libphepng.a', 'libphefontconfig.a', 'libphepango.a', 'libphepixbuf.a', 'libpheatk.a', 'libphegdk.a', 'libphegtk.a', 'libphedbus.a', 'libphedbusglib.a'] + ["libpheglibtest.a", "libpheffi.a", "libphegdkwin.a", "libphegtkwin.a", "libphehildonwin.a", "libphedbustest.a", "libphehildondesktoptest.a"]
def nm(a):
    d, u = {}, set()
    for line in subprocess.run(["nm", "-g", "--no-demangle", os.path.join(adir, a)], capture_output=True, text=True).stdout.splitlines():
        p = line.split()
        if len(p) == 3 and p[1] in "TDBRVWGSdbrvs": d[p[2]] = p[1]
        elif len(p) == 2 and p[0] == "U": u.add(p[1])
    return d, u
info = {a: nm(a) for _, a in LIBS + [(None, o) for o in OTHER] if os.path.exists(os.path.join(adir, a))}
PRIV = re.compile(r"^(phe_|fork$|execv$|execve$|setrlimit$)")
for name, a in [l for l in LIBS if l[1] in info]:
    d, _ = info[a]
    need = set()
    for b, (_, u) in info.items():
        if b != a: need |= u
    exp = sorted(n for n in d if not PRIV.match(n) and (re.match(r"^(g_|glib_|pixman_|cairo_|FT_|hb_|png_|Fc|XML_|pango_|gdk_pixbuf_|gdk_pixdata_|gdk_|atk_|gtk_|hildon_|dbus_|hd_)", n) or n in need))
    path = os.path.join(out, name, name + ".def")
    # frozen ABI: a .def that exists is only EXTENDED (new names get the next ordinals, nothing is renumbered or removed); only a missing one is written from scratch
    have, last = set(), 0
    if os.path.exists(path):
        for l in open(path, newline="").read().replace("\r\n", "\n").splitlines():
            m = re.match(r"^\s*(\S+)\s*@\s*(\d+)", l)
            if m: have.add(m.group(1)); last = max(last, int(m.group(2)))
    mode = "a" if have else "w"
    with open(path, mode, newline="\n") as fh:
        if not have: fh.write("EXPORTS\n")
        for n in exp:
            if n in have: continue
            last += 1
            fh.write("\t%s @ %d NONAME%s\n" % (n, last, " DATA" if d[n] in "DBRVGSdbrvs" else ""))
    print(name, len(exp), "exports ->", path)
