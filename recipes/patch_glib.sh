#!/usr/bin/env bash
# patch_glib.sh -- x32 source patches to GLib 2.56.4 (idempotent). Run in the container on /wk/glib-2.56.4.
#  guniprop.c: `special_case_table + val - 0x1000000` is folded by the compiler into (table - 0x1000000) + val, a NEGATIVE link-time
#  addend that an R_X86_64_32 relocation (x32 absolute address) cannot hold. An empty asm hides the base from the folder.
set -eu
F=/wk/glib-2.56.4/glib/guniprop.c
grep -q PHE_X32 $F || sed -i 's|const gchar \*p = special_case_table + val - 0x1000000;|const gchar *p; { const gchar *tb_ = special_case_table; __asm__ ("" : "+r" (tb_)); p = tb_ + val - 0x1000000; } /* PHE_X32 */|' $F
true
# GWakeup / g_poll for Open C (no eventfd, pipe, socketpair): overlay/phe_gwakeup.c replaces gwakeup.c, and the select()-based g_poll of
# gpoll.c is renamed phe_g_poll_select and wrapped (overlay/phe_gpoll_tail.c appended).
G=/wk/glib-2.56.4/glib
if ! grep -q phe_g_poll_select $G/gpoll.c; then
  cp $G/gwakeup.c $G/gwakeup.c.orig; cp /h/overlay/phe_gwakeup.c $G/gwakeup.c
  python3 - <<'PY'
p = "/wk/glib-2.56.4/glib/gpoll.c"
s = open(p, newline="").read()
i = s.rindex("gint\ng_poll (GPollFD *fds,")
s = s[:i] + "static gint\nphe_g_poll_select (GPollFD *fds," + s[i + len("gint\ng_poll (GPollFD *fds,"):]
j = s.rindex("#endif /* !G_OS_WIN32 */")
tail = open("/h/overlay/phe_gpoll_tail.c", newline="").read()
s = s[:j] + tail + "\n" + s[j:]
open(p, "w", newline="\n").write(s)
PY
fi
# the replaced gpoll.c tail uses __builtin_alloca (no alloca.h in Open C); re-apply when an older patch used g_alloca
sed -i 's/g_alloca (/__builtin_alloca (/g' /wk/glib-2.56.4/glib/gpoll.c
