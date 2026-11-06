/* phe_gwakeup.c -- GWakeup for Open C, replacing glib/gwakeup.c. Open C (P.I.P.S.) has neither eventfd, pipe() (ENOENT here) nor
 * socketpair(), so a wakeup is an in-process flag + one global condition variable; the patched g_poll (gpoll.c, see patch_glib.sh)
 * waits on that condition when no real descriptor is polled and slices select() otherwise. GPollFD.fd of a wakeup = a fake id >= PHE_WAKE_BASE.
 * BSD-2-Clause (Phorepoc); links against LGPL GLib. */
#include "config.h"
#include "gwakeup.h"
#include "glib.h"
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>

#define PHE_WAKE_BASE 0x40000000
#define PHE_WAKE_MAX 256
struct _GWakeup { int id; int signaled; };
static pthread_mutex_t wk_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wk_cond = PTHREAD_COND_INITIALIZER;
static GWakeup *wk_tab[PHE_WAKE_MAX];

GWakeup *g_wakeup_new (void)
{
  GWakeup *w = g_slice_new (GWakeup); int i;
  w->signaled = 0; w->id = -1;
  pthread_mutex_lock (&wk_lock);
  for (i = 0; i < PHE_WAKE_MAX; i++) if (!wk_tab[i]) { wk_tab[i] = w; w->id = PHE_WAKE_BASE + i; break; }
  pthread_mutex_unlock (&wk_lock);
  if (w->id < 0) g_error ("GWakeup: more than %d main contexts", PHE_WAKE_MAX);
  return w;
}
void g_wakeup_get_pollfd (GWakeup *w, GPollFD *fd) { fd->fd = w->id; fd->events = G_IO_IN; }
void g_wakeup_acknowledge (GWakeup *w) { pthread_mutex_lock (&wk_lock); w->signaled = 0; pthread_mutex_unlock (&wk_lock); }
void g_wakeup_signal (GWakeup *w) { pthread_mutex_lock (&wk_lock); w->signaled = 1; pthread_cond_broadcast (&wk_cond); pthread_mutex_unlock (&wk_lock); }
void g_wakeup_free (GWakeup *w) { pthread_mutex_lock (&wk_lock); wk_tab[w->id - PHE_WAKE_BASE] = NULL; pthread_mutex_unlock (&wk_lock); g_slice_free (GWakeup, w); }

int phe_wakeup_pending (int id)
{ int r = 0; if (id < PHE_WAKE_BASE || id >= PHE_WAKE_BASE + PHE_WAKE_MAX) return 0;
  pthread_mutex_lock (&wk_lock); if (wk_tab[id - PHE_WAKE_BASE]) r = wk_tab[id - PHE_WAKE_BASE]->signaled; pthread_mutex_unlock (&wk_lock); return r; }

/* wait until any wakeup is signalled (or timeout_ms elapses; -1 = no limit). Spurious returns are fine: the caller re-checks. */
void phe_wakeup_wait (int timeout_ms)
{
  pthread_mutex_lock (&wk_lock);
  if (timeout_ms < 0) pthread_cond_wait (&wk_cond, &wk_lock);
  else { struct timeval tv; struct timespec ts; gettimeofday (&tv, NULL);
    ts.tv_sec = tv.tv_sec + timeout_ms / 1000; ts.tv_nsec = (tv.tv_usec + (timeout_ms % 1000) * 1000) * 1000L;
    if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
    pthread_cond_timedwait (&wk_cond, &wk_lock, &ts); }
  pthread_mutex_unlock (&wk_lock);
}
