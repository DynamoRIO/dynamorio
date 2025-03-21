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

NTSTATUS NTAPI
NtWow64CallFunction64(
    __in ULONG FunctionIndex,
    __in ULONG Flags,
    __in ULONG InputLength,
    __in_bcount(InputLength) PVOID InputBuffer,
    __in ULONG OutputLength,
    __out_bcount(OutputLength) PVOID OutputBuffer,
    __out_opt PULONG ReturnStatus
    );

PVOID NTAPI
NtWow64CsrAllocateCaptureBuffer(
    __in ULONG Count,
    __in ULONG Size
    );

ULONG NTAPI
NtWow64CsrAllocateMessagePointer(
    __inout PVOID CaptureBuffer,
    __in ULONG Length,
    __out PVOID *CapturedBuffer
    );

VOID NTAPI
NtWow64CsrCaptureMessageBuffer(
    __inout PVOID CaptureBuffer,
    __in_opt PVOID Buffer,
    __in ULONG Length,
    __out PVOID *CapturedBuffer
    );

VOID NTAPI
NtWow64CsrCaptureMessageString(
    __inout PVOID CaptureBuffer,
    __in_opt PCSTR String,
    __in ULONG Length,
    __in ULONG MaximumLength,
    __out PSTRING CapturedString
    );

NTSTATUS NTAPI
NtWow64CsrClientCallServer(
    __inout PVOID ApiMessage,
    __inout_opt PVOID CaptureBuffer,
    __in ULONG ApiNumber,
    __in ULONG ArgLength
    );

NTSTATUS NTAPI
NtWow64CsrClientConnectToServer(
    __in PWSTR ObjectDirectory,
    __in ULONG ServerDllIndex,
    __in_opt PCSR_CALLBACK_INFO CallbackInformation,
    __in PVOID ConnectionInformation,
    __inout_opt PULONG ConnectionInformationLength,
    __out_opt PBOOLEAN CalledFromServer
    );

NTSTATUS NTAPI
NtWow64CsrFreeCaptureBuffer(
    __in PVOID CaptureBuffer
    );

DWORD NTAPI
NtWow64CsrGetProcessId(
    VOID
    );

NTSTATUS NTAPI
NtWow64CsrIdentifyAlertableThread(
    VOID
    );

NTSTATUS NTAPI
NtWow64CsrNewThread(
    VOID
    );

NTSTATUS NTAPI
NtWow64CsrSetPriorityClass(
    __in HANDLE ProcessHandle,
    __inout PULONG PriorityClass
    );

NTSTATUS NTAPI
NtWow64CsrVerifyRegion(
    __in PVOID Buffer,
    __in ULONG Length
    );

NTSTATUS NTAPI
NtWow64DebuggerCall(
    __in ULONG ServiceClass,
    __in ULONG Arg1,
    __in ULONG Arg2,
    __in ULONG Arg3,
    __in ULONG Arg4
    );

NTSTATUS NTAPI
NtWow64GetCurrentProcessorNumberEx(
    __out PPROCESSOR_NUMBER ProcNumber
    );

NTSTATUS NTAPI
NtWow64GetNativeSystemInformation(
    __in SYSTEM_INFORMATION_CLASS SystemInformationClass,
    __out PVOID SystemInformation,
    __in ULONG SystemInformationLength,
    __out_opt PULONG ReturnLength
    );

NTSTATUS NTAPI
NtWow64InterlockedPopEntrySList(
    __inout PSLIST_HEADER ListHead
    );

NTSTATUS NTAPI
NtWow64QueryInformationProcess64(
    __in HANDLE ProcessHandle,
    __in PROCESSINFOCLASS ProcessInformationClass,
    __out_bcount(ProcessInformationLength) PVOID ProcessInformation64,
    __in ULONG ProcessInformationLength,
    __out_opt PULONG ReturnLength
    );

/* PVOID64 and ULONGLONG take 2 params each */
NTSTATUS NTAPI
NtWow64ReadVirtualMemory64(
    __in HANDLE ProcessHandle,
    __in_opt PVOID64 BaseAddress,
    __out_bcount(BufferSize) PVOID Buffer,
    __in ULONGULONG BufferSize,
    __out_opt PULONGLONG NumberOfBytesRead
    );

NTSTATUS NTAPI
NtWow64WriteVirtualMemory64(
    __in HANDLE ProcessHandle,
    __in_opt PVOID64 BaseAddress,
    __in_bcount(BufferSize) PVOID Buffer,
    __in ULONGLONG BufferSize,
    __out_opt PULONGLONG NumberOfBytesWritten
    );
