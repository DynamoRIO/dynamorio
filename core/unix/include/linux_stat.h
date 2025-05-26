/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was generated from kernel headers to make
 ***   information necessary for userspace to call into the Linux
 ***   kernel available to DynamoRIO.  It contains only constants,
 ***   structures, and macros generated from the original header, and
 ***   thus, contains no copyrightable information.
 ***
 ****************************************************************************
 ****************************************************************************/

#ifndef _CORE_UNIX_INCLUDE_STAT
#define _CORE_UNIX_INCLUDE_STAT

#include <linux/types.h>

struct statx_timestamp {
    __s64 tv_sec;
    __u32 tv_nsec;
    __s32 __reserved;
};

struct statx {
    /* 0x00 */
    __u32 stx_mask;       /* What results were written [uncond] */
    __u32 stx_blksize;    /* Preferred general I/O size [uncond] */
    __u64 stx_attributes; /* Flags conveying information about the file [uncond] */
    /* 0x10 */
    __u32 stx_nlink; /* Number of hard links */
    __u32 stx_uid;   /* User ID of owner */
    __u32 stx_gid;   /* Group ID of owner */
    __u16 stx_mode;  /* File mode */
    __u16 __spare0[1];
    /* 0x20 */
    __u64 stx_ino;             /* Inode number */
    __u64 stx_size;            /* File size */
    __u64 stx_blocks;          /* Number of 512-byte blocks allocated */
    __u64 stx_attributes_mask; /* Mask to show what's supported in stx_attributes */
    /* 0x40 */
    struct statx_timestamp stx_atime; /* Last access time */
    struct statx_timestamp stx_btime; /* File creation time */
    struct statx_timestamp stx_ctime; /* Last attribute change time */
    struct statx_timestamp stx_mtime; /* Last data modification time */
    /* 0x80 */
    __u32 stx_rdev_major; /* Device ID of special file [if bdev/cdev] */
    __u32 stx_rdev_minor;
    __u32 stx_dev_major; /* ID of device containing file [uncond] */
    __u32 stx_dev_minor;
    /* 0x90 */
    __u64 stx_mnt_id;
    __u64 __spare2;
    /* 0xa0 */
    __u64 __spare3[12]; /* Spare space for future expansion */
                        /* 0x100 */
};

#endif /* _CORE_UNIX_INCLUDE_STAT */
