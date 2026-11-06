#!/usr/bin/env bash
# patch_gio.sh -- x32 / Open C source patches to GIO (GLib 2.56.4), idempotent; run in the container before the x32 compile of gio.
set -eu
G=/wk/glib-2.56.4/gio
# no mount table on Open C: empty implementations instead of the #error of the system switch
grep -q PHE_NOMOUNTS $G/gunixmounts.c || python3 - <<'PY'
p = "/wk/glib-2.56.4/gio/gunixmounts.c"
s = open(p, newline="").read()
a = "#error No _g_get_unix_mounts() implementation for system"
b = "#error No g_get_mount_table() implementation for system"
assert a in s and b in s
s = s.replace(a, "/* PHE_NOMOUNTS */\nstatic GList *\n_g_get_unix_mounts (void)\n{\n  return NULL;\n}")
s = s.replace(b, "static GList *\n_g_get_unix_mount_points (void)\n{\n  return NULL;\n}")
open(p, "w", newline="\n").write(s)
PY
# the netlink network monitor is Linux-only (HAVE_NETLINK is off): compile the file to nothing
grep -q PHE_NONETLINK $G/gnetworkmonitornetlink.c || { { echo '/* PHE_NONETLINK */'; echo '#include "config.h"'; echo '#ifdef HAVE_NETLINK'; cat $G/gnetworkmonitornetlink.c; echo '#endif'; } > $G/gnetworkmonitornetlink.c.new; mv $G/gnetworkmonitornetlink.c.new $G/gnetworkmonitornetlink.c; }
# Open C's headers define `environ` as a macro; GApplicationCommandLine has a struct field of that name: include the libc headers first, then undefine it
sed -i '/PHE_ENVIRON/d' $G/gapplicationcommandline.c
sed -i '0,/^#include "config.h"/s//#include "config.h"\n#include <stdlib.h>\n#include <unistd.h>\n#include <string.h>\n#undef environ \/* PHE_ENVIRON *\//' $G/gapplicationcommandline.c
grep -c "PHE_" $G/gunixmounts.c $G/gnetworkmonitornetlink.c $G/gapplicationcommandline.c
