#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: BSD-2-Clause
#
# sis.sh -- the pdk's package.yml runs it after the components: Hildon as $PHOREPOC_BUILD/sis/hildon-ui-<arch>.sis (the DLLs, their resources, the licence texts and the
# source offer in \resource\hildon-ui\licences). The package needs: GLib, GTK, D-Bus (installed first, by UID and version); the Home's D-Bus service file comes with D-Bus (phedbusd's data).
# Installs through the Catalogue; no image carries it. Resources are found by the programs on whatever drive it was installed to (glib-symbian overlay/phe_respath.h).
set -eu
ARCH=${PHOREPOC_ARCH:-x32}
REPO=$(cd "$(dirname "$0")/.." && pwd)
LIC=
for f in "$REPO"/LICENSES/* "$REPO/LICENSE" "$REPO/NOTICE" "$REPO/SOURCE-OFFER.txt"; do
  LIC="$LIC --file $f=!:/resource/hildon-ui/licences/$(basename "$f")"
done
# --depends UID,0.1.0 = the lower layer at version >= 0.1.0 (its first release), no upper bound: any later release satisfies it
python3 "$PHOREPOC_PDK/tools/make_sis.py" phehildon phehildondesktop phehome phehomeexample sapwood --unsigned --version "${PHOREPOC_PACKAGE_VERSION:-1.0.0}" \
  --name "Hildon" --vendor Phorepoc --depends 0xE7015772,0.1.0,"GLib" --depends 0xE7015777,0.1.0,"GTK" --depends 0xE7015787,0.1.0,"D-Bus" $LIC \
  --out "$PHOREPOC_BUILD/sis/hildon-ui-$ARCH.sis"
# the libhildon demo (phehildonwin) as its own small package: the Catalogue's "Hildon demo", registered with the application menu
DLIC=
for f in "$REPO/LICENSE" "$REPO/NOTICE" "$REPO/SOURCE-OFFER.txt"; do
  DLIC="$DLIC --file $f=!:/resource/hildon-ui-demo/licences/$(basename "$f")"
done
python3 "$PHOREPOC_PDK/tools/make_sis.py" phehildonwin --unsigned --version "${PHOREPOC_PACKAGE_VERSION:-1.0.0}" \
  --name "Hildon demo" --vendor Phorepoc --depends 0xE7015786,0.1.0,"Hildon" $DLIC \
  --out "$PHOREPOC_BUILD/sis/hildon-ui-demo-$ARCH.sis"
