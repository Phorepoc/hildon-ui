/* phedbus_posix.c -- the few POSIX calls libdbus makes that Open C does not provide. BSD-2-Clause (Phorepoc).
 *  socketpair   a connected pair of stream sockets, made over the loopback (a listener on an ephemeral port, a connect, an accept): what D-Bus does itself on Windows.
 *  pwrite       write at an offset, through lseek / write / lseek back.
 *  environ      Open C keeps its environment private; the variable exists (empty) for the one use libdbus makes of it, clearing the environment.
 *  fork, execvp a process cannot be forked here: they fail with ENOSYS (the same answer the GLib build gives); libdbus uses them only to start services and to daemonize. */
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

static char *empty_environment[] = { 0 };
#undef environ
char **environ = empty_environment;

int
socketpair (int domain, int type, int protocol, int sv[2])
{
  int listener, a, b, one = 1; struct sockaddr_in addr; socklen_t len = sizeof addr;
  if (domain != AF_INET && domain != AF_UNIX) { errno = EAFNOSUPPORT; return -1; }
  if (type != SOCK_STREAM) { errno = EPROTOTYPE; return -1; }
  listener = socket (AF_INET, SOCK_STREAM, 0);
  if (listener < 0) return -1;
  memset (&addr, 0, sizeof addr); addr.sin_family = AF_INET; addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK); addr.sin_port = 0;
  if (bind (listener, (struct sockaddr *) &addr, sizeof addr) != 0 || listen (listener, 1) != 0 || getsockname (listener, (struct sockaddr *) &addr, &len) != 0) { close (listener); return -1; }
  a = socket (AF_INET, SOCK_STREAM, 0);
  if (a < 0) { close (listener); return -1; }
  if (connect (a, (struct sockaddr *) &addr, sizeof addr) != 0) { close (a); close (listener); return -1; }
  b = accept (listener, 0, 0); close (listener);
  if (b < 0) { close (a); return -1; }
  setsockopt (a, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one); setsockopt (b, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
  (void) protocol;
  sv[0] = a; sv[1] = b;
  return 0;
}

ssize_t
pwrite (int fd, const void *buf, size_t count, off_t offset)
{
  off_t saved = lseek (fd, 0, SEEK_CUR); ssize_t n;
  if (saved < 0 || lseek (fd, offset, SEEK_SET) < 0) return -1;
  n = write (fd, buf, count);
  lseek (fd, saved, SEEK_SET);
  return n;
}

int fork (void) { errno = ENOSYS; return -1; }
int execvp (const char *file, char *const argv[]) { (void) file; (void) argv; errno = ENOSYS; return -1; }
