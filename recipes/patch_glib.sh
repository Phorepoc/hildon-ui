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
# output_special_case() (inlined into real_toupper/real_tolower) has the same folding: table + (val - 0x1000000)
grep -q PHE_X32B /wk/glib-2.56.4/glib/guniprop.c || sed -i 's|const gchar \*p = special_case_table + offset;|const gchar *p; { const gchar *tb_ = special_case_table; __asm__ ("" : "+r" (tb_)); p = tb_ + offset; } /* PHE_X32B */|' /wk/glib-2.56.4/glib/guniprop.c
# g_spawn over posix_spawn: the fork-based fork_exec_with_pipes (2nd occurrence = the definition) is renamed and ours appended (overlay/phe_fork_exec.c)
if ! grep -q phe_spawn_fail $G/gspawn.c; then
  python3 - <<'PY'
p = "/wk/glib-2.56.4/glib/gspawn.c"
s = open(p, newline="").read()
head = "static gboolean\nfork_exec_with_pipes (gboolean              intermediate_child,"
i = s.rindex(head)
s = s[:i] + "static G_GNUC_UNUSED gboolean\nfork_exec_with_pipes_unused (gboolean              intermediate_child," + s[i + len(head):]
s += open("/h/overlay/phe_fork_exec.c", newline="").read()
open(p, "w", newline="\n").write(s)
PY
fi
# GChildWatchSource: GLib reaps children on SIGCHLD, which Open C never delivers (waitpid itself works: ftable.cpp CFileTable::waitpid does
# RProcess::Logon on the child). The unix prepare/check poll waitpid(WNOHANG) instead, with a 20 ms poll timeout while the child lives.
if ! grep -q PHE_CHILD_POLL $G/gmain.c; then
  python3 - <<'PY'
p = "/wk/glib-2.56.4/glib/gmain.c"
s = open(p, newline="").read()
old_prep = "static gboolean\ng_child_watch_prepare (GSource *source,\n\t\t       gint    *timeout)\n{\n  GChildWatchSource *child_watch_source;\n\n  child_watch_source = (GChildWatchSource *) source;\n\n  return child_watch_source->child_exited;\n}\n\nstatic gboolean\ng_child_watch_check (GSource *source)\n{\n  GChildWatchSource *child_watch_source;\n\n  child_watch_source = (GChildWatchSource *) source;\n\n  return child_watch_source->child_exited;\n}\n"
assert old_prep in s
new = """/* PHE_CHILD_POLL */
static gboolean
phe_child_poll (GChildWatchSource *w)
{
  if (!w->child_exited)
    {
      pid_t r = waitpid (w->pid, &w->child_status, WNOHANG);
      if (r > 0) w->child_exited = TRUE;
      else if (r == -1 && errno == ECHILD) { w->child_exited = TRUE; w->child_status = 0; }
    }
  return w->child_exited;
}

static gboolean
g_child_watch_prepare (GSource *source,
\t\t       gint    *timeout)
{
  if (phe_child_poll ((GChildWatchSource *) source)) return TRUE;
  *timeout = 20;
  return FALSE;
}

static gboolean
g_child_watch_check (GSource *source)
{
  return phe_child_poll ((GChildWatchSource *) source);
}
"""
s = s.replace(old_prep, new)
open(p, "w", newline="\n").write(s)
PY
fi
