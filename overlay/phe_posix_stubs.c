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

/* fnmatch for GIO's xdgmime (Open C has none): * ? [set] [!set] and backslash quoting; FNM_CASEFOLD honoured, the other flags are not. */
#include <fnmatch.h>
#include <ctype.h>
static int phe_fn (const char *p, const char *s, int fl)
{
  for (; *p; p++, s++)
    {
      switch (*p)
        {
        case '*': { while (p[1] == '*') p++; if (!p[1]) return 0; for (;; s++) { if (!phe_fn (p + 1, s, fl)) return 0; if (!*s) return FNM_NOMATCH; } }
        case '?': if (!*s) return FNM_NOMATCH; break;
        case '[': { int neg = 0, ok = 0; const char *q = p + 1; int c = (unsigned char) *s; if (!c) return FNM_NOMATCH;
          if (fl & FNM_CASEFOLD) c = tolower (c);
          if (*q == '!' || *q == '^') { neg = 1; q++; }
          for (; *q && (*q != ']' || q == p + 1 + neg); q++)
            { int lo = (unsigned char) *q, hi = lo; if (q[1] == '-' && q[2] && q[2] != ']') { hi = (unsigned char) q[2]; q += 2; }
              if (fl & FNM_CASEFOLD) { lo = tolower (lo); hi = tolower (hi); } if (c >= lo && c <= hi) ok = 1; }
          if (!*q) return FNM_NOMATCH; if (ok == neg) return FNM_NOMATCH; p = q; break; }
        case 0x5c: if (!(fl & FNM_NOESCAPE) && p[1]) p++; /* fall through */
        default: { int a = (unsigned char) *p, b = (unsigned char) *s; if (fl & FNM_CASEFOLD) { a = tolower (a); b = tolower (b); } if (a != b) return FNM_NOMATCH; }
        }
    }
  return *s ? FNM_NOMATCH : 0;
}
int fnmatch (const char *pattern, const char *string, int flags) { return phe_fn (pattern, string, flags); }
