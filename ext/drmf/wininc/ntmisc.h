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

 /* from phlib/include/ntmisc.h */

#ifndef __PHLIB_NTMISC_H
#define __PHLIB_NTMISC_H

NTSTATUS NTAPI
NtDrawText(
    __in PUNICODE_STRING Text
    );

NTSTATUS NTAPI
NtTraceControl(
    __in ULONG FunctionCode,
    __in_bcount_opt(InBufferLen) PVOID InBuffer,
    __in ULONG InBufferLen,
    __out_bcount_opt(OutBufferLen) PVOID OutBuffer,
    __in ULONG OutBufferLen,
    __out PULONG ReturnLength
    );

NTSTATUS NTAPI
NtSetWnfProcessNotificationEvent(
    __in HANDLE Unknown1
    );

#endif /* __PHLIB_NTMISC_H */

/* EOF */
