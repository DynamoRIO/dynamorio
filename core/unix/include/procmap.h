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

#ifndef _PROCMAP_H_
#define _PROCMAP_H_

#include <linux/fs.h>
#include <linux/ioctl.h>

#ifndef PROCMAP_QUERY
/* PROCMAP_QUERY was added in Linux 6.11. We want to be still be able to compile on
 * systems with older kernels so we define the interface here if it is not available.
 * This code is all derived from the Linux header include/uapi/linux/fs.h which is
 * licensed "GPL-2.0 WITH Linux-syscall-note".
 */
#    define PROCMAP_QUERY _IOWR('f', 17, struct procmap_query)

enum procmap_query_flags {
    PROCMAP_QUERY_VMA_READABLE = 0x01,
    PROCMAP_QUERY_VMA_WRITABLE = 0x02,
    PROCMAP_QUERY_VMA_EXECUTABLE = 0x04,
    PROCMAP_QUERY_VMA_SHARED = 0x08,
    PROCMAP_QUERY_COVERING_OR_NEXT_VMA = 0x10,
    PROCMAP_QUERY_FILE_BACKED_VMA = 0x20,
};

struct procmap_query {
    __u64 size;
    __u64 query_flags;
    __u64 query_addr;
    __u64 vma_start;
    __u64 vma_end;
    __u64 vma_flags;
    __u64 vma_page_size;
    __u64 vma_offset;
    __u64 inode;
    __u32 dev_major;
    __u32 dev_minor;
    __u32 vma_name_size;
    __u32 build_id_size;
    __u64 vma_name_addr;
    __u64 build_id_addr;
};
#endif /* !PROCMAP_QUERY */

#endif /* _PROCMAP_H_ */
