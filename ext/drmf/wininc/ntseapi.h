/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was created from a ProcessHacker header to make
 ***   information necessary for userspace to call into the Windows
 ***   kernel available to Dr. Memory.  It contains only constants,
 ***   structures, and macros generated from the original header, and
 ***   thus, contains no copyrightable information.
 ***
 ****************************************************************************
 ****************************************************************************/

 /* from phlib/include/ntseapi.h */

#ifndef __PHLIB_NTSEAPI_H
#define __PHLIB_NTSEAPI_H

NTSTATUS
NTAPI
NtQuerySecurityAttributesToken(
    __in HANDLE TokenHandle,
    __in_ecount_opt(NumberOfAttributes) PUNICODE_STRING Attributes,
    __in ULONG NumberOfAttributes,
    __out_bcount(Length) PVOID Buffer
    __in ULONG Length,
    __out PULONG ReturnLength
    );

#endif /* __PHLIB_NTSEAPI_H */
/* EOF */
