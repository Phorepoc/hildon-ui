#!/usr/bin/env python3
# gen_defs.py <archive dir> <out os/src dir> -- the frozen-ordinal .def files of the GLib DLLs, from nm of the static archives.
# A DLL exports (a) every public name (g_*, glib_*) its archive defines and (b) every other defined global that one of the OTHER
# archives (or the test) needs from it. Ordinals are assigned in sorted order: the first freeze. Data symbols get DATA.
# Run in the phorepoc-x86 image. Re-running on changed archives renumbers: from the first release on, extend the .def by hand.
import re, subprocess, sys, os
adir, out = sys.argv[1:3]
LIBS = [("pheglib", "libpheglib.a"), ("phegobject", "libphegobject.a"), ("phegmodule", "libphegmodule.a"), ("phegthread", "libphegthread.a"), ("phegio", "libphegio.a"), ("phepixman", "libphepixman.a"), ("phecairo", "libphecairo.a"), ("phefreetype", "libphefreetype.a"), ("phehb", "libphehb.a"), ("phepng", "libphepng.a"), ("phefontconfig", "libphefontconfig.a"), ("phepango", "libphepango.a"), ("phepixbuf", "libphepixbuf.a")]
OTHER = ["libpheglibtest.a", "libpheffi.a"]
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
    exp = sorted(n for n in d if not PRIV.match(n) and (re.match(r"^(g_|glib_|pixman_|cairo_|FT_|hb_|png_|Fc|XML_|pango_|gdk_pixbuf_|gdk_pixdata_|gdk_pixbuf_)", n) or n in need))
    path = os.path.join(out, name, name + ".def")
    with open(path, "w", newline="\n") as fh:
        fh.write("EXPORTS\n")
        for i, n in enumerate(exp, 1):
            fh.write("\t%s @ %d NONAME%s\n" % (n, i, " DATA" if d[n] in "DBRVGSdbrvs" else ""))
    print(name, len(exp), "exports ->", path)
