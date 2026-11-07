#!/usr/bin/env bash
# build_gtk_x32.sh -- IN phorepoc-x86 (volume at /wk, repo :ro at /repo, this repo at /h): GTK+ 2.24.33 (gtk/*.c) for x32 over Open C, from the pinned tarball, on top of the GDK of
# build_gdk_x32.sh + build_gdk_backend_x32.sh and ATK (build_atk_x32.sh). The release tarball ships every generated source (gtkmarshal*.c, gtktypebuiltins.c, gtkbuiltincache.h, gtkversion.h);
# the translation units are the ones gtk/Makefile.am lists as gtk_base_c_sources plus the *-stub.c ones of a backend without XEMBED (GTK's own, for backends such as DirectFB), plus the Unix
# printing ones. There is no configure: gtk/config.h below is written from what the Open C headers provide. Out: /wk/gtk-x32/*.o, /wk/gtk-x32/errors.txt (compile errors by file),
# /wk/gtk-x32/undefined.txt (symbols GTK needs that GLib/GDK/ATK/Pango/cairo/pixbuf do not define = what is still missing for the link).
set -eu
. /h/recipes/pins.sh
CC=/wk/lsc/bin/pp-gcc
cd /wk; [ -f gtk+-$GTK_VER/gtk/gtkmarshalers.c ] || tar xf /h/dl/gtk+-$GTK_VER.tar.xz gtk+-$GTK_VER/gtk gtk+-$GTK_VER/config.h.win32 gtk+-$GTK_VER/configure.ac
G=/wk/gtk+-$GTK_VER; O=/wk/gtk-x32; mkdir -p $O
# the source list: the make variables of gtk/Makefile.am, read as make would (continuation lines, no conditionals involved for these three)
list() { awk -v v="$1" '$0 ~ "^"v" *(\\+?=)" {on=1; sub(/^[^=]*=/,""); } on { line=$0; cont = (line ~ /\\[ \t]*$/); gsub(/\\[ \t]*$/,"",line); print line; if (!cont) on=0 }' $G/gtk/Makefile.am | tr -s ' \t' '\n\n' | grep "\.c$" || true; }
{ list gtk_base_c_sources; list gtk_use_stub_c_sources; list gtk_os_unix_c_sources; list gtk_clipboard_dnd_c_sources | grep -v quartz; } | sort -u > $O/sources.txt
echo "gtk: $(wc -l < $O/sources.txt) translation units"
cat > $O/config.h <<EOC
#define GETTEXT_PACKAGE "gtk20"
#define GTK_LOCALEDIR "z:/resource/locale"
#define GTK_DATA_PREFIX "z:/resource/gtk"
#define GTK_LIBDIR "z:/resource/gtk/lib"
#define GTK_SYSCONFDIR "z:/resource/gtk/etc"
#define GTK_BINARY_VERSION "2.10.0"
#define GTK_HOST "i686-pc-symbian"
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_UNISTD_H 1
#define HAVE_FCNTL_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_PWD_H 1
#define HAVE_MMAP 1
#define GTK_PRINT_BACKENDS "file,lpr"
#define GTK_PRINT_PREVIEW_COMMAND "evince --unlink-tempfile --preview --print-settings %s %f"
#define HAVE_FLOCKFILE 1
#define HAVE_FUNLOCKFILE 1
EOC
X32="-I/wk/x32 -I/wk/x32/glib -I/wk/glib-2.56.4 -I/wk/glib-2.56.4/glib -I/wk/glib-2.56.4/gobject -I/wk/glib-2.56.4/gmodule -I/wk/glib-2.56.4/gio -I/wk/ffi-x32/include"
FF="-O2 -g0 -w -fno-pic -I/h/overlay -include /h/overlay/phe_glib_compat.h -include /wk/gdk-x32/gtkcompat.h -DHAVE_CONFIG_H -DGTK_COMPILATION -DG_LOG_DOMAIN=\"Gtk\" -DDISABLE_VISIBILITY -DGDK_DISABLE_DEPRECATION_WARNINGS \
 -DGTK_DISABLE_DEPRECATION_WARNINGS -DGTK_FILE_SYSTEM_ENABLE_UNSUPPORTED -DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED -DGTK_ENABLE_BROKEN \
 -I$O -I$G -I$G/gtk -I$G/gdk -I/wk/gdk-x32 -I/wk/atk-$ATK_VER -I/wk/atk-x32 $X32 -I/wk/cairo-1.16.0/src -I/wk/cairo-x32/src -I/wk/pixman-0.42.2/pixman -I/wk/pango-1.40.14 -I/wk/pango-x32 \
 -I/wk/harfbuzz-$HARFBUZZ_VER/src -I/wk/fontconfig-2.14.2 -I/wk/freetype-2.13.3/include -I/wk/gdk-pixbuf-2.36.12 -I/wk/pixbuf-x32"
# every public header of ATK / GDK is reached as <atk/atk.h> / <gdk/gdk.h> / <gtk/gtk.h>: a include tree of symlinks
mkdir -p $O/inc; ln -sfn /wk/atk-$ATK_VER/atk $O/inc/atk; ln -sfn $G/gdk $O/inc/gdk; ln -sfn $G/gtk $O/inc/gtk; ln -sfn /wk/gdk-pixbuf-2.36.12/gdk-pixbuf $O/inc/gdk-pixbuf
FF="$FF -I$O/inc -I/wk/atk-$ATK_VER"
rm -f $O/*.o $O/errs.*; export CC FF O G
# gtkoldeditable.h includes <gtk/gtk.h>, whose gtktext.h needs the types declared after that include: the file needs gtk.h seen first
one() { f=$1; b=${f%.c}; x=""; [ $b = gtkoldeditable ] && x="-include gtk/gtk.h"; $CC $FF $x -c $G/gtk/$f -o $O/$b.o 2> $O/errs.$b || echo "FAIL $b"; }
export -f one
fails=$(xargs -P "${JOBS:-8}" -I{} bash -c 'one {}' < $O/sources.txt | tee $O/failed.txt | wc -l)
cat $O/errs.* > $O/errors.txt 2>/dev/null || true; rm -f $O/errs.*
echo "gtk: $(ls $O/*.o 2>/dev/null | wc -l) objects, $fails failed"
head -40 $O/failed.txt | tr '\n' ' '; echo
grep -a "error" $O/errors.txt | sed "s|$G/||" | cut -c1-200 | head -${ERRN:-30}
GO=$(ls /wk/gdk-x32/*.o /wk/gdk-x32/be/*.o /wk/atk-x32/*.o)
nm -u $O/*.o | grep " U " | awk '{print $2}' | sort -u > $O/und_all.txt
nm --defined-only $O/*.o $GO | awk 'NF==3{print $3}' | sort -u > $O/def_all.txt
comm -23 $O/und_all.txt $O/def_all.txt | grep -E "^(_?gtk_|gtk_|_?gdk_|atk_)" > $O/undefined.txt || true
grep -v "^gdk_pixbuf\|^gdk_pixdata\|^gdk_colorspace\|^gdk_interp" $O/undefined.txt > $O/undefined-own.txt; echo "undefined gtk_/gdk_/atk_ symbols (the pixbuf ones are phepixbuf's): $(wc -l < $O/undefined-own.txt)"; head -80 $O/undefined-own.txt | tr '\n' ' '
