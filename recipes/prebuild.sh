#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: BSD-2-Clause
#
# prebuild.sh -- the pdk's package.yml runs it in the toolchain container before the components: the GLib, GTK and D-Bus layers (the
# stack_*.sh recipes of glib-symbian, gtk-symbian, dbus-symbian), then (recipes/fetch.sh: the pinned Leste clones) libhildon, libhildondesktop, the Home, sapwood and the demo as
# the archives under $PHOREPOC_BUILD/hildon/ that registry/ links.
set -eu
HERE=$(cd "$(dirname "$0")/.." && pwd)
glib_dir() {
  local r n i
  IFS=: read -r -a r <<< "${PHOREPOC_ADAPTATIONS:-}"; IFS=: read -r -a n <<< "${PHOREPOC_ADAPTATION_NAMES:-}"
  for i in "${!r[@]}"; do [ "${n[i]:-$(basename "${r[i]}")}" = glib-symbian ] && { echo "${r[i]}"; return 0; }; done
  echo "glib-symbian is not in PHOREPOC_ADAPTATIONS" >&2; return 1
}
. "$(glib_dir)/recipes/stack_env.sh"
. "$(adaptation_dir gtk-symbian)/recipes/stack_gtk.sh"
. "$(adaptation_dir dbus-symbian)/recipes/stack_dbus.sh"
stack_glib
stack_gtk
stack_dbus
stack_use hildon-ui
stack_step hildon fetch "bash /h/recipes/fetch.sh"
stack_step hildon hildon "bash /h/recipes/build_hildon_x32.sh"
stack_step hildon hildondesktop "bash /h/recipes/build_hildondesktop_x32.sh"
stack_step hildon hildonhome "bash /h/recipes/build_hildonhome_x32.sh"
stack_step hildon sapwood "bash /h/recipes/build_sapwood_x32.sh"
stack_step hildon archives "bash /h/recipes/package_x32.sh"
stack_need libphehildon.a libphehildondesktop.a libphehome.a libphehomeexample.a libsapwood.a libphehildonwin.a \
  gmodule/sapwood.sym gmodule/phehomeexample.sym testdata/phehildon.rc testdata/phehildon_beep.wav testdata/phehildonwin_fonts.conf
