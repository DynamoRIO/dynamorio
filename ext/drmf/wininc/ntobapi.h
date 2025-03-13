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

/* from phlib/include/ntobapi.h */

#ifndef __PHLIB_NTOBAPI_H
#define __PHLIB_NTOBAPI_H

NTSTATUS
NTAPI
NtOpenPrivateNamespace(
    __out PHANDLE NamespaceHandle,
    __in ACCESS_MASK DesiredAccess,
    __in_opt POBJECT_ATTRIBUTES ObjectAttributes,
    __in PVOID BoundaryDescriptor
    );

NTSTATUS
NTAPI
NtCreatePrivateNamespace(
    __out PHANDLE NamespaceHandle,
    __in ACCESS_MASK DesiredAccess,
    __in_opt POBJECT_ATTRIBUTES ObjectAttributes,
    __in PVOID BoundaryDescriptor
    );

NTSTATUS
NTAPI
NtDeletePrivateNamespace(
    __in HANDLE NamespaceHandle
    );

#endif /* __PHLIB_NTOBAPI_H */

/* EOF */
