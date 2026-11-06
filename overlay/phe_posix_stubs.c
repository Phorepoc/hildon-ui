/* phe_posix_stubs.c -- POSIX calls Open C does not have. They fail cleanly with ENOSYS (GLib's gspawn.c / gtestutils.c then report
 * "Function not implemented" instead of the link stubbing them into a crash): fork/exec (a Symbian process cannot fork) and setrlimit. */
#include <errno.h>
#include <sys/types.h>
struct rlimit;
int fork (void) { errno = ENOSYS; return -1; }
int execv (const char *p, char *const a[]) { (void) p; (void) a; errno = ENOSYS; return -1; }
int execve (const char *p, char *const a[], char *const e[]) { (void) p; (void) a; (void) e; errno = ENOSYS; return -1; }
int setrlimit (int r, const struct rlimit *l) { (void) r; (void) l; errno = ENOSYS; return -1; }
int execvp (const char *f, char *const a[]) { (void) f; (void) a; errno = ENOSYS; return -1; }
/* Open C does not export `environ`; GLib's g_get_environ() reads it. NULL = an empty environment (getenv() still works via Open C). */
char **environ = 0;

/* GIO (glocalfile.c): filesystem info is not available on Open C */
#include <sys/statvfs.h>
int statvfs (const char *p, struct statvfs *b) { (void) p; (void) b; errno = ENOSYS; return -1; }
int fstatvfs (int fd, struct statvfs *b) { (void) fd; (void) b; errno = ENOSYS; return -1; }
/* GIO (ginetaddress.c): the IPv6 wildcard/loopback constants Open C declares nowhere */
#include <netinet/in.h>
const struct in6_addr in6addr_any = { { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } } };
const struct in6_addr in6addr_loopback = { { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 } } };
