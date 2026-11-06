#!/usr/bin/env bash
# native_config.sh -- IN the phorepoc-x86 image (private volume hildon-wk mounted at /wk): unpack GLib and run its configure
# NATIVELY (the x32 glibc dev files are not in the image, so a cross configure cannot link-test). The resulting config.h is the
# starting point for the x32/Open C one (gen_config.py -- NOT WRITTEN YET, see README.md "Status"). Verified to configure OK.
#   docker run --rm -v <hildon-symbian>:/h -v hildon-wk:/wk phorepoc-x86 bash /h/recipes/native_config.sh
set -eu
. /h/recipes/pins.sh
cd /wk
tar xf /h/dl/glib-$GLIB_VER.tar.xz
mkdir -p nat && cd nat
# MSGFMT & co = /bin/true: the image has no gettext tools; NLS is switched off for the x32 build anyway
MSGFMT=/bin/true GMSGFMT=/bin/true XGETTEXT=/bin/true LIBFFI_CFLAGS=" " LIBFFI_LIBS="-lffi" \
 /wk/glib-$GLIB_VER/configure --with-pcre=internal --disable-libmount --disable-selinux --disable-fam --disable-xattr \
 --disable-dtrace --disable-systemtap --disable-gtk-doc --disable-man --disable-installed-tests --disable-always-build-tests \
 --with-libiconv=no > cfg.log 2>&1
ls config.h glib/glibconfig.h
