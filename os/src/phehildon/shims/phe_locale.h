/* Copyright The Phorepoc Authors */
/* SPDX-License-Identifier: BSD-2-Clause */
/* phe_locale.h -- the glibc locale API libhildon uses for calendar / date formats (newlocale + nl_langinfo_l, and the two glibc time items), for a C library that has the process locale only:
 * a "locale_t" names that locale, nl_langinfo_l () is nl_langinfo (), and the two glibc items answer as the C/POSIX locale does (first weekday Sunday, weeks counted from 1997-11-30). Included with -include. */
#ifndef PHE_LOCALE_H
#define PHE_LOCALE_H
#include <locale.h>
#include <langinfo.h>
typedef struct _phe_locale *locale_t;
#define LC_TIME_MASK (1 << 5)
#define _NL_TIME_FIRST_WEEKDAY 0x7f000001
#define _NL_TIME_WEEK_1STDAY   0x7f000002
static inline locale_t newlocale (int mask, const char *name, locale_t base) { (void) mask; (void) name; (void) base; return (locale_t) 1; }
static inline void freelocale (locale_t l) { (void) l; }
static inline char *phe_nl_langinfo (nl_item item)
{
  static char first_weekday[2] = { 1, 0 };
  if (item == _NL_TIME_FIRST_WEEKDAY) return first_weekday;                  /* 1 = Sunday */
  if (item == _NL_TIME_WEEK_1STDAY) return (char *) (unsigned long) 19971130;   /* a date, returned in the pointer as glibc does */
  return nl_langinfo (item);
}
#define nl_langinfo(item) phe_nl_langinfo (item)
#define nl_langinfo_l(item, l) phe_nl_langinfo (item)
#endif
