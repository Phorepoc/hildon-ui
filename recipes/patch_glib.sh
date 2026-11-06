#!/usr/bin/env bash
# patch_glib.sh -- x32 source patches to GLib 2.56.4 (idempotent). Run in the container on /wk/glib-2.56.4.
#  guniprop.c: `special_case_table + val - 0x1000000` is folded by the compiler into (table - 0x1000000) + val, a NEGATIVE link-time
#  addend that an R_X86_64_32 relocation (x32 absolute address) cannot hold. An empty asm hides the base from the folder.
set -eu
F=/wk/glib-2.56.4/glib/guniprop.c
grep -q PHE_X32 $F || sed -i 's|const gchar \*p = special_case_table + val - 0x1000000;|const gchar *p; { const gchar *tb_ = special_case_table; __asm__ ("" : "+r" (tb_)); p = tb_ + val - 0x1000000; } /* PHE_X32 */|' $F
grep -c PHE_X32 $F
