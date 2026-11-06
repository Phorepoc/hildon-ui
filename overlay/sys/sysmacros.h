/* sys/sysmacros.h -- stand-in (Open C has none): GIO only needs major()/minor() for device numbers. */
#ifndef PHE_SYSMACROS_H
#define PHE_SYSMACROS_H
#define major(d) (((d) >> 8) & 0xff)
#define minor(d) ((d) & 0xff)
#endif
