/* phedbustest_main.c -- libdbus over the real transport: a server on the loopback (tcp:host=127.0.0.1), a client connection to it (private, ANONYMOUS authentication, no peer credentials),
 * a method call with a reply, and a signal, through the wire protocol. The watches and timeouts libdbus asks for are served by a select loop in one thread (what a main loop does
 * for an application). BSD-2-Clause (Phorepoc). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <dbus/dbus.h>

extern void phe_log (const char *msg);
#define LOG(...) do { char m_[300]; snprintf (m_, sizeof m_, __VA_ARGS__); phe_log (m_); } while (0)

#define MAX 64
static DBusWatch *watches[MAX]; static int nwatches;
static DBusTimeout *timeouts[MAX]; static long timeout_due[MAX]; static int ntimeouts;
static DBusConnection *connections[MAX]; static int nconns;
static pthread_mutex_t lk = PTHREAD_MUTEX_INITIALIZER;
static volatile int stop_loop;
static int got_signal, server_calls;

static long now_ms (void) { struct timeval tv; gettimeofday (&tv, 0); return tv.tv_sec * 1000L + tv.tv_usec / 1000; }

static dbus_bool_t add_watch (DBusWatch *w, void *d) { (void) d; pthread_mutex_lock (&lk); if (nwatches < MAX) watches[nwatches++] = w; pthread_mutex_unlock (&lk); return TRUE; }
static void remove_watch (DBusWatch *w, void *d) { int i; (void) d; pthread_mutex_lock (&lk); for (i = 0; i < nwatches; i++) if (watches[i] == w) { watches[i] = watches[--nwatches]; break; } pthread_mutex_unlock (&lk); }
static void toggle_watch (DBusWatch *w, void *d) { (void) w; (void) d; }
static dbus_bool_t add_timeout (DBusTimeout *t, void *d) { (void) d; pthread_mutex_lock (&lk); if (ntimeouts < MAX) { timeouts[ntimeouts] = t; timeout_due[ntimeouts++] = now_ms () + dbus_timeout_get_interval (t); } pthread_mutex_unlock (&lk); return TRUE; }
static void remove_timeout (DBusTimeout *t, void *d) { int i; (void) d; pthread_mutex_lock (&lk); for (i = 0; i < ntimeouts; i++) if (timeouts[i] == t) { timeouts[i] = timeouts[--ntimeouts]; timeout_due[i] = timeout_due[ntimeouts]; break; } pthread_mutex_unlock (&lk); }
static void toggle_timeout (DBusTimeout *t, void *d) { int i; (void) d; pthread_mutex_lock (&lk); for (i = 0; i < ntimeouts; i++) if (timeouts[i] == t) timeout_due[i] = now_ms () + dbus_timeout_get_interval (t); pthread_mutex_unlock (&lk); }

static void *
loop (void *arg)
{
  (void) arg;
  while (!stop_loop) {
    fd_set rd, wr; int i, maxfd = -1; struct timeval tv; DBusWatch *ready[MAX]; unsigned flags[MAX]; int nready = 0;
    FD_ZERO (&rd); FD_ZERO (&wr);
    pthread_mutex_lock (&lk);
    for (i = 0; i < nwatches; i++) {
      DBusWatch *w = watches[i]; unsigned f; int fd;
      if (!dbus_watch_get_enabled (w)) continue;
      fd = dbus_watch_get_unix_fd (w); f = dbus_watch_get_flags (w);
      if (fd < 0) continue;
      if (f & DBUS_WATCH_READABLE) FD_SET (fd, &rd);
      if (f & DBUS_WATCH_WRITABLE) FD_SET (fd, &wr);
      if (fd > maxfd) maxfd = fd;
    }
    pthread_mutex_unlock (&lk);
    tv.tv_sec = 0; tv.tv_usec = 20000;
    if (select (maxfd + 1, &rd, &wr, 0, &tv) > 0) {
      pthread_mutex_lock (&lk);
      for (i = 0; i < nwatches; i++) {
        DBusWatch *w = watches[i]; int fd = dbus_watch_get_unix_fd (w); unsigned f = 0;
        if (!dbus_watch_get_enabled (w) || fd < 0) continue;
        if (FD_ISSET (fd, &rd)) f |= DBUS_WATCH_READABLE;
        if (FD_ISSET (fd, &wr)) f |= DBUS_WATCH_WRITABLE;
        if (f) { ready[nready] = w; flags[nready++] = f; }
      }
      pthread_mutex_unlock (&lk);
      for (i = 0; i < nready; i++) dbus_watch_handle (ready[i], flags[i]);
    }
    { long t = now_ms (); DBusTimeout *due[MAX]; int nd = 0;
      pthread_mutex_lock (&lk);
      for (i = 0; i < ntimeouts; i++) if (dbus_timeout_get_enabled (timeouts[i]) && timeout_due[i] <= t) { due[nd++] = timeouts[i]; timeout_due[i] = t + dbus_timeout_get_interval (timeouts[i]); }
      pthread_mutex_unlock (&lk);
      for (i = 0; i < nd; i++) dbus_timeout_handle (due[i]); }
    pthread_mutex_lock (&lk); for (i = 0; i < nconns; i++) dbus_connection_ref (connections[i]); pthread_mutex_unlock (&lk);
    for (i = 0; i < nconns; i++) { while (dbus_connection_get_dispatch_status (connections[i]) == DBUS_DISPATCH_DATA_REMAINS) dbus_connection_dispatch (connections[i]); dbus_connection_unref (connections[i]); }
  }
  return NULL;
}

/* the server's side of a connection: answers Hello (string) with a greeting and emits a signal */
static DBusHandlerResult
server_filter (DBusConnection *c, DBusMessage *m, void *d)
{
  (void) d;
  if (dbus_message_is_method_call (m, "org.phorepoc.Test", "Hello")) {
    const char *who = NULL; DBusMessage *reply, *sig; char text[120]; DBusMessageIter it;
    dbus_message_get_args (m, NULL, DBUS_TYPE_STRING, &who, DBUS_TYPE_INVALID);
    snprintf (text, sizeof text, "Hello, %s (answered over TCP by the server side)", who ? who : "?");
    reply = dbus_message_new_method_return (m); dbus_message_iter_init_append (reply, &it); { const char *t = text; dbus_message_iter_append_basic (&it, DBUS_TYPE_STRING, &t); }
    dbus_connection_send (c, reply, NULL); dbus_message_unref (reply);
    sig = dbus_message_new_signal ("/org/phorepoc/Test", "org.phorepoc.Test", "Greeted"); dbus_connection_send (c, sig, NULL); dbus_message_unref (sig);
    server_calls++;
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void
new_connection (DBusServer *s, DBusConnection *c, void *d)
{
  (void) s; (void) d;
  dbus_connection_ref (c);
  dbus_connection_set_allow_anonymous (c, TRUE);
  dbus_connection_set_watch_functions (c, add_watch, remove_watch, toggle_watch, NULL, NULL);
  dbus_connection_set_timeout_functions (c, add_timeout, remove_timeout, toggle_timeout, NULL, NULL);
  dbus_connection_add_filter (c, server_filter, NULL, NULL);
  pthread_mutex_lock (&lk); connections[nconns++] = c; pthread_mutex_unlock (&lk);
  LOG ("[phedbustest] server: new connection accepted");
}

static DBusHandlerResult
client_filter (DBusConnection *c, DBusMessage *m, void *d)
{
  (void) c; (void) d;
  if (dbus_message_is_signal (m, "org.phorepoc.Test", "Greeted")) { got_signal = 1; return DBUS_HANDLER_RESULT_HANDLED; }
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/* what the loopback offers: bind a stream socket to 127.0.0.1 / any address, on port 0 and on a fixed port, and report errno */
static void
probe_bind (const char *what, unsigned addr, int port)
{
  int fd = socket (AF_INET, SOCK_STREAM, 0); struct sockaddr_in sa; socklen_t len = sizeof sa;
  if (fd < 0) { LOG ("[phedbustest] probe %s: socket failed errno %d", what, errno); return; }
  memset (&sa, 0, sizeof sa); sa.sin_family = AF_INET; sa.sin_addr.s_addr = htonl (addr); sa.sin_port = htons (port);
  if (bind (fd, (struct sockaddr *) &sa, sizeof sa) != 0) LOG ("[phedbustest] probe %s: bind failed errno %d", what, errno);
  else if (listen (fd, 1) != 0) LOG ("[phedbustest] probe %s: bound, listen failed errno %d", what, errno);
  else { getsockname (fd, (struct sockaddr *) &sa, &len); LOG ("[phedbustest] probe %s: listening on port %d", what, ntohs (sa.sin_port)); }
  close (fd);
}

/* one complete exchange over one transport: listen, connect (private connection, ANONYMOUS), a method call with a reply, a signal; returns 0 on success, -1 when the transport cannot listen */
static int
exchange (const char *listen_address, const char *connect_address, const char *label)
{
  DBusError err; DBusServer *server; DBusConnection *client; pthread_t th; char *address; const char *mechs[] = { "ANONYMOUS", NULL };
  DBusMessage *call, *reply; const char *who = "Hildon"; int i, ok = 0;
  dbus_error_init (&err);
  got_signal = 0; server_calls = 0; stop_loop = 0; nwatches = ntimeouts = nconns = 0;
  server = dbus_server_listen (listen_address, &err);
  LOG ("[phedbustest] %s: listen %s: %s", label, listen_address, server ? "ok" : (err.message ? err.message : "?"));
  if (!server) return -1;
  dbus_server_set_auth_mechanisms (server, mechs);
  dbus_server_set_new_connection_function (server, new_connection, NULL, NULL);
  dbus_server_set_watch_functions (server, add_watch, remove_watch, toggle_watch, NULL, NULL);
  dbus_server_set_timeout_functions (server, add_timeout, remove_timeout, toggle_timeout, NULL, NULL);
  address = connect_address ? (char *) connect_address : dbus_server_get_address (server);
  LOG ("[phedbustest] %s: server address %s", label, address);
  pthread_create (&th, NULL, loop, NULL);
  client = dbus_connection_open_private (address, &err);
  if (!client) { LOG ("[phedbustest] %s: connect failed: %s", label, err.message ? err.message : "?"); stop_loop = 1; pthread_join (th, NULL); return 2; }
  dbus_connection_set_exit_on_disconnect (client, FALSE);
  dbus_connection_set_watch_functions (client, add_watch, remove_watch, toggle_watch, NULL, NULL);
  dbus_connection_set_timeout_functions (client, add_timeout, remove_timeout, toggle_timeout, NULL, NULL);
  dbus_connection_add_filter (client, client_filter, NULL, NULL);
  pthread_mutex_lock (&lk); connections[nconns++] = client; pthread_mutex_unlock (&lk);
  call = dbus_message_new_method_call (NULL, "/org/phorepoc/Test", "org.phorepoc.Test", "Hello");
  dbus_message_append_args (call, DBUS_TYPE_STRING, &who, DBUS_TYPE_INVALID);
  reply = dbus_connection_send_with_reply_and_block (client, call, 5000, &err);
  if (reply) {
    const char *text = NULL;
    dbus_message_get_args (reply, NULL, DBUS_TYPE_STRING, &text, DBUS_TYPE_INVALID);
    LOG ("[phedbustest] %s: reply: %s", label, text ? text : "?");
    ok = text && strstr (text, "Hello, Hildon") != NULL; dbus_message_unref (reply);
  } else LOG ("[phedbustest] %s: call failed: %s", label, err.message ? err.message : "?");
  dbus_message_unref (call);
  for (i = 0; i < 100 && !got_signal; i++) usleep (20000);
  LOG ("[phedbustest] %s: signal received: %d, calls served: %d", label, got_signal, server_calls);
  LOG ("[phedbustest] %s: %s", label, ok && got_signal ? "PASS" : "FAIL");
  stop_loop = 1; pthread_join (th, NULL);
  dbus_connection_close (client);
  return ok && got_signal ? 0 : 3;
}

int
phe_dbus_test_main (void)
{
  int r_pipe, r_tcp = -1, t;
  const char *tries[] = { "tcp:host=127.0.0.1", "tcp:host=127.0.0.1,port=5371", NULL };
  dbus_threads_init_default ();
  LOG ("[phedbustest] libdbus %s", "1.16.2");
  /* the protocol, authentication, message and signal machinery without the network: libdbus's own in-process transport */
  r_pipe = exchange ("debug-pipe:name=phe", NULL, "debug-pipe");
  /* the same over the real transport: TCP on the loopback. Open C has no default connection here: name the access point (what the ROM's CommsDat calls the DHCP Ethernet IAP) */
  { struct ifreq ifr; memset (&ifr, 0, sizeof ifr); strcpy (ifr.ifr_name, "Ethernet - DHCP"); LOG ("[phedbustest] setdefaultif(%s) -> %d", ifr.ifr_name, setdefaultif (&ifr)); }
  probe_bind ("127.0.0.1 port 0", INADDR_LOOPBACK, 0); probe_bind ("any port 0", INADDR_ANY, 0);
  for (t = 0; tries[t] && r_tcp == -1; t++) r_tcp = exchange (tries[t], NULL, "tcp");
  LOG ("[phedbustest] done: debug-pipe %s, tcp %s", r_pipe == 0 ? "PASS" : "FAIL", r_tcp == 0 ? "PASS" : (r_tcp == -1 ? "no socket" : "FAIL"));
  return r_pipe == 0 && r_tcp == 0 ? 0 : 1;
}
