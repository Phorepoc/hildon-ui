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
