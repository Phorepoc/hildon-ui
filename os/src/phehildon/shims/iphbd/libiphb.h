/* shim: libiphb, the client side of the Nokia IP heartbeat daemon, implemented in phehildon_iphb.c with an in-process heartbeat: a client asks to be woken between mintime and maxtime
 * seconds from now; the wake-ups of all clients are batched (every waiter whose minimum has passed is woken when the first one reaches its maximum) so that periodic work of several
 * plug-ins happens together. The handle's file descriptor becomes readable at the wake-up. BSD-2-Clause (Phorepoc). */
#ifndef PHE_LIBIPHB_H
#define PHE_LIBIPHB_H
#include <time.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct _iphb *iphb_t;
iphb_t iphb_open (int *dummy);                                   /* the heartbeat interval, in seconds, is not reported (the heartbeat is continuous) */
iphb_t iphb_close (iphb_t iphbh);                                /* returns NULL */
int    iphb_get_fd (iphb_t iphbh);
time_t iphb_wait (iphb_t iphbh, unsigned short mintime, unsigned short maxtime, int must_wait);
int    iphb_discard_wakeups (iphb_t iphbh);
#ifdef __cplusplus
}
#endif
#endif
