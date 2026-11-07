/* pheglibtest.c -- console test of the x32/Open C GLib 2.56.4 build: g_print, GMainLoop + timeout, a GObject subclass with a
 * signal, a GThread, and one of GLib's own test programs (glib/tests/hash.c, run as phe_glib_test_hash). Output goes to the
 * serial log through phe_log (RDebug). Licence of this file: BSD-2-Clause. */
#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
static int phe_front_on;
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <spawn.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/time.h>

extern void phe_log (const char *msg);
extern int phe_glib_test_hash (int argc, char **argv);
extern void phe_kill_pid (int pid);

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

/* the first directory this OS lets a program create files in (what Open C paths and the test ROM's drives allow) */
static const char *phe_writable_dir (void)
{
  static const char *dirs[] = { "c:/data/", "c:/" };   /* drive paths: absolute to GLib since PHE_DRIVE (patch_glib.sh) */
  unsigned i; char p[64];
  for (i = 0; i < sizeof dirs / sizeof dirs[0]; i++)
    { int fd; snprintf (p, sizeof p, "%sphe_wd_probe", dirs[i]); fd = open (p, O_CREAT | O_WRONLY, 0600); if (fd >= 0) { close (fd); unlink (p); return dirs[i]; } }
  return "c:/";
}
/* log the tail of a child's stdout/stderr file (the harness redirects both there) */
static void phe_log_tail (const char *name)
{
  char of[96], buf[1500], ln[220]; int fd, n; off_t sz; char *p, *q;
  snprintf (of, sizeof of, "%sphe_child_%s.out", phe_writable_dir (), name);
  fd = open (of, O_RDONLY); if (fd < 0) { snprintf (ln, sizeof ln, "  [%s: no child output file]", name); phe_log (ln); return; }
  sz = lseek (fd, 0, SEEK_END); if (sz > (off_t) sizeof buf - 1) { lseek (fd, sz - (off_t) (sizeof buf - 1), SEEK_SET); } else lseek (fd, 0, SEEK_SET);
  n = (int) read (fd, buf, sizeof buf - 1); close (fd); if (n < 0) n = 0; buf[n] = 0;
  snprintf (ln, sizeof ln, "  [%s child output: %d bytes total, tail follows]", name, (int) sz); phe_log (ln);
  for (p = buf; *p; p = q) { q = strchr (p, '\n'); if (q) *q++ = 0; else q = p + strlen (p); if (*p) { snprintf (ln, sizeof ln, "  | %.200s", p); phe_log (ln); } }
}
/* run one test program as a child process: stdout+stderr through pipes (drained here, so a chatty child never blocks), exit status through waitpid,
 * a time limit with RProcess::Kill. Open C reports pipe EOF as an exception, so select() watches the exception set too. Returns 0 = exited, 1 = timeout, 2 = spawn failed. */
static int phe_run_child (const char *name, int limit_ms, char *out, int cap, int *status)
{
  int op[2], ep[2], rc, done = 0, ms = 0, olen = 0, drain = 0; pid_t pid = 0; posix_spawn_file_actions_t fa;
  char *av[] = { (char *) "pheglibtest.exe", (char *) "--phe-run", (char *) name, NULL };
  out[0] = 0; *status = -1;
  if (pipe (op) != 0 || pipe (ep) != 0) return 2;
  posix_spawn_file_actions_init (&fa); posix_spawn_file_actions_adddup2 (&fa, op[1], 1); posix_spawn_file_actions_adddup2 (&fa, ep[1], 2);
  rc = posix_spawn (&pid, "pheglibtest.exe", &fa, NULL, av, NULL); posix_spawn_file_actions_destroy (&fa);
  close (op[1]); close (ep[1]);
  if (rc != 0) { close (op[0]); close (ep[0]); return 2; }
  while (ms < limit_ms)
    {
      int fds[2], k, nopen = 0, maxfd = 0; fd_set rs, xs; struct timeval tv; fds[0] = op[0]; fds[1] = ep[0];
      FD_ZERO (&rs); FD_ZERO (&xs);
      for (k = 0; k < 2; k++) if (fds[k] >= 0) { FD_SET (fds[k], &rs); FD_SET (fds[k], &xs); nopen++; if (fds[k] > maxfd) maxfd = fds[k]; }
      if (nopen) { tv.tv_sec = 0; tv.tv_usec = 100000; if (select (maxfd + 1, &rs, NULL, &xs, &tv) > 0)
        for (k = 0; k < 2; k++) if (fds[k] >= 0 && (FD_ISSET (fds[k], &rs) || FD_ISSET (fds[k], &xs)))
          { char tmp[256]; int n = (int) read (fds[k], tmp, sizeof tmp);
            if (n > 0) { if (olen + n < cap - 1) { memcpy (out + olen, tmp, n); olen += n; out[olen] = 0; } }
            else { close (fds[k]); if (k == 0) op[0] = -1; else ep[0] = -1; } } }
      else g_usleep (100000);
      if (nopen) ms += 100; else ms += 100;
      { pid_t w = waitpid (pid, status, WNOHANG); if (w == pid) { done = 1; if (op[0] < 0 && ep[0] < 0) break; /* drain what is left, then stop */ }
        else if (w < 0) { done = 1; break; } }
      if (done && ms > 0 && op[0] < 0 && ep[0] < 0) break;
      if (done && ++drain > 20) break;
    }
  if (op[0] >= 0) close (op[0]); if (ep[0] >= 0) close (ep[0]);
  if (!done) { int k; phe_kill_pid ((int) pid); for (k = 0; k < 50; k++) { if (waitpid (pid, status, WNOHANG) == pid) break; g_usleep (100000); } return 1; }
  return 0;
}
static void phe_log_text (const char *label, char *text)
{
  char ln[260], *p, *q; snprintf (ln, sizeof ln, "  [%s: %d bytes of child output; tail follows]", label, (int) strlen (text)); phe_log (ln);
  p = text; if (strlen (text) > 1200) p = text + strlen (text) - 1200;
  for (; *p; p = q) { q = strchr (p, '\n'); if (q) *q++ = 0; else q = p + strlen (p); if (*p) { snprintf (ln, sizeof ln, "  | %.220s", p); phe_log (ln); } }
}
/* the bytes a trapped test child wrote to stderr, as kept by the patched g_test_trap_assertions (see patch_glib.sh) */
static void phe_log_trap_stderr (void)
{
  unsigned char tb[400]; char ln[1400], *o; int fd, n, i = 0; fd = open ("c:/data/phe_trap_stderr.bin", O_RDONLY); if (fd < 0) return;
  n = (int) read (fd, tb, sizeof tb); close (fd); if (n < 0) n = 0;
  snprintf (ln, sizeof ln, "  [trap stderr: %d bytes (first %d shown) as text then hex]", n, n); phe_log (ln);
  o = ln; *o = 0; for (i = 0; i < n; i++) *o++ = (tb[i] >= 32 && tb[i] < 127) ? (char) tb[i] : '.'; *o = 0; { char lt[1500]; snprintf (lt, sizeof lt, "  | %.1000s", ln); phe_log (lt); }
  o = ln; for (i = 0; i < n && i < 120; i++) o += snprintf (o, 8, "%02x ", tb[i]); phe_log (ln);
}
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

  { /* probe: /dev/null (Open C: the backend's null device) */
    int fd = open ("/dev/null", O_RDWR); char rb[8], pb4[240]; int rd = -9, wr = -9, sel = -9; fd_set rs, ws; struct timeval tv;
    if (fd >= 0) { rd = (int) read (fd, rb, sizeof rb); wr = (int) write (fd, "hello", 5);
      FD_ZERO (&rs); FD_ZERO (&ws); FD_SET (fd, &rs); FD_SET (fd, &ws); tv.tv_sec = 0; tv.tv_usec = 200000; sel = select (fd + 1, &rs, &ws, NULL, &tv);
      snprintf (pb4, sizeof pb4, "probe /dev/null: fd=%d read=%d write=%d select=%d readable=%d writable=%d", fd, rd, wr, sel, FD_ISSET (fd, &rs) ? 1 : 0, FD_ISSET (fd, &ws) ? 1 : 0); close (fd); }
    else snprintf (pb4, sizeof pb4, "probe /dev/null: open failed errno=%d", errno);
    phe_log (pb4); }
  { /* GIO: GFile read/write/query/delete and GKeyFile through a file, in a directory this OS lets us write */
    const char *wd = phe_writable_dir (); gchar *path = g_strconcat (wd, "phe_gio_test.txt", NULL); GFile *gf = g_file_new_for_path (path);
    GError *ge = NULL; gchar *content = NULL; gsize clen = 0; GFileInfo *fi; gboolean ok;
    snprintf (b, sizeof b, "gio: writable dir [%s], file [%s]", wd, path); phe_log (b);
    ok = g_file_replace_contents (gf, "hello gio", 9, NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL, &ge);
    snprintf (b, sizeof b, "gio: replace_contents=%d %s", ok, ge ? ge->message : "-"); phe_log (b); CHECK (ok); g_clear_error (&ge);
    ok = g_file_load_contents (gf, NULL, &content, &clen, NULL, &ge);
    snprintf (b, sizeof b, "gio: load_contents=%d len=%u [%.12s] %s", ok, (unsigned) clen, content ? content : "-", ge ? ge->message : "-"); phe_log (b);
    CHECK (ok && clen == 9 && memcmp (content, "hello gio", 9) == 0); g_clear_error (&ge); g_free (content);
    fi = g_file_query_info (gf, "standard::name,standard::size,standard::type", G_FILE_QUERY_INFO_NONE, NULL, &ge);
    snprintf (b, sizeof b, "gio: query_info name=%s size=%d type=%d %s", fi ? g_file_info_get_name (fi) : "-", fi ? (int) g_file_info_get_size (fi) : -1, fi ? (int) g_file_info_get_file_type (fi) : -1, ge ? ge->message : "-"); phe_log (b);
    CHECK (fi && g_file_info_get_size (fi) == 9); if (fi) g_object_unref (fi); g_clear_error (&ge);
    { GKeyFile *kf2 = g_key_file_new (); gchar *data; gsize dl; GKeyFile *kf3 = g_key_file_new (); gchar *back;
      g_key_file_set_string (kf2, "grp", "key", "value"); g_key_file_set_integer (kf2, "grp", "n", 42); data = g_key_file_to_data (kf2, &dl, NULL);
      ok = g_file_set_contents (path, data, dl, &ge); CHECK (ok); g_clear_error (&ge);
      ok = g_key_file_load_from_file (kf3, path, G_KEY_FILE_NONE, &ge); back = ok ? g_key_file_get_string (kf3, "grp", "key", NULL) : NULL;
      snprintf (b, sizeof b, "gio: GKeyFile through a file: load=%d key=[%s] n=%d %s", ok, back ? back : "-", ok ? g_key_file_get_integer (kf3, "grp", "n", NULL) : -1, ge ? ge->message : "-"); phe_log (b);
      CHECK (ok && back && !strcmp (back, "value")); g_clear_error (&ge); g_free (back); g_free (data); g_key_file_free (kf2); g_key_file_free (kf3); }
    ok = g_file_delete (gf, NULL, &ge); CHECK (ok); g_clear_error (&ge); CHECK (!g_file_query_exists (gf, NULL));
    snprintf (b, sizeof b, "gio: delete ok=%d exists_after=%d", ok, g_file_query_exists (gf, NULL)); phe_log (b);
    g_object_unref (gf); g_free (path);
  }
#ifdef PHE_HAVE_PIXMAN
  { extern int phe_pixman_test (void); int pf = phe_pixman_test (); if (pf) fails += pf; }
#endif
#ifdef PHE_HAVE_CAIRO
  { extern int phe_cairo_test (void); int cf = phe_cairo_test (); if (cf) fails += cf; }
#endif
#ifdef PHE_HAVE_PANGO
  { /* heap probe: grow many small blocks by realloc (the g_datalist pattern: 8 + 8n bytes) and touch every byte; a fault here = heap growth bug independent of GLib */
    enum { N = 3000 }; static char *blk[N]; int i, k, bad = 0; size_t sz;
    for (i = 0; i < N; i++) { blk[i] = (char *) malloc (8); if (blk[i]) memset (blk[i], 1, 8); }
    for (k = 2; k <= 6; k++) { sz = 8 + 8 * k; for (i = 0; i < N; i++) { char *q = (char *) realloc (blk[i], sz); if (!q) { bad++; continue; } blk[i] = q; memset (q, 2, sz); } }
    for (i = 0; i < N; i++) free (blk[i]);
    snprintf (b, sizeof b, "heap probe: %d blocks regrown to %u bytes, touched, failures %d", N, (unsigned) sz, bad); phe_log (b); }
#endif
#ifdef PHE_HAVE_PANGO
  { /* heap probe 2: shrink-then-grow at the heap top (RHeap trims the top on free): pin a small block above a big one, free the big one, grow again, touch everything */
    enum { M = 300 }; static char *pin[M]; int i, bad = 0; for (i = 0; i < M; i++) pin[i] = NULL;
    for (i = 0; i < M; i++) { size_t big = 150000 + (size_t) i * 997; char *p1 = (char *) malloc (big); char *q1 = (char *) malloc (24 + (i % 5) * 8);
      if (!p1 || !q1) { bad++; free (p1); free (q1); continue; } memset (p1, 3, big); memset (q1, 4, 24 + (i % 5) * 8); pin[i] = q1; free (p1);
      if (i % 3 == 0) { char *r1 = (char *) realloc (pin[i], 24 + 8 * 12); if (r1) { pin[i] = r1; memset (r1, 5, 24 + 8 * 12); } } }
    for (i = 0; i < M; i++) free (pin[i]);
    snprintf (b, sizeof b, "heap probe 2: %d shrink/grow rounds touched, failures %d", M, bad); phe_log (b); }
  setenv ("FONTCONFIG_FILE", "z:/resource/pheglibtest/fonts.conf", 1);
  { extern int phe_pango_test (void); int pg = phe_pango_test (); if (pg) fails += pg; }
#endif
#ifdef PHE_HAVE_PIXBUF
  { extern int phe_pixbuf_test (void); int pf2 = phe_pixbuf_test (); if (pf2) fails += pf2; }
#endif
  snprintf (b, sizeof b, "own tests done, fails=%d", fails); phe_log (b);
  /* GLib's own test programs, each in a CHILD process (posix_spawn; a test program calls g_test_init once and exits). The parent logs
   * every child's exit status. hash.c spawns grandchildren itself (g_test_trap_subprocess). */
  {
  { /* probe: doubles through libc snprintf and GLib's own printf (gnulib), and strtod round trips */
    double ds[5]; const char *nm[5] = { "DBL_MAX", "-DBL_MAX", "pow(2,-1024.1)", "0.1", "1e300" }; int i; char b1[64], b2[64], pb2[220];
    ds[0] = DBL_MAX;
    ds[1] = -ds[0]; ds[2] = pow (2.0, -1024.1); ds[3] = 0.1; ds[4] = 1e300;
    for (i = 0; i < 5; i++) { double back1, back2; snprintf (b1, sizeof b1, "%.17g", ds[i]); g_snprintf (b2, sizeof b2, "%.17g", ds[i]);
      back1 = strtod (b1, NULL); back2 = g_ascii_strtod (b2, NULL);
      snprintf (pb2, sizeof pb2, "probe dbl %s: libc[%s] strtod_back_eq=%d | glib[%s] g_ascii_strtod_back_eq=%d | same_string=%d", nm[i], b1, back1 == ds[i], b2, back2 == ds[i], strcmp (b1, b2) == 0); phe_log (pb2); } }
  { /* probe: where can a test write? */
    const char *paths[] = { "c:/phe_probe.txt", "c:\phe_probe.txt", "/tmp/phe_probe.txt", "c:/data/phe_probe.txt", "c:/private/phe_probe.txt" }; int i; char pb3[200];
    for (i = 0; i < 5; i++) { int fd; errno = 0; fd = open (paths[i], O_CREAT | O_WRONLY, 0600); snprintf (pb3, sizeof pb3, "probe open(%s) fd=%d errno=%d", paths[i], fd, errno); phe_log (pb3); if (fd >= 0) { close (fd); unlink (paths[i]); } }
    { char t1[] = "c:/phe_XXXXXX"; char t2[] = "/tmp/phe_XXXXXX"; int fd; errno = 0; fd = mkstemp (t1); snprintf (pb3, sizeof pb3, "probe mkstemp(c:/) fd=%d errno=%d [%s]", fd, errno, t1); phe_log (pb3); if (fd >= 0) { close (fd); unlink (t1); }
      errno = 0; fd = mkstemp (t2); snprintf (pb3, sizeof pb3, "probe mkstemp(/tmp) fd=%d errno=%d [%s]", fd, errno, t2); phe_log (pb3); if (fd >= 0) { close (fd); unlink (t2); }
      { GError *ge = NULL; gchar *nn = NULL; fd = g_file_open_tmp (NULL, &nn, &ge); snprintf (pb3, sizeof pb3, "probe g_file_open_tmp fd=%d name=%s err=%s tmpdir=%s", fd, nn ? nn : "-", ge ? ge->message : "-", g_get_tmp_dir ()); phe_log (pb3); if (fd >= 0) { close (fd); unlink (nn); } } } }
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
#ifdef PHE_ONLY
    static const char *names[] = { PHE_ONLY };
#else
    static const char *names[] = { "queue", "array", "strfuncs", "keyfile", "slist", "list", "hash", "mainloop" };
#endif
    unsigned i; int total_bad = 0;
    for (i = 0; i < sizeof names / sizeof names[0]; i++)
      {
        gchar *av[] = { (gchar *) "pheglibtest.exe", (gchar *) "--phe-run", (gchar *) names[i], NULL };
        char co[4000]; int st = -1, vr, lim = strcmp (names[i], "queue") == 0 ? 500000 : strcmp (names[i], "mainloop") == 0 ? 300000 : 100000; gint64 t0;
#ifdef PHE_LIMIT_MS
        lim = PHE_LIMIT_MS;
#endif
        unlink ("c:/data/phe_trap_stderr.bin"); t0 = g_get_monotonic_time ();
        vr = phe_run_child (names[i], lim, co, sizeof co, &st);
        snprintf (b, sizeof b, "  [%s: elapsed %d ms of a %d ms limit]", names[i], (int) ((g_get_monotonic_time () - t0) / 1000), lim); phe_log (b);
        if (vr == 2) { snprintf (b, sizeof b, "glib/tests/%s: spawn failed VERDICT=SPAWN_FAILED", names[i]); phe_log (b); total_bad++; continue; }
        if (vr == 1) { snprintf (b, sizeof b, "glib/tests/%s: no exit within its time limit, killed -- VERDICT=TIMEOUT", names[i]); phe_log (b); phe_log_text (names[i], co); total_bad++; continue; }
        snprintf (b, sizeof b, "glib/tests/%s: exit_status=%d (type %d reason %d) VERDICT=%s", names[i], st, st & 0xff, (st >> 8) & 0xff, st == 0 ? "PASS" : "FAIL"); phe_log (b);
        if (st != 0) { phe_log_text (names[i], co); phe_log_trap_stderr (); total_bad++; }
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
  setvbuf (stderr, NULL, _IONBF, 0);   /* a test child that aborts (int3) right after printing must not lose its last stderr bytes: seen as an intermittent /list/double-free failure, the trapped child's stderr was EMPTY */
  int i; const char *name = NULL; char *cargv[16]; int cargc = 0;
  for (i = 1; i < argc; i++) if (!strcmp (argv[i], "--phe-run") && i + 1 < argc) { name = argv[i + 1]; i++; }
                             else if (cargc < 14) cargv[++cargc] = argv[i];
  cargv[0] = argv[0]; cargc++; cargv[cargc] = NULL;
  { const char *wd0 = phe_writable_dir (); static char wdb[64]; snprintf (wdb, sizeof wdb, "%s", wd0); if (strlen (wdb) > 3) wdb[strlen (wdb) - 1] = 0; chdir (wdb); }   /* keyfile.c test_save makes a file in the CURRENT directory (g_mkstemp ("key_file_XXXXXX")) */
  setenv ("G_TEST_SRCDIR", "z:/resource/pheglibtest", 1); { static char td[64]; snprintf (td, sizeof td, "%s", phe_writable_dir ()); if (strlen (td) > 1) td[strlen (td) - 1] = 0; setenv ("TMPDIR", td, 1); } phe_front_on = name && (!strcmp (name, "strfuncs") || !strcmp (name, "keyfile") || !strcmp (name, "mainloop"));   /* the data files of the test programs (keyfile): rom_data of the registry key */
  if (name && !strcmp (name, "exit0")) return 0;
  if (name && !strcmp (name, "keyfile"))   /* why does g_file_open_tmp fail in keyfile.c test_save? */
    { char tb[300]; GError *te = NULL; gchar *tn = NULL; int tfd; const char *tv = getenv ("TMPDIR");
      tfd = g_file_open_tmp ("phediagXXXXXX", &tn, &te);
      snprintf (tb, sizeof tb, "keyfile tmp diag: getenv(TMPDIR)=[%s] g_get_tmp_dir=[%s] g_file_open_tmp fd=%d name=[%s] err=[%s]", tv ? tv : "(null)", g_get_tmp_dir (), tfd, tn ? tn : "-", te ? te->message : "-"); phe_log (tb);
      if (tfd >= 0) { close (tfd); unlink (tn); } }
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

/* g_test_init() replaced in the GLib test programs (package_x32.sh: -Dg_test_init=phe_g_test_init). g_test_init installs GLib's test log handler,
 * which prints to stderr and aborts on a fatal message -- in a child process of ours that output is lost. This wrapper puts a handler in front that
 * writes every message to the serial log first and then calls the one g_test_init installed. */
#include <stdarg.h>
static int phe_front_on; static GLogFunc phe_prev_log; static gpointer phe_prev_data;
static void phe_log_front (const gchar *d, GLogLevelFlags l, const gchar *m, gpointer u)
{ char b[400]; snprintf (b, sizeof b, "GLOG[%s/%d] %.330s", d ? d : "-", (int) l, m ? m : "(null)"); phe_log (b);
  if (phe_prev_log) phe_prev_log (d, l, m, phe_prev_data); }
void phe_g_test_init (int *argc, char ***argv, ...)
{
  g_test_init (argc, argv, NULL);
  if (!phe_front_on) return;   /* the front handler changes what a fatal message prints to stderr: only for the programs under diagnosis */
  phe_prev_log = g_log_set_default_handler (phe_log_front, NULL);
  if (!phe_prev_log) phe_prev_log = g_log_default_handler;   /* g_test_init left none: the default prints to stderr, which a parent test captures */
}
