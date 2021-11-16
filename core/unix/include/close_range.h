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

#ifndef _CLOSE_RANGE_H
#define _CLOSE_RANGE_H

/* Derived from /include/uapi/linux/close_range.h */

/* Unshare the file descriptor table before closing file descriptors. */
#define CLOSE_RANGE_UNSHARE (1U << 1)

/* Set the FD_CLOEXEC bit instead of closing the file descriptor. */
#define CLOSE_RANGE_CLOEXEC (1U << 2)

#endif /* _CLOSE_RANGE_H */
