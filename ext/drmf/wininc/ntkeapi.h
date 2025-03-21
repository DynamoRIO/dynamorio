/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was generated from a ProcessHacker header to make
 ***   information necessary for userspace to call into the Windows
 ***   kernel available to Dr. Memory.  It contains only constants,
 ***   structures, and macros generated from the original header, and
 ***   thus, contains no copyrightable information.
 ***
 ****************************************************************************
 ****************************************************************************/

/* from phlib/include/ntkeapi.h */

#ifndef __PHLIB_NTKEAPI_H
#define __PHLIB_NTKEAPI_H

/**************************************************
 * Syscalls added in Vista SP0
 */
VOID NTAPI
NtFlushProcessWriteBuffers(VOID);

#endif /* __PHLIB_NTKEAPI_H */

/* EOF */
