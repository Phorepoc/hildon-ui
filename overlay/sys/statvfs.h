/* sys/statvfs.h -- stand-in: Open C has none; GIO's glocalfile.c uses statvfs() for filesystem info. The functions fail with ENOSYS (phe_posix_stubs.c). */
#ifndef PHE_SYS_STATVFS_H
#define PHE_SYS_STATVFS_H
struct statvfs { unsigned long f_bsize, f_frsize; unsigned long long f_blocks, f_bfree, f_bavail, f_files, f_ffree, f_favail; unsigned long f_fsid, f_flag, f_namemax; };
#define ST_RDONLY 1
#define ST_NOSUID 2
int statvfs (const char *path, struct statvfs *buf);
int fstatvfs (int fd, struct statvfs *buf);
#endif
