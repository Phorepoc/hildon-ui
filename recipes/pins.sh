# pins.sh -- sourced by the recipes: exact upstream versions + checksums (verified 2026-11-06)
GLIB_VER=2.56.4
GLIB_SHA256=27f703d125efb07f8a743666b580df0b4095c59fc8750e8890132c91d437504c
GLIB_URL=https://download.gnome.org/sources/glib/2.56/glib-$GLIB_VER.tar.xz
FFI_VER=3.4.6
FFI_SHA256=b0dea9df23c863a7a50e825440f3ebffabd65df1497108e5d437747843895a4e
FFI_URL=https://github.com/libffi/libffi/releases/download/v$FFI_VER/libffi-$FFI_VER.tar.gz
# Leste sources (read-only reference for the later steps; commits as cloned 2026-11-06)
LIBHILDON=78c268e512009d0a742ed7c5326ac036be9873b3
LIBHILDONDESKTOP=d05587180c24dbbe863823686461782e0db4b7c2
HILDON_HOME=8dd16c59f867689ee3d6c7b10166d84491aa82f5
HILDON_STATUS_MENU=796dbd7ca9737718db25ce9c43a3339c16114db9
HILDON_CONTROL_PANEL=359e43a8a89903c39ccea2f580420e8ff0bf9a85
# step 2 of docs/phorepoc/hildon-port.md: the drawing stack (sha256 checked against the published values on 2026-11-07)
PIXMAN_VER=0.42.2
PIXMAN_SHA256=ea1480efada2fd948bc75366f7c349e1c96d3297d09a3fe62626e38e234a625e
PIXMAN_URL=https://cairographics.org/releases/pixman-$PIXMAN_VER.tar.gz
CAIRO_VER=1.16.0
CAIRO_SHA256=5e7b29b3f113ef870d1e3ecf8adf21f923396401604bda16d44be45e66052331
CAIRO_URL=https://cairographics.org/releases/cairo-$CAIRO_VER.tar.xz
FREETYPE_VER=2.13.3
FREETYPE_SHA256=0550350666d427c74daeb85d5ac7bb353acba5f76956395995311a9c6f063289
FREETYPE_URL=https://download.savannah.gnu.org/releases/freetype/freetype-$FREETYPE_VER.tar.xz
# Pango 1.40.14 needs: glib >= 2.34 (ours 2.56.4), cairo >= 1.12 (1.16.0), freetype, fontconfig >= 2.11, harfbuzz >= 0.9.9. HarfBuzz: the NEWEST (15.1.0, 2026) still declares every hb_* function
# Pango 1.40's sources call (checked by grep against src/*.h, deprecated ones included: hb_font_funcs_set_glyph_func is in hb-deprecated.h), so the newest is used.
HARFBUZZ_VER=15.1.0
HARFBUZZ_SHA256=0fc7c565443a5f4be7da65ac5ee5bd737d9b158dc908708b7689a0a5796b0c8d
HARFBUZZ_URL=https://github.com/harfbuzz/harfbuzz/releases/download/$HARFBUZZ_VER/harfbuzz-$HARFBUZZ_VER.tar.xz
EXPAT_VER=2.5.0
EXPAT_SHA256=ef2420f0232c087801abf705e89ae65f6257df6b7931d37846a193ef2e8cdcbe
EXPAT_URL=https://github.com/libexpat/libexpat/releases/download/R_2_5_0/expat-$EXPAT_VER.tar.xz
FONTCONFIG_VER=2.14.2
FONTCONFIG_SHA256=dba695b57bce15023d2ceedef82062c2b925e51f5d4cc4aef736cf13f60a468b
FONTCONFIG_URL=https://www.freedesktop.org/software/fontconfig/release/fontconfig-$FONTCONFIG_VER.tar.xz
PANGO_VER=1.40.14
PANGO_SHA256=90af1beaa7bf9e4c52db29ec251ec4fd0a8f2cc185d521ad1f88d01b3a6a17e3
PANGO_URL=https://download.gnome.org/sources/pango/1.40/pango-$PANGO_VER.tar.xz
ZLIB_VER=1.3.1
ZLIB_SHA256=38ef96b8dfe510d42707d9c781877914792541133e1870841463bfa73f883e32
ZLIB_URL=https://github.com/madler/zlib/releases/download/v$ZLIB_VER/zlib-$ZLIB_VER.tar.xz
LIBPNG_VER=1.6.43
LIBPNG_SHA256=6a5ca0652392a2d7c9db2ae5b40210843c0bbc081cbd410825ab00cc59f14a6c
LIBPNG_URL=https://download.sourceforge.net/libpng/libpng-$LIBPNG_VER.tar.xz
GDKPIXBUF_VER=2.36.12
GDKPIXBUF_SHA256=fff85cf48223ab60e3c3c8318e2087131b590fd6f1737e42cb3759a3b427a334
GDKPIXBUF_URL=https://download.gnome.org/sources/gdk-pixbuf/2.36/gdk-pixbuf-$GDKPIXBUF_VER.tar.xz
# Verification record (2026-11-07), each against a source other than this file:
#  glib, pango, gdk-pixbuf: download.gnome.org .sha256sum files.  pixman: cairographics.org .sha512 (file matches).  cairo: cairographics.org .sha1 + Fedora sources sha512.
#  freetype, expat: upstream GPG signatures verify (Werner Lemberg, Sebastian Pipping).  libffi, libpng: Arch packaging sha256.  fontconfig: Fedora f38-f40 `sources` sha512.
#  zlib: Arch packaging sha512 for the .tar.xz (so the pin is the .tar.xz, not the .tar.gz).  harfbuzz 15.1.0: GitHub release asset digest (real release, 04 Nov).
GTK_VER=2.24.33
GTK_SHA256=ac2ac757f5942d318a311a54b0c80b5ef295f299c2a73c632f6bfb1ff49cc6da   # = download.gnome.org gtk+-2.24.33.sha256sum (verified 2026-11-07); LGPL 2.1+
GTK_URL=https://download.gnome.org/sources/gtk+/2.24/gtk+-$GTK_VER.tar.xz
ATK_VER=2.38.0
ATK_SHA256=ac4de2a4ef4bd5665052952fe169657e65e895c5057dffb3c2a810f6191a0c36   # = download.gnome.org atk-2.38.0.sha256sum; SHA512 dffd0a08...2c77 = Gentoo Manifest (verified 2026-11-07); LGPL 2+; GTK 2.24 needs atk >= 2.10
ATK_URL=https://download.gnome.org/sources/atk/2.38/atk-$ATK_VER.tar.xz
