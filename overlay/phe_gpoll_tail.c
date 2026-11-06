
/* ---- Phorepoc: g_poll over Open C. Wakeups (fake ids >= PHE_WAKE_BASE) are handled in-process, real descriptors by select(). ---- */
#define PHE_WAKE_BASE 0x40000000
extern int phe_wakeup_pending (int id);
extern void phe_wakeup_wait (int timeout_ms);
gint
g_poll (GPollFD *fds, guint nfds, gint timeout)
{
  guint i; gint real = 0, nready; gint64 deadline = 0;
  GPollFD *rf = __builtin_alloca (sizeof (GPollFD) * (nfds + 1)); guint *ri = __builtin_alloca (sizeof (guint) * (nfds + 1));
  for (i = 0; i < nfds; i++) { fds[i].revents = 0; if (fds[i].fd < PHE_WAKE_BASE) { rf[real] = fds[i]; ri[real++] = i; } }
  if (timeout > 0) deadline = g_get_monotonic_time () + (gint64) timeout * 1000;
  for (;;)
    {
      gint slice;
      nready = 0;
      for (i = 0; i < nfds; i++)
        if (fds[i].fd >= PHE_WAKE_BASE && (fds[i].events & G_IO_IN) && phe_wakeup_pending (fds[i].fd)) { fds[i].revents = G_IO_IN; nready++; }
      if (real)
        {
          gint r = phe_g_poll_select (rf, real, (nready || timeout == 0) ? 0 : 10);
          if (r > 0) { guint k; for (k = 0; k < (guint) real; k++) fds[ri[k]].revents = rf[k].revents; nready += r; }
          else if (r < 0 && nready == 0) return r;
        }
      if (nready || timeout == 0) return nready;
      if (timeout > 0) { gint64 left = (deadline - g_get_monotonic_time ()) / 1000; if (left <= 0) return 0; slice = left > 10 ? 10 : (gint) left; }
      else slice = 10;
      if (!real) phe_wakeup_wait (timeout > 0 ? (gint) ((deadline - g_get_monotonic_time ()) / 1000) : -1);
      /* with real descriptors the select() above already slept one slice */
      (void) slice;
    }
}
