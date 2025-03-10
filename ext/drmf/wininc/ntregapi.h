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

 /* from phlib/include/ntregapi.h */

#ifndef __PHLIB_NTREGAPI_H
#define __PHLIB_NTREGAPI_H

NTSTATUS NTAPI
NtOpenKeyTransacted(
    __out PHANDLE KeyHandle,
    __in ACCESS_MASK DesiredAccess,
    __in POBJECT_ATTRIBUTES ObjectAttributes,
    __in HANDLE TransactionHandle
    );

/* added in Windows 7 */
NTSTATUS NTAPI
NtOpenKeyTransactedEx(
    __out PHANDLE KeyHandle,
    __in ACCESS_MASK DesiredAccess,
    __in POBJECT_ATTRIBUTES ObjectAttributes,
    __in ULONG OpenOptions,
    __in HANDLE TransactionHandle
    );

NTSTATUS
NTAPI
NtFreezeRegistry(
    __in ULONG TimeOutInSeconds
    );

NTSTATUS
NTAPI
NtCreateKeyTransacted(
    __out PHANDLE KeyHandle,
    __in ACCESS_MASK DesiredAccess,
    __in POBJECT_ATTRIBUTES ObjectAttributes,
    __reserved ULONG TitleIndex,
    __in_opt PUNICODE_STRING Class,
    __in ULONG CreateOptions,
    __in HANDLE TransactionHandle,
    __out_opt PULONG Disposition
    );

#endif /* __PHLIB_NTPSAPI_H */

/* EOF */
