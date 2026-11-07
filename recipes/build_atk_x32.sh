#!/usr/bin/env bash
# build_atk_x32.sh -- IN phorepoc-x86 (volume at /wk, repo :ro at /repo, this repo at /h): ATK 2.38.0 (LGPL 2+; GTK 2.24 needs >= 2.10) for x32 over Open C, from the pinned tarball.
# ATK ships no generated files: atkversion.h (from atkversion.h.in), atkmarshal.[ch] (glib-genmarshal over atkmarshal.list) and atk-enum-types.[ch] (glib-mkenums over the
# public headers with the two templates) are produced here exactly as the project's meson.build does, with the GLib tools of the native build (/wk/nat). Out: /wk/atk-x32/*.o, /wk/out/libpheatk.a
set -eu
. /h/recipes/pins.sh
CC=/wk/lsc/bin/pp-gcc
cd /wk; [ -d atk-$ATK_VER/atk ] || tar xf /h/dl/atk-$ATK_VER.tar.xz --exclude=docs --exclude=po --exclude=tests
A=/wk/atk-$ATK_VER/atk; O=/wk/atk-x32; mkdir -p $O /wk/out; NAT=/wk/nat/gobject
MAJ=${ATK_VER%%.*}; REST=${ATK_VER#*.}; MIN=${REST%%.*}; MIC=${REST#*.}
BIN=$((10000 * MAJ + 100 * MIN + 10 + MIC))
sed -e "s/@ATK_MAJOR_VERSION@/$MAJ/g" -e "s/@ATK_MINOR_VERSION@/$MIN/g" -e "s/@ATK_MICRO_VERSION@/$MIC/g" -e "s/@ATK_BINARY_AGE@/$BIN/g" -e "s/@ATK_INTERFACE_AGE@/1/g" \
    -e "s/@LT_CURRENT_MINUS_AGE@/0/g" -e "s/@ATK_VERSION@/$ATK_VER/g" $A/atkversion.h.in > $A/atkversion.h
cd $A
$NAT/glib-genmarshal --header --prefix=atk_marshal atkmarshal.list > atkmarshal.h
{ echo '#include "atkmarshal.h"'; $NAT/glib-genmarshal --body --prefix=atk_marshal atkmarshal.list; } > atkmarshal.c
HDRS="atkaction.h atkcomponent.h atkdocument.h atkeditabletext.h atkgobjectaccessible.h atkhyperlink.h atkhyperlinkimpl.h atkhypertext.h atkimage.h atkmisc.h atknoopobject.h atknoopobjectfactory.h atkobject.h atkobjectfactory.h atkplug.h atkrange.h atkregistry.h atkrelation.h atkrelationtype.h atkrelationset.h atkselection.h atksocket.h atkstate.h atkstateset.h atkstreamablecontent.h atktable.h atktablecell.h atktext.h atkutil.h atkvalue.h atkwindow.h atk-autocleanups.h"
python3 $NAT/glib-mkenums --template atk-enum-types.h.template $HDRS > atk-enum-types.h
python3 $NAT/glib-mkenums --template atk-enum-types.c.template $HDRS > atk-enum-types.c
cat > $O/config.h <<EOC
#define GETTEXT_PACKAGE "atk10"
#define VERSION "$ATK_VER"
#define PACKAGE_VERSION "$ATK_VER"
EOC
X32="-I/wk/x32 -I/wk/x32/glib -I/wk/glib-2.56.4 -I/wk/glib-2.56.4/glib -I/wk/glib-2.56.4/gobject -I/wk/glib-2.56.4/gmodule -I/wk/glib-2.56.4/gio -I/wk/ffi-x32/include"
FF="-O2 -g0 -w -fno-pic -I/h/overlay -include /h/overlay/phe_glib_compat.h -DHAVE_CONFIG_H -DG_LOG_DOMAIN=\"Atk\" -DGLIB_DISABLE_DEPRECATION_WARNINGS -DATK_DISABLE_DEPRECATION_WARNINGS -DATK_COMPILATION \
 -DATK_LOCALEDIR=\"z:/resource/locale\" -DG_DISABLE_SINGLE_INCLUDES -DATK_DISABLE_SINGLE_INCLUDES -I$O -I$A -I/wk/atk-$ATK_VER $X32"
SRC="atkaction atkcomponent atkdocument atkeditabletext atkgobjectaccessible atkhyperlink atkhyperlinkimpl atkhypertext atkimage atknoopobject atknoopobjectfactory atkobject atkobjectfactory atkplug \
 atkprivate atkrange atkregistry atkrelation atkrelationset atkselection atksocket atkstate atkstateset atkstreamablecontent atktable atktablecell atktext atkutil atkmisc atkvalue atkversion atkwindow atkmarshal atk-enum-types"
rm -f $O/*.o $O/errors.txt; fails=0
for f in $SRC; do $CC $FF -c $A/$f.c -o $O/$f.o 2>> $O/errors.txt || { echo "FAIL $f"; fails=$((fails+1)); }; done
echo "atk: $(ls $O/*.o 2>/dev/null | wc -l) objects, $fails failed"
grep -a "error" $O/errors.txt | sed 's|.*/atk/||' | cut -c1-190 | head -20
[ $fails -eq 0 ] || exit 0
rm -f /wk/out/libpheatk.a; ar rcs /wk/out/libpheatk.a $O/*.o; echo "libpheatk.a: $(ar t /wk/out/libpheatk.a | wc -l) members"
