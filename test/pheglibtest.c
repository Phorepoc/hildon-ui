/* pheglibtest.c -- console test of the x32/Open C GLib 2.56.4 build: g_print, GMainLoop + timeout, a GObject subclass with a
 * signal, a GThread, and one of GLib's own test programs (glib/tests/hash.c, run as phe_glib_test_hash). Output goes to the
 * serial log through phe_log (RDebug). Licence of this file: BSD-2-Clause. */
#include <glib.h>
#include <glib-object.h>
#include <string.h>
#include <stdio.h>

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
  GRegex *re = g_regex_new ("(\d+)-(\d+)", 0, 0, NULL); GMatchInfo *mi; CHECK (g_regex_match (re, "range 10-20 ok", 0, &mi));
  gchar *m2 = g_match_info_fetch (mi, 2); CHECK (m2 && strcmp (m2, "20") == 0); g_print ("regex: 2nd group %s\n", m2); g_free (m2);
  g_match_info_free (mi); g_regex_unref (re);

  snprintf (b, sizeof b, "own tests done, fails=%d", fails); phe_log (b);
  /* GLib's own test: glib/tests/hash.c */
  char *argv[] = { (char *) "hash", NULL };
  int rc = phe_glib_test_hash (1, argv);
  snprintf (b, sizeof b, "glib/tests/hash.c rc=%d", rc); phe_log (b);
  return fails + rc;
}
