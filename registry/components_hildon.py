# Copyright The Phorepoc Authors
# SPDX-License-Identifier: BSD-2-Clause

"""components_hildon -- the Hildon groundwork's components (merged into core's registry when this repository is a layer, like
codecpack: PHOREPOC_ADAPTATIONS=<board adaptations>:<path to hildon-symbian>). Paths are relative to the merged OS tree;
".hildon/..." must resolve to $PHOREPOC_BUILD/hildon/ (the pdk's paths.resolve needs that prefix added, like ".codecpack/").
recipes/ builds the archives; nothing here is in an OS image -- Hildon is a Catalogue download.

  GLib 2.56.4 + GObject + GModule + GThread (LGPL 2.1+) as four app-private DLLs, and the console test:
    phe component build pheglib phegobject phegmodule phegthread pheglibtest
"""
import os

from paths import OS_ROOT  # noqa: E402,F401

_COMMON = dict(cdriver=True, allow_wsd=True, static_ctors=True, rtti=False, defs=["__SOFTFP="])
_RT = [f"thirdparty/x86/cxxrt/{a}.a" for a in ("libphcompat", "libgcc")]


def _dll(name, archives):
    return dict(mmp=os.path.join(OS_ROOT, f"src/{name}/{name}.mmp"), rom=f"{name}.dll",
                x32_archives=[f".hildon/{a}.a" for a in archives] + _RT, **{"def": os.path.join(OS_ROOT, f"src/{name}/{name}.def")}, **_COMMON)


COMPONENTS = {
    "pheglib": _dll("pheglib", ["libpheglib"]),
    "phegobject": _dll("phegobject", ["libphegobject", "libpheffi"]),
    "phegmodule": _dll("phegmodule", ["libphegmodule"]),
    "phegthread": _dll("phegthread", ["libphegthread"]),
    "phepixman": _dll("phepixman", ["libphepixman"]),
    "phefreetype": _dll("phefreetype", ["libphefreetype"]),
    "phecairo": _dll("phecairo", ["libphecairo"]),
    "phecairowin": dict(mmp=os.path.join(OS_ROOT, "src/phecairowin/phecairowin.mmp"), x32_flags=["-I.hildon/include"]),
    "phegio": dict(_dll("phegio", ["libphegio"]), x32_archives=[".hildon/libphegio.a", "thirdparty/x86/cxxrt/libz.a", "thirdparty/x86/cxxrt/libphcompat.a", "thirdparty/x86/cxxrt/libgcc.a"]),
    # the console test EXE: links the four DLLs through its mmp's LIBRARY line (SSM_TEST=pheglibtest)
    "pheglibtest": dict(mmp=os.path.join(OS_ROOT, "src/pheglibtest/pheglibtest.mmp"), test=True, stack=0x100000,
                        rom_data=["resource\pheglibtest\\" + _f + "=.hildon/testdata/" + _f for _f in ("keyfiletest.ini", "pages.ini", "keyfile.c")],
                        x32_archives=[".hildon/libpheglibtest.a"] + _RT, **_COMMON),
}

# installed, not in a ROM: loaded from RAM, where the loader validates each image
for _c in COMPONENTS.values():
    _c.setdefault("sis", True)
