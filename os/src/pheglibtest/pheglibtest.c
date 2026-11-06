/* pheglibtest.c -- console test of the x32/Open C GLib 2.56.4 build: g_print, GMainLoop + timeout, a GObject subclass with a
 * signal, a GThread, and one of GLib's own test programs (glib/tests/hash.c, run as phe_glib_test_hash). Output goes to the
 * serial log through phe_log (RDebug). Licence of this file: BSD-2-Clause. */
#include <glib.h>
#include <glib-object.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <spawn.h>
#include <sys/wait.h>
#include <sys/time.h>

extern void phe_log (const char *msg);
extern int phe_glib_test_hash (int argc, char **argv);

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; char b[160]; snprintf (b, sizeof b, "FAIL %s:%d %s", __FILE__, __LINE__, #c); phe_log (b); } } while (0)

static void print_handler (const gchar *s) { char b[300]; size_t n = strlen (s);
  if (n && s[n - 1] == '\n') n--; if (n > 280) n = 280; memcpy (b, s, n); b[n] = 0; phe_log (b); }
static void log_handler (const gchar *d, GLogLevelFlags l, const gchar *m, gpointer u) { char b[300];
  snprintf (b, sizeof b, "log[%s/%d] %s", d ? d : "-", (int) l, m); phe_log (b); }

/* --- main loop --- */
static int ticks; static GMainLoop *loop;
static gboolean on_tick (gpointer d) { ticks++; if (ticks >= 3) { g_main_loop_quit (loop); return FALSE; } return TRUE; }

/* --- GObject subclass with a signal --- */
typedef struct { GObject parent; int value; } PheCounter;
typedef struct { GObjectClass parent_class; void (*bumped) (PheCounter *c, int v); } PheCounterClass;
G_DEFINE_TYPE (PheCounter, phe_counter, G_TYPE_OBJECT)
static guint bumped_sig; static int bumped_seen;
static void phe_counter_init (PheCounter *c) { c->value = 0; }
static void phe_counter_class_init (PheCounterClass *k)
{ bumped_sig = g_signal_new ("bumped", G_TYPE_FROM_CLASS (k), G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (PheCounterClass, bumped),
                             NULL, NULL, g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT); }
static void on_bumped (PheCounter *c, int v, gpointer d) { bumped_seen += v; }

/* --- thread --- */
static gpointer worker (gpointer d) { int *p = d; g_usleep (20000); *p = 42; return GINT_TO_POINTER (7); }

int phe_glib_test_run (void)
{
  char b[200];
  { /* probe what Open C offers for GLib's main-loop wakeup */
    int fds[2], rc, e; char pb[200];
    errno = 0; rc = pipe (fds); e = errno; snprintf (pb, sizeof pb, "probe pipe rc=%d errno=%d fds=%d,%d", rc, e, fds[0], fds[1]); phe_log (pb);
    if (rc == 0) { errno = 0; rc = fcntl (fds[0], F_SETFD, FD_CLOEXEC); e = errno; snprintf (pb, sizeof pb, "probe fcntl rc=%d errno=%d", rc, e); phe_log (pb); }
    if (rc == 0) { fd_set rs; struct timeval tv; char rb[8]; int w, n, s1, s2; w = (int) write (fds[1], "abc", 3);
      FD_ZERO (&rs); FD_SET (fds[0], &rs); tv.tv_sec = 0; tv.tv_usec = 100000; s1 = select (fds[0] + 1, &rs, NULL, NULL, &tv);
      n = (int) read (fds[0], rb, sizeof rb);
      FD_ZERO (&rs); FD_SET (fds[0], &rs); tv.tv_sec = 0; tv.tv_usec = 50000; s2 = select (fds[0] + 1, &rs, NULL, NULL, &tv);
      snprintf (pb, sizeof pb, "probe pipe roundtrip: wrote=%d select(data)=%d read=%d [%.3s] select(empty)=%d", w, s1, n, n == 3 ? rb : "", s2); phe_log (pb); }
    errno = 0; rc = socketpair (AF_UNIX, SOCK_STREAM, 0, fds); e = errno; snprintf (pb, sizeof pb, "probe socketpair rc=%d errno=%d fds=%d,%d", rc, e, fds[0], fds[1]); phe_log (pb);
  }
  g_set_print_handler (print_handler);
  g_log_set_default_handler (log_handler, NULL);
  g_print ("pheglibtest: GLib %u.%u.%u, sizeof(gsize)=%u sizeof(gint64)=%u sizeof(gpointer)=%u\n", glib_major_version,
           glib_minor_version, glib_micro_version, (unsigned) sizeof (gsize), (unsigned) sizeof (gint64), (unsigned) sizeof (gpointer));
  CHECK (sizeof (gsize) == 4 && sizeof (gint64) == 8);

  GString *s = g_string_new ("hello"); g_string_append_printf (s, " %d %s %" G_GINT64_FORMAT, 5, "x", (gint64) 1234567890123LL);
  g_print ("string: %s\n", s->str); CHECK (strcmp (s->str, "hello 5 x 1234567890123") == 0); g_string_free (s, TRUE);

  loop = g_main_loop_new (NULL, FALSE); g_timeout_add (30, on_tick, NULL); g_main_loop_run (loop); g_main_loop_unref (loop);
  g_print ("mainloop: %d timeout ticks\n", ticks); CHECK (ticks == 3);

  PheCounter *c = g_object_new (phe_counter_get_type (), NULL);
  g_signal_connect (c, "bumped", G_CALLBACK (on_bumped), NULL);
  g_signal_emit (c, bumped_sig, 0, 5); g_signal_emit (c, bumped_sig, 0, 6);
  g_print ("gobject: type %s, signal seen %d\n", G_OBJECT_TYPE_NAME (c), bumped_seen); CHECK (bumped_seen == 11); g_object_unref (c);

  int flag = 0; GThread *t = g_thread_new ("phe-worker", worker, &flag); gpointer r = g_thread_join (t);
  g_print ("gthread: joined, flag=%d ret=%d\n", flag, GPOINTER_TO_INT (r)); CHECK (flag == 42 && GPOINTER_TO_INT (r) == 7);

  GKeyFile *kf = g_key_file_new (); GError *e = NULL;
  CHECK (g_key_file_load_from_data (kf, "[a]\nk=v\nn=12\n", -1, G_KEY_FILE_NONE, &e));
  gchar *v = g_key_file_get_string (kf, "a", "k", NULL); CHECK (v && strcmp (v, "v") == 0); g_free (v);
  CHECK (g_key_file_get_integer (kf, "a", "n", NULL) == 12); g_key_file_free (kf);
  { const char *pats[] = { "abc", "[0-9]+", "\\d+", "(\\d+)-(\\d+)" }; int pi; for (pi = 0; pi < 4; pi++) { GError *ge = NULL; GRegex *r0 = g_regex_new (pats[pi], 0, 0, &ge); GMatchInfo *m0 = NULL; gboolean ok = r0 && g_regex_match (r0, "xx abc 10-20 ok", 0, &m0); snprintf (b, sizeof b, "regex probe %s: new=%s match=%\d err=%s", pats[pi], r0 ? "ok" : "NULL", ok, ge ? ge->message : "-"); phe_log (b); } }
  GRegex *re = g_regex_new ("(\\d+)-(\\d+)", 0, 0, NULL); GMatchInfo *mi; CHECK (g_regex_match (re, "range 10-20 ok", 0, &mi));
  gchar *m2 = g_match_info_fetch (mi, 2); CHECK (m2 && strcmp (m2, "20") == 0); g_print ("regex: 2nd group %s\n", m2); g_free (m2);
  g_match_info_free (mi); g_regex_unref (re);

  snprintf (b, sizeof b, "own tests done, fails=%d", fails); phe_log (b);
  /* GLib's own test programs, each in a CHILD process (posix_spawn; a test program calls g_test_init once and exits). The parent logs
   * every child's exit status. hash.c spawns grandchildren itself (g_test_trap_subprocess). */
  {
    { /* probe: a child that exits, its stdout pipe, and what waitpid / select / read say afterwards (pipe EOF) */
      pid_t pp = 0; int pf[2], pst = -1, pr, wr = -1, s1 = -9, s2 = -9, rd = -9, rdyr = 0, rdyx = 0; char rb[4]; fd_set rs, xs; struct timeval tv;
      posix_spawn_file_actions_t fa; char *pav[] = { (char *) "pheglibtest.exe", (char *) "--phe-run", (char *) "exit0", NULL };
      if (pipe (pf) == 0) { posix_spawn_file_actions_init (&fa); posix_spawn_file_actions_adddup2 (&fa, pf[1], 1);
        pr = posix_spawn (&pp, "pheglibtest.exe", &fa, NULL, pav, NULL); close (pf[1]);
        if (pr == 0) wr = (int) waitpid (pp, &pst, 0);
        FD_ZERO (&rs); FD_SET (pf[0], &rs); tv.tv_sec = 0; tv.tv_usec = 500000; s1 = select (pf[0] + 1, &rs, NULL, NULL, &tv); rdyr = FD_ISSET (pf[0], &rs) ? 1 : 0;
        FD_ZERO (&rs); FD_ZERO (&xs); FD_SET (pf[0], &rs); FD_SET (pf[0], &xs); tv.tv_sec = 0; tv.tv_usec = 500000; s2 = select (pf[0] + 1, &rs, NULL, &xs, &tv); rdyx = (FD_ISSET (pf[0], &rs) ? 1 : 0) | (FD_ISSET (pf[0], &xs) ? 2 : 0);
        fcntl (pf[0], F_SETFL, O_NONBLOCK); rd = (int) read (pf[0], rb, sizeof rb);
        snprintf (b, sizeof b, "probe pipe-EOF: spawn=%d waitpid=%d status=%d | select(read only)=%d readable=%d | select(read+except)=%d bits=%d (1=read 2=except) | read=%d errno=%d", pr, wr, pst, s1, rdyr, s2, rdyx, rd, errno); phe_log (b); } }
    static const char *names[] = { "list", "hash", "slist", "queue", "array", "strfuncs", "keyfile", "mainloop" };
    unsigned i; int total_bad = 0;
    for (i = 0; i < sizeof names / sizeof names[0]; i++)
      {
        gchar *av[] = { (gchar *) "pheglibtest.exe", (gchar *) "--phe-run", (gchar *) names[i], NULL };
        gchar *so = NULL, *se = NULL; gint st = -1; GError *e2 = NULL;
        gboolean ok = g_spawn_sync (NULL, av, NULL, 0, NULL, NULL, &so, &se, &st, &e2);
        snprintf (b, sizeof b, "glib/tests/%s: spawn=%d exit_status=%d %s", names[i], ok, st, e2 ? e2->message : "");
        phe_log (b); if (!ok || st != 0) total_bad++;
        g_free (so); g_free (se); if (e2) g_error_free (e2);
      }
    snprintf (b, sizeof b, "glib tests: %d of %u programs not clean", total_bad, (unsigned) (sizeof names / sizeof names[0])); phe_log (b);
    fails += total_bad;
  }
  return fails;
}

/* child mode: pheglibtest.exe --phe-run <name> [args], or a GTestSubprocess re-run of hash.c */
extern int phe_glib_test_list (int, char **), phe_glib_test_slist (int, char **), phe_glib_test_queue (int, char **),
  phe_glib_test_array (int, char **), phe_glib_test_strfuncs (int, char **), phe_glib_test_keyfile (int, char **),
  phe_glib_test_mainloop (int, char **);
int phe_glib_child (int argc, char **argv)
{
  int i; const char *name = NULL; char *cargv[16]; int cargc = 0;
  for (i = 1; i < argc; i++) if (!strcmp (argv[i], "--phe-run") && i + 1 < argc) { name = argv[i + 1]; i++; }
                             else if (cargc < 14) cargv[++cargc] = argv[i];
  cargv[0] = argv[0]; cargc++; cargv[cargc] = NULL;
  if (name && !strcmp (name, "exit0")) return 0;
  if (!name)   /* a GTestSubprocess re-run (-p /<program>/<case>/subprocess): the first path segment names the program */
    { static char seg[24]; for (i = 1; i < argc; i++) if (argv[i][0] == (char) 0x2f) { const char *q = argv[i] + 1; unsigned k = 0; while (*q && *q != (char) 0x2f && k < sizeof seg - 1) seg[k++] = *q++; seg[k] = 0; name = seg; break; } }
  if (!name) name = "hash";
  g_set_print_handler (print_handler); g_log_set_default_handler (log_handler, NULL);
  if (!strcmp (name, "hash")) return phe_glib_test_hash (cargc, cargv);
  if (!strcmp (name, "list")) return phe_glib_test_list (cargc, cargv);
  if (!strcmp (name, "slist")) return phe_glib_test_slist (cargc, cargv);
  if (!strcmp (name, "queue")) return phe_glib_test_queue (cargc, cargv);
  if (!strcmp (name, "array")) return phe_glib_test_array (cargc, cargv);
  if (!strcmp (name, "strfuncs")) return phe_glib_test_strfuncs (cargc, cargv);
  if (!strcmp (name, "keyfile")) return phe_glib_test_keyfile (cargc, cargv);
  if (!strcmp (name, "mainloop")) return phe_glib_test_mainloop (cargc, cargv);
  return 99;
}
