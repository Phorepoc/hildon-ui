#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: BSD-2-Clause
# build_hildon_x32.sh -- IN phorepoc-x86 (volume at /wk, repo :ro at /repo, this repo at /h): libhildon 2.2.30 (LGPL 2.1, github.com/maemo-leste/libhildon at the commit pinned in pins.sh, cloned into
# dl/libhildon) for x32 over Open C, on GTK 2.24.33 / the Symbian GDK backend. No configure: the generated sources (hildon-enum-types.[ch], hildon-marshalers.[ch], hildon-version.h) are made exactly as
# hildon/Makefile.am makes them; the platform dependencies of libhildon -- Xlib / gdkx.h (window ids, atoms, properties, client messages), GConf, libcanberra, libintl -- are the shims of
# os/src/phehildon (real implementations over this backend, see their headers). The two compositor-side classes (HildonAnimationActor, HildonRemoteTexture) are not built.
# Out: /wk/hildon-x32/*.o, /wk/hildon-x32/errors.txt, /wk/out/libphehildon.a, /wk/hildon-x32/undefined.txt
set -eu
. /glib/recipes/pins.sh; . /gtk/recipes/pins.sh; . /dbus/recipes/pins.sh; . /h/recipes/pins.sh
CC=/wk/lsc/bin/pp-gcc
H=/h/dl/libhildon; O=/wk/hildon-x32; mkdir -p $O /wk/out; NAT=/wk/nat/gobject; S=$H/hildon; SH=/h/os/src/phehildon
[ -d $S ] || { echo "no $S: git clone github.com/maemo-leste/libhildon into dl/ and check out \$LIBHILDON"; exit 1; }
# generated sources into $O (the clone stays untouched); the sources are read from the clone
list() { awk -v v="$1" '$0 ~ "^"v" *(\\+?=)" {on=1; sub(/^[^=]*=/,""); } on { line=$0; cont = (line ~ /\\[ \t]*$/); gsub(/\\[ \t]*$/,"",line); print line; if (!cont) on=0 }' $S/Makefile.am | tr -s ' \t' '\n\n' | grep "\.[ch]$" || true; }
PUB=$(list 'libhildon_@API_VERSION_MAJOR@_public_headers' | grep -v "animation-actor\|remote-texture" | tr '\n' ' ')
V=$(sed -n 's/^AC_INIT(.*\[\([0-9][0-9.]*\)\].*/\1/p' $H/configure.ac | head -1); MAJ=${V%%.*}; R=${V#*.}; MIN=${R%%.*}; MIC=${R#*.}
sed -e "s/@PACKAGE_VERSION_MAJOR@/$MAJ/g; s/@PACKAGE_VERSION_MINOR@/$MIN/g; s/@PACKAGE_VERSION_MICRO@/$MIC/g; s/@PACKAGE_VERSION@/$V/g" $S/hildon-version.h.in > $O/hildon-version.h
cp $S/hildon-marshalers.list $O/
cd $O
$NAT/glib-genmarshal --prefix _hildon_marshal --header hildon-marshalers.list > hildon-marshalers.h
{ echo '#include "hildon-marshalers.h"'; $NAT/glib-genmarshal --prefix _hildon_marshal --body hildon-marshalers.list; } > hildon-marshalers.c
mkdir -p $O/hdr; rm -f $O/hdr/*; for h in $S/hildon-*.h $S/maemo-gtk-compat.h; do b=$(basename $h); case $b in *-private.h|hildon-enum-types.h|hildon-marshalers.h|hildon-animation-actor.h|hildon-remote-texture.h) ;; *) ln -s $h $O/hdr/$b;; esac; done; ln -sf $O/hildon-version.h $O/hdr/hildon-version.h
PUB=$(cd $O/hdr && ls | tr '
' ' ')
PUB=$(echo $PUB | tr -s " " "
" | grep -v maemo-gtk-compat | tr "
" " ")     # -DMAEMO_GTK: the enums of maemo-gtk-compat.h (HildonMode, HildonUIMode, ...) are GTK's own, with their GTypes in GTK
cd $O/hdr
python3 $NAT/glib-mkenums --fhead "#ifndef __HILDON_ENUM_TYPES_H__\n#define __HILDON_ENUM_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n\n" \
  --fprod "/* Enumerations from \"@filename@\" */\n" \
  --vhead "GType @enum_name@_get_type (void);\n#define HILDON_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
  --ftail "\nG_END_DECLS\n\n#endif /* __HILDON_ENUM_TYPES_H__ */" $PUB > $O/hildon-enum-types.h
python3 $NAT/glib-mkenums --fhead "#undef HILDON_DISABLE_DEPRECATED\n#include \"hildon.h\"\n#include <glib-object.h>" \
  --fprod "\n/* enumerations from \"@filename@\" */" \
  --vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" \
  --vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
  --vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" $PUB > $O/hildon-enum-types.c
cat > $O/config.h <<EOC
#define GETTEXT_PACKAGE "hildon-1"
#define PACKAGE "hildon"
#define VERSION "$V"
#define HAVE_SYS_TIME_H 1
EOC
SRC=$(list 'libhildon_@API_VERSION_MAJOR@_la_SOURCES' | grep "\.c$" | grep -v "animation-actor\|remote-texture\|hildon-enum-types\|hildon-marshalers" | sed 's/\.c$//')
SRC=$(echo $SRC | tr ' ' '\n' | sort -u | tr '\n' ' ')
echo "hildon: $(echo $SRC | wc -w) sources of libhildon $V"
G=/wk/gtk+-$GTK_VER
X32="-I/wk/x32 -I/wk/x32/glib -I/wk/glib-2.56.4 -I/wk/glib-2.56.4/glib -I/wk/glib-2.56.4/gobject -I/wk/glib-2.56.4/gmodule -I/wk/glib-2.56.4/gio -I/wk/ffi-x32/include"
FF="-O2 -g0 -w -fno-pic -I/glib/overlay -include /glib/overlay/phe_glib_compat.h -include /wk/gdk-x32/gtkcompat.h -include $SH/shims/phe_locale.h -DHAVE_CONFIG_H -DMAEMO_GTK -DG_LOG_DOMAIN=\"hildon-1\" -DLOCALEDIR=\"z:/resource/locale\" \
 -DGTK_DISABLE_DEPRECATION_WARNINGS -DGDK_DISABLE_DEPRECATION_WARNINGS -I$O -I$S -I$SH/shims -I$G -I$G/gdk -I/wk/gdk-x32 -I/wk/atk-$ATK_VER -I/wk/gtk-x32/inc $X32 \
 -I/wk/cairo-1.16.0/src -I/wk/cairo-x32/src -I/wk/pixman-0.42.2/pixman -I/wk/pango-1.40.14 -I/wk/pango-x32 -I/wk/harfbuzz-$HARFBUZZ_VER/src -I/wk/fontconfig-2.14.2 -I/wk/freetype-2.13.3/include -I/wk/gdk-pixbuf-2.36.12 -I/wk/pixbuf-x32 -I/wk/gtk-x32"
# <hildon/hildon.h> style includes and <gtk/gtk.h>: symlink trees
mkdir -p $O/inc; ln -sfn $S $O/inc/hildon
rm -f $O/*.o $O/errs.*; export CC FF O S SH
one() { f=$1; $CC $FF -I$O/inc -c $S/$f.c -o $O/$f.o 2> $O/errs.$f || echo "FAIL $f"; }
export -f one
for x in hildon-marshalers hildon-enum-types; do $CC $FF -I$O/inc -c $O/$x.c -o $O/$x.o 2> $O/errs.$x || echo "FAIL $x"; done
for x in phehildon_x11 phehildon_gconf phehildon_canberra; do $CC $FF -I$O/inc -c $SH/$x.c -o $O/$x.o 2> $O/errs.$x || echo "FAIL $x"; done
fails=$(echo $SRC | tr ' ' '\n' | xargs -P "${JOBS:-8}" -I{} bash -c 'one {}' | tee $O/failed.txt | wc -l)
cat $O/errs.* > $O/errors.txt 2>/dev/null || true; rm -f $O/errs.*
echo "hildon: $(ls $O/*.o 2>/dev/null | wc -l) objects, $fails failed"
head -40 $O/failed.txt | tr '\n' ' '; echo
grep -a "error" $O/errors.txt | sed "s|$S/||" | cut -c1-200 | head -${ERRN:-30}
GO=$(ls /wk/gdk-x32/*.o /wk/gdk-x32/be/*.o /wk/atk-x32/*.o /wk/gtk-x32/*.o)
nm -u $O/*.o | grep " U " | awk '{print $2}' | sort -u > $O/und_all.txt
nm --defined-only $O/*.o $GO | awk 'NF==3{print $3}' | sort -u > $O/def_all.txt
comm -23 $O/und_all.txt $O/def_all.txt | grep -E "^(_?hildon_|gtk_|gdk_|atk_|X[A-Z]|ca_|gconf_|_?hildon)" > $O/undefined.txt || true
echo "undefined hildon_/gtk_/gdk_/X*/ca_/gconf_ symbols: $(wc -l < $O/undefined.txt)"; head -60 $O/undefined.txt | tr '\n' ' '
rm -f /wk/out/libphehildon.a; ar rcs /wk/out/libphehildon.a $O/*.o; echo "libphehildon.a: $(ar t /wk/out/libphehildon.a | wc -l) members"
