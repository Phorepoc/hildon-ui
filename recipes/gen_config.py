#!/usr/bin/env python3
# gen_config.py <native-config.h> <native-glibconfig.h> <libc_syms.txt> <stdapis-dir> <out-dir>
# Native (x86_64 glibc) GLib config -> x32 / Open C config.h + glibconfig.h. Run in the phorepoc-x86 image.
import os, re, sys
cfg, gcfg, symf, apis, out = sys.argv[1:6]
syms = set(open(symf).read().split())
os.makedirs(out, exist_ok=True)
KEEP = {"HAVE_UINT64_T","HAVE_TYPEOF","HAVE_BUILTIN_CLZ","HAVE_GCC_VECTOR_EXTENSIONS","HAVE_PTHREADS","HAVE_ALLOCA","HAVE_C99_SNPRINTF","HAVE_C99_VSNPRINTF","HAVE_GOOD_PRINTF","HAVE_LONG_DOUBLE","HAVE_LONG_LONG",
        "HAVE_LONG_LONG_FORMAT","HAVE_INTMAX_T","HAVE_INTTYPES_H_WITH_UINTMAX","HAVE_STDINT_H_WITH_UINTMAX","HAVE_PTRDIFF_T",
        "HAVE_SIG_ATOMIC_T","HAVE_WCHAR_T","HAVE_WINT_T","HAVE_RTLD_GLOBAL","HAVE_RTLD_LAZY","HAVE_RTLD_NOW","HAVE_LC_MESSAGES"}
DROP_PREFIX = ("HAVE_STRUCT_", "HAVE_LANGINFO_", "HAVE_DECL_")
DROP = {"STRERROR_R_CHAR_P","HAVE_UNIX98_PRINTF","HAVE_OPEN_O_DIRECTORY","HAVE_LIBC_ENABLE_SECURE","HAVE_FUTEX","HAVE_NETLINK","HAVE_IP_MREQN",
        "HAVE_SIOCGIFADDR","USE_STATFS","ENABLE_NLS","HAVE_GETTEXT","HAVE_DCGETTEXT","HAVE_BIND_TEXTDOMAIN_CODESET",
        "HAVE_PTHREAD_SETNAME_NP_WITH_TID","HAVE_PTHREAD_GETNAME_NP","HAVE_CODESET","HAVE_MMAP","HAVE_MEMALIGN","HAVE_VALLOC",
        "HAVE_POSIX_MEMALIGN","HAVE_GETC_UNLOCKED"}
SIZES = {"SIZEOF_LONG":"4","SIZEOF_SIZE_T":"4","SIZEOF_SSIZE_T":"4","SIZEOF_VOID_P":"4"}
def header_ok(n):
    base = n[5:-2].lower()
    for pre in ("sys","netinet","arpa","net"):
        if base.startswith(pre+"_"):
            cand = pre + "/" + base[len(pre)+1:] + ".h"; break
    else: cand = base + ".h"
    return os.path.exists(os.path.join(apis, cand))
kept, dropped = [], []
res = []
for line in open(cfg):
    m = re.match(r"#define (\w+)(?: (.*))?$", line.rstrip("\n"))
    if not m: res.append(line); continue
    n, v = m.group(1), m.group(2)
    if n in SIZES: res.append("#define %s %s\n" % (n, SIZES[n])); continue
    if n.startswith("HAVE_") or n in ("ENABLE_NLS","USE_STATFS","STRERROR_R_CHAR_P"):
        if n in KEEP or n.startswith(("HAVE_FT_", "HAVE_CAIRO", "HAVE_FREETYPE")): ok = True   # FT_* are FreeType functions (phefreetype), not libc
        elif n in DROP or n.startswith(DROP_PREFIX): ok = False
        elif n.endswith("_H"): ok = header_ok(n)
        else: ok = n[5:].lower() in syms
        (kept if ok else dropped).append(n)
        res.append(line if ok else "/* #undef %s (x32/Open C) */\n" % n); continue
    res.append(line)
open(os.path.join(out, "config.h"), "w", newline="\n").write("".join(res))
if gcfg == "-":   # a config.h only (pixman, cairo ...): no glibconfig.h to patch
    print("kept %d HAVE_, dropped %d: %s" % (len(kept), len(dropped), " ".join(sorted(dropped)))); sys.exit(0)
g = open(gcfg).read()
rep = [("typedef signed long gint64;","typedef signed long long gint64;"),("typedef unsigned long guint64;","typedef unsigned long long guint64;"),
 ("(val##L)","(val##LL)"),("(val##UL)","(val##ULL)"),('G_GINT64_MODIFIER "l"','G_GINT64_MODIFIER "ll"'),
 ('G_GINT64_FORMAT "li"','G_GINT64_FORMAT "lli"'),('G_GUINT64_FORMAT "lu"','G_GUINT64_FORMAT "llu"'),
 ("#define GLIB_SIZEOF_VOID_P 8","#define GLIB_SIZEOF_VOID_P 4"),("#define GLIB_SIZEOF_LONG   8","#define GLIB_SIZEOF_LONG   4"),
 ("#define GLIB_SIZEOF_SIZE_T 8","#define GLIB_SIZEOF_SIZE_T 4"),("#define GLIB_SIZEOF_SSIZE_T 8","#define GLIB_SIZEOF_SSIZE_T 4"),
 ("typedef signed long gssize;","typedef signed int gssize;"),("typedef unsigned long gsize;","typedef unsigned int gsize;"),
 ('G_GSIZE_MODIFIER "l"','G_GSIZE_MODIFIER ""'),('G_GSSIZE_MODIFIER "l"','G_GSSIZE_MODIFIER ""'),
 ('G_GSIZE_FORMAT "lu"','G_GSIZE_FORMAT "u"'),('G_GSSIZE_FORMAT "li"','G_GSSIZE_FORMAT "i"'),
 ("#define G_MAXSIZE\tG_MAXULONG","#define G_MAXSIZE\tG_MAXUINT"),("#define G_MINSSIZE\tG_MINLONG","#define G_MINSSIZE\tG_MININT"),
 ("#define G_MAXSSIZE\tG_MAXLONG","#define G_MAXSSIZE\tG_MAXINT"),
 ("typedef signed long gintptr;","typedef signed int gintptr;"),("typedef unsigned long guintptr;","typedef unsigned int guintptr;"),
 ('G_GINTPTR_MODIFIER      "l"','G_GINTPTR_MODIFIER      ""'),('G_GINTPTR_FORMAT        "li"','G_GINTPTR_FORMAT        "i"'),
 ('G_GUINTPTR_FORMAT       "lu"','G_GUINTPTR_FORMAT       "u"'),("#define GLIB_HAVE_ALLOCA_H\n",""),
 ("#define GLIB_USING_SYSTEM_PRINTF\n","")]
for a, b in rep:
    if a not in g: print("glibconfig: pattern not found:", repr(a))
    g = g.replace(a, b)
open(os.path.join(out, "glibconfig.h"), "w", newline="\n").write(g)
print("kept %d HAVE_, dropped %d: %s" % (len(kept), len(dropped), " ".join(sorted(dropped))))
