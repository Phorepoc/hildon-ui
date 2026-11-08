#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: BSD-2-Clause
#
# sis.sh -- the pdk's package.yml runs it after the components: Hildon as $PHOREPOC_BUILD/sis/hildon-symbian-<arch>.sis (the DLLs, their resources, the licence texts and the
# source offer in \resource\hildon-symbian\licences). The package needs: GLib, GTK, D-Bus (installed first, by UID and version).
# Installs through the Catalogue; no image carries it. Resources are found by the programs on whatever drive it was installed to (glib-symbian overlay/phe_respath.h).
set -eu
ARCH=${PHOREPOC_ARCH:-x32}
REPO=$(cd "$(dirname "$0")/.." && pwd)
LIC=
for f in "$REPO"/LICENSES/* "$REPO/LICENSE" "$REPO/NOTICE" "$REPO/SOURCE-OFFER.txt"; do
  LIC="$LIC --file $f=!:/resource/hildon-symbian/licences/$(basename "$f")"
done
python3 "$PHOREPOC_PDK/tools/make_sis.py" phehildon phehildondesktop phehome phehomeexample --unsigned --version "${PHOREPOC_PACKAGE_VERSION:-1.0.0}" \
  --name "Hildon" --vendor Phorepoc --depends 0xE7015772,1.0.0,"GLib" --depends 0xE7015777,1.0.0,"GTK" --depends 0xE7015787,1.0.0,"D-Bus" --file "$REPO/../dbus-symbian/os/data/dbus/services/com.nokia.HildonHome.service=!:/resource/dbus/services/com.nokia.HildonHome.service" $LIC \
  --out "$PHOREPOC_BUILD/sis/hildon-symbian-$ARCH.sis"
