/* Copyright The Phorepoc Authors */
/* SPDX-License-Identifier: BSD-2-Clause */
/* phehildon_iphb.c -- the in-process heartbeat behind shims/iphbd/libiphb.h. One thread keeps the time for every handle; a handle is a pipe: the wake-up is a byte written to it, the client
 * polls the read end (iphb_get_fd) and reads the byte. A waiter has a window [start + mintime, start + maxtime]; the first waiter to reach the end of its window wakes every waiter
 * whose window has opened. BSD-2-Clause (Phorepoc). */
#include <glib.h>
#include <unistd.h>
#include <fcntl.h>
#include "iphbd/libiphb.h"

struct _iphb { int rfd, wfd; gboolean waiting; time_t min_at, max_at; };

static GMutex lock; static GSList *handles = NULL; static gboolean thread_started = FALSE;

static gpointer
heartbeat_thread (gpointer d)
{
  (void) d;
  for (;;) {
    GSList *l; time_t now; gboolean due = FALSE;
    g_usleep (1000 * 1000);
    g_mutex_lock (&lock);
    now = time (NULL);
    for (l = handles; l; l = l->next) { struct _iphb *h = l->data; if (h->waiting && now >= h->max_at) due = TRUE; }
    if (due)
      for (l = handles; l; l = l->next) {
        struct _iphb *h = l->data;
        if (h->waiting && now >= h->min_at) { char b = 'w'; h->waiting = FALSE; if (write (h->wfd, &b, 1) < 0) { } }
      }
    g_mutex_unlock (&lock);
  }
  return NULL;
}

iphb_t
iphb_open (int *dummy)
{
  struct _iphb *h; int fds[2];
  if (dummy) *dummy = 1;
  if (pipe (fds) != 0) return NULL;
  h = g_new0 (struct _iphb, 1); h->rfd = fds[0]; h->wfd = fds[1];
  fcntl (h->rfd, F_SETFL, fcntl (h->rfd, F_GETFL) | O_NONBLOCK);
  g_mutex_lock (&lock);
  handles = g_slist_prepend (handles, h);
  if (!thread_started) { thread_started = TRUE; g_thread_new ("iphb", heartbeat_thread, NULL); }
  g_mutex_unlock (&lock);
  return h;
}

iphb_t
iphb_close (iphb_t h)
{
  if (!h) return NULL;
  g_mutex_lock (&lock); handles = g_slist_remove (handles, h); g_mutex_unlock (&lock);
  close (h->rfd); close (h->wfd); g_free (h);
  return NULL;
}

int iphb_get_fd (iphb_t h) { return h ? h->rfd : -1; }

time_t
iphb_wait (iphb_t h, unsigned short mintime, unsigned short maxtime, int must_wait)
{
  time_t now = time (NULL);
  (void) must_wait;
  if (!h) return (time_t) -1;
  g_mutex_lock (&lock);
  h->min_at = now + mintime; h->max_at = now + (maxtime > mintime ? maxtime : mintime); h->waiting = TRUE;
  g_mutex_unlock (&lock);
  return now;
}

int
iphb_discard_wakeups (iphb_t h)
{
  char b[16];
  if (!h) return -1;
  while (read (h->rfd, b, sizeof b) > 0) { }
  return 0;
}
