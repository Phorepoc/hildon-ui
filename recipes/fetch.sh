#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: BSD-2-Clause
#
# fetch.sh -- clone the Maemo Leste sources the build uses into dl/ (gitignored, never copied into this repository), each at its
# commit in pins.sh: libhildon, libhildondesktop, hildon-home, sapwood. A clone already there is only checked out at the pin.
set -eu
cd "$(dirname "$0")/.."; . recipes/pins.sh; mkdir -p dl
for p in libhildon:$LIBHILDON libhildondesktop:$LIBHILDONDESKTOP hildon-home:$HILDON_HOME sapwood:$SAPWOOD hildon-control-panel:$HILDON_CONTROL_PANEL; do
  n=${p%%:*}; c=${p#*:}
  [ -d "dl/$n/.git" ] || git clone -q "https://github.com/maemo-leste/$n.git" "dl/$n"
  git -C "dl/$n" checkout -q "$c"
  echo "$n at $c"
done
