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
/* from phlib/include/ntpnpapi.h */

#ifndef __PHLIB_NTPNPAPI_H
#    define __PHLIB_NTPNPAPI_H

NTSTATUS
NTAPI
NtReplacePartitionUnit(__in PUNICODE_STRING TargetInstancePath,
                       __in PUNICODE_STRING SpareInstancePath, __in ULONG Flags);

NTSTATUS
NTAPI
NtSerializeBoot(VOID);

#endif /* __PHLIB_NTPNPAPI_H */

/* EOF */
