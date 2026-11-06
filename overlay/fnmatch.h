/* fnmatch.h -- stand-in: Open C has no fnmatch(); phe_posix_stubs.c has a small one (* ? [set] and backslash; FNM_CASEFOLD). GIO's xdgmime globbing uses it. */
#ifndef PHE_FNMATCH_H
#define PHE_FNMATCH_H
#define FNM_NOMATCH 1
#define FNM_NOESCAPE 2
#define FNM_PATHNAME 4
#define FNM_PERIOD 8
#define FNM_CASEFOLD 16
int fnmatch (const char *pattern, const char *string, int flags);
#endif
