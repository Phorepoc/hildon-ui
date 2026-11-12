# Copyright The Phorepoc Authors
# SPDX-License-Identifier: BSD-2-Clause

"""components_hildonui -- hildon-ui's components (merged into core's registry when this repository is a layer:
PHOREPOC_ADAPTATIONS=...:<glib-symbian>:<gtk-symbian>:<dbus-symbian>:<hildon-ui>). ".hildon/..." resolves to
$PHOREPOC_BUILD/hildon/ (the archives recipes/ builds). Nothing here is in an OS image: Hildon and its demo are Catalogue
downloads (recipes/sis.sh).
"""
import os

from paths import OS_ROOT

_COMMON = dict(allow_wsd=True, static_ctors=True, rtti=False, defs=["__SOFTFP="])
_RT = [f"thirdparty/x86/cxxrt/{a}.a" for a in ("libphcompat", "libgcc")]
_DATA = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "os", "data")


def _dll(name, sym=False):
    c = dict(mmp=os.path.join(OS_ROOT, f"src/{name}/{name}.mmp"), rom=f"{name}.dll", cdriver=True,
             x32_archives=[f".hildon/lib{name}.a"] + _RT, **{"def": os.path.join(OS_ROOT, f"src/{name}/{name}.def")}, **_COMMON)
    if sym:   # a GModule plug-in: looked up by name through its symbol table
        c["rom_data"] = [f"resource\\gmodule\\{name}.sym=.hildon/gmodule/{name}.sym"]
    return c


def _exe(name, data):
    return dict(mmp=os.path.join(OS_ROOT, f"src/{name}/{name}.mmp"), stack=0x100000, rom_data=data,
                x32_archives=[f".hildon/lib{name}.a"] + _RT, **_COMMON)


COMPONENTS = {
    "phehildon": _dll("phehildon"),
    "phehildondesktop": _dll("phehildondesktop"),
    "phehomeexample": _dll("phehomeexample", sym=True),
    "sapwood": _dll("sapwood", sym=True),
    "phehome": _exe("phehome", ["resource\\phehome\\fonts.conf=.hildon/testdata/phehome_fonts.conf",
                                "resource\\phehome\\phehildon.rc=.hildon/testdata/phehildon.rc",
                                "resource\\phehome\\wallpaper.png=" + os.path.join(_DATA, "phehome/wallpaper.png"),
                                "resource\\hildondesktop\\home.conf=" + os.path.join(_DATA, "hildondesktop/home.conf"),
                                "resource\\hildondesktop\\home.plugins=" + os.path.join(_DATA, "hildondesktop/home.plugins"),
                                "resource\\hildondesktop\\entries\\home\\phehomeexample.desktop="
                                + os.path.join(_DATA, "hildondesktop/entries/home/phehomeexample.desktop")]),
    # the libhildon demo (its own package, hildon-ui-demo: the menu's "Hildon demo")
    "phehildonwin": _exe("phehildonwin", ["resource\\phehildonwin\\fonts.conf=.hildon/testdata/phehildonwin_fonts.conf",
                                          "resource\\phehildonwin\\phehildon.rc=.hildon/testdata/phehildon.rc",
                                          "resource\\phehildonwin\\beep.wav=.hildon/testdata/phehildon_beep.wav"]),
    # the Hildon control panel (its own package, hildon-settings: the menu's "Settings (Hildon)"), its two stand-in applets (docs hildon-port.md 7.104)
    "phehcp": _exe("phehcp", ["resource\\phehcp\\fonts.conf=.hildon/testdata/phehildonwin_fonts.conf",
                              "resource\\phehcp\\phehildon.rc=.hildon/testdata/phehildon.rc"]
                  + [f"resource\\phehcp\\icons\\hicolor\\{f.replace('/', chr(92))}=" + os.path.join(_DATA, "phehcp/icons/hicolor", f)
                     for f in ("index.theme", "48x48/hildon/general_clock.png", "48x48/hildon/general_globe.png", "48x48/hildon/general_display.png", "48x48/hildon/filemanager_unknown_file.png")]),
    "phehcpdatetime": _dll("phehcpdatetime", sym=True),
    "phehcpregion": _dll("phehcpregion", sym=True),
    "phehcpdisplay": _dll("phehcpdisplay", sym=True),
    # the clock home widget (its own package, hildon-clock-widget)
    "phehomeclock": _dll("phehomeclock", sym=True),
    # the date home widget (its own package, hildon-date-widget)
    "phehomedate": _dll("phehomedate", sym=True),
}

for _n in ("phehcpdatetime", "phehcpregion", "phehcpdisplay"):
    COMPONENTS[_n]["rom_data"].append(f"resource\\hildon-control-panel\\{_n}.desktop=" + os.path.join(_DATA, f"hildon-control-panel/{_n}.desktop"))
COMPONENTS["phehomeclock"]["rom_data"].append("resource\\hildondesktop\\entries\\home\\phehomeclock.desktop="
                                            + os.path.join(_DATA, "hildondesktop/entries/home/phehomeclock.desktop"))
COMPONENTS["phehomedate"]["rom_data"].append("resource\\hildondesktop\\entries\\home\\phehomedate.desktop="
                                           + os.path.join(_DATA, "hildondesktop/entries/home/phehomedate.desktop"))

# installed, not in a ROM
for _c in COMPONENTS.values():
    _c.setdefault("sis", True)
