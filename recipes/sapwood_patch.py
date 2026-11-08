# sapwood_patch.py DIR -- the patches of build_sapwood_x32.sh to the upstream engine sources copied into DIR: no X-pixmap masks (shaped windows and 1-bit masks are not supported), so
# the mask the renderer would create is NULL, and the shape code of sapwood-style.c is off.
import sys

d = sys.argv[1]
p = d + '/theme-pixbuf.c'
s = open(p).read()
s = s.replace('mask = gdk_pixmap_new (NULL, mask_width, mask_height, 1);', 'mask = NULL;')
s = s.replace('mask = gdk_pixmap_new (NULL, pixbuf_width, pixbuf_height, 1);', 'mask = NULL;')
s = s.replace('g_object_unref (mask);', 'if (mask) g_object_unref (mask);')
s = s.replace('if (gdk_error_trap_pop ())', 'if (0)')
if 'sapwood-pixmap.h' not in s:
    s = s.replace('#include "theme-pixbuf.h"', '#include "theme-pixbuf.h"\n#include "sapwood-pixmap.h"', 1)
open(p, 'w').write(s)
p = d + '/sapwood-style.c'
s = open(p).read()
s = s.replace('if (image->background_shaped &&', 'if (0 && image->background_shaped &&')
open(p, 'w').write(s)
print('sapwood patched')
