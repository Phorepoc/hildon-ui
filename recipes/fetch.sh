#!/usr/bin/env bash
# fetch.sh -- download + verify the pinned tarballs into dl/ (gitignored)
set -eu
cd "$(dirname "$0")/.."; . recipes/pins.sh; mkdir -p dl
for p in GLIB FFI PIXMAN CAIRO FREETYPE HARFBUZZ EXPAT FONTCONFIG PANGO ZLIB LIBPNG GDKPIXBUF GTK; do u=$(eval echo \$${p}_URL); s=$(eval echo \$${p}_SHA256); f=dl/$(basename "$u")
  [ -f "$f" ] || curl -sSfL -o "$f" "$u"; echo "$s  $f" | sha256sum -c -; done
