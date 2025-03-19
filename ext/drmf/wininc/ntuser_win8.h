/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was created to make information necessary for userspace
 ***   to call into the Windows kernel available to Dr. Memory.  It contains
 ***   only constants, structures, and macros, and thus, contains no
 ***   copyrightable information.
 ***
 ****************************************************************************
 ****************************************************************************/

#ifndef _NTUSER_WIN8_
#define _NTUSER_WIN8_ 1

/* Observed called from IMM32!IsLegacyIMEDisabled.  Both fields are written
 * with 0 in all observed instances.
 */
typedef struct _PROCESS_UI_CONTEXT {
    DWORD Unknown1;
    DWORD Unknown2;
} PROCESS_UI_CONTEXT, *PPROCESS_UI_CONTEXT;

BOOL NTAPI
NtUserGetProcessUIContextInformation(__in HANDLE ProcessHandle,
                                     __out PROCESS_UI_CONTEXT *ContextInformation);

/* This can be observed just starting up calc.exe on Windows 8.1, called from
 * C:\Program Files (x86)\Common Files\Microsoft Shared\Ink\tiptsf.dll
 */
BOOL NTAPI
NtUserGetWindowBand(__in HANDLE hwnd, __out DWORD *value);

#endif /* _NTUSER_WIN8_ */
