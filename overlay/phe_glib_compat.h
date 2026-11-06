/* phe_glib_compat.h -- force-included when compiling GLib 2.56 for Open C (P.I.P.S.): what that libc lacks. */
#ifndef PHE_GLIB_COMPAT_H
#define PHE_GLIB_COMPAT_H
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <string.h>
/* Open C strerror() can return NULL for an errno it does not know; GLib passes the result to g_locale_to_utf8 */
static inline char *phe_strerror (int e) { char *s = (strerror) (e); return s ? s : (char *) "Unknown error"; }
#define strerror(e) phe_strerror (e)
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC CLOCK_REALTIME   /* Open C has one clock; GCond timeouts and g_get_monotonic_time agree on it */
#endif
#include <sys/time.h>
/* Open C: clock_gettime() is there but fails at run time (seen 2026-11-06); gettimeofday() works */
static inline int phe_clock_gettime (int c, struct timespec *ts) { struct timeval tv; (void) c; if (gettimeofday (&tv, 0)) return -1;
  ts->tv_sec = tv.tv_sec; ts->tv_nsec = tv.tv_usec * 1000; return 0; }
#define clock_gettime(c, ts) phe_clock_gettime ((c), (ts))
static inline int pthread_condattr_setclock (pthread_condattr_t *a, int c) { (void) a; (void) c; return 0; }
#ifndef HAVE_PTHREAD_CONDATTR_SETCLOCK
#define HAVE_PTHREAD_CONDATTR_SETCLOCK 1
#endif
/* sig_atomic_t */
typedef int sig_atomic_t;
/* pthread_rwlock_t: Open C has none. A writer-excluding implementation on mutex + condvar (GRWLock is lazily initialised) */
typedef struct { pthread_mutex_t m; pthread_cond_t c; int readers; int writer; } pthread_rwlock_t;
static inline int pthread_rwlock_init (pthread_rwlock_t *l, const void *a) { (void) a; l->readers = 0; l->writer = 0;
  pthread_mutex_init (&l->m, 0); return pthread_cond_init (&l->c, 0); }
static inline int pthread_rwlock_destroy (pthread_rwlock_t *l) { pthread_cond_destroy (&l->c); return pthread_mutex_destroy (&l->m); }
static inline int pthread_rwlock_rdlock (pthread_rwlock_t *l) { pthread_mutex_lock (&l->m);
  while (l->writer) pthread_cond_wait (&l->c, &l->m); l->readers++; return pthread_mutex_unlock (&l->m); }
static inline int pthread_rwlock_tryrdlock (pthread_rwlock_t *l) { int r = 0; pthread_mutex_lock (&l->m);
  if (l->writer) r = EBUSY; else l->readers++; pthread_mutex_unlock (&l->m); return r; }
static inline int pthread_rwlock_wrlock (pthread_rwlock_t *l) { pthread_mutex_lock (&l->m);
  while (l->writer || l->readers) pthread_cond_wait (&l->c, &l->m); l->writer = 1; return pthread_mutex_unlock (&l->m); }
static inline int pthread_rwlock_trywrlock (pthread_rwlock_t *l) { int r = 0; pthread_mutex_lock (&l->m);
  if (l->writer || l->readers) r = EBUSY; else l->writer = 1; pthread_mutex_unlock (&l->m); return r; }
static inline int pthread_rwlock_unlock (pthread_rwlock_t *l) { pthread_mutex_lock (&l->m);
  if (l->writer) l->writer = 0; else if (l->readers) l->readers--; pthread_cond_broadcast (&l->c); return pthread_mutex_unlock (&l->m); }
struct in6_addr;
extern const struct in6_addr in6addr_any, in6addr_loopback;
#endif
