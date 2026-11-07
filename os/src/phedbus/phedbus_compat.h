/* phedbus_compat.h -- what libdbus expects of the C library and Open C lacks. Included with -include. BSD-2-Clause (Phorepoc). */
#ifndef PHEDBUS_COMPAT_H
#define PHEDBUS_COMPAT_H
#include <stddef.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
/* the type with the strictest fundamental alignment (C11); Open C predates it */
typedef union { long long phe_ll; long double phe_ld; void *phe_p; } max_align_t;
#endif
