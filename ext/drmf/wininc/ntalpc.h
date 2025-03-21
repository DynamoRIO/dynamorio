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
NtAlpcAcceptConnectPort(
    __out PHANDLE                        PortHandle,
    __in HANDLE                          ConnectionPortHandle,
    __in ULONG                           Flags,
    __in POBJECT_ATTRIBUTES              ObjectAttributes,
    __in PALPC_PORT_ATTRIBUTES           PortAttributes,
    __in_opt PVOID                       PortContext, // opaque value
    __in PPORT_MESSAGE                   ConnectionRequest,
    __inout_opt PALPC_MESSAGE_ATTRIBUTES MessageAttributes,
    __in BOOLEAN                         AcceptConnection
    );

NTSTATUS NTAPI
NtAlpcCancelMessage(
    __in HANDLE                  PortHandle,
    __in ULONG                   Flags,
    __in ALPC_CONTEXT_ATTRIBUTES MessageContext
    );

NTSTATUS NTAPI
NtAlpcConnectPort(
    __out PHANDLE                           PortHandle,
    __in PUNICODE_STRING                    PortName,
    __in POBJECT_ATTRIBUTES                 ObjectAttributes,
    __in_opt PALPC_PORT_ATTRIBUTES          PortAttributes,
    __in ULONG                              Flags,
    __in_opt PSID                           Sid,
    __inout PPORT_MESSAGE                   ConnectionMessage,
    __inout_opt PULONG                      BufferLength,
    __inout_opt PALPC_MESSAGE_ATTRIBUTES    OutMessageAttributes,
    __inout_opt PALPC_MESSAGE_ATTRIBUTES    InMessageAttributes,
    __in_opt PLARGE_INTEGER                 Timeout
    );

NTSTATUS NTAPI
NtAlpcCreatePort(
    __out PHANDLE                   PortHandle,
    __in POBJECT_ATTRIBUTES         ObjectAttributes,
    __in_opt PALPC_PORT_ATTRIBUTES  PortAttributes
    );

NTSTATUS NTAPI
NtAlpcCreatePortSection(
    __in HANDLE     PortHandle,
    __in ULONG      Flags,
    __in_opt HANDLE SectionHandle,
    __in ULONG      SectionSize,
    __out PHANDLE   AlpcSectionHandle,
    __out PULONG    ActualSectionSize
    );

NTSTATUS NTAPI
NtAlpcCreateResourceReserve(
    __in HANDLE         PortHandle,
    __reserved ULONG    Flags,
    __in SIZE_T         MessageSize,
    __out PHANDLE       ResourceID
    );

NTSTATUS NTAPI
NtAlpcCreateSectionView(
    __in HANDLE             PortHandle,
    __reserved ULONG        Flags,
    __inout PALPC_DATA_VIEW ViewAttrbutes
    );

NTSTATUS NTAPI
NtAlpcCreateSecurityContext(
    __in HANDLE                       PortHandle,
    __reserved ULONG                  Flags,
    __inout PALPC_SECURITY_ATTRIBUTES SecurityAttribute
    );

NTSTATUS NTAPI
NtAlpcDeletePortSection(
    __in HANDLE      PortHandle,
    __reserved ULONG Flags,
    __in HANDLE      SectionHandle
    );

NTSTATUS NTAPI
NtAlpcDeleteResourceReserve(
    __in HANDLE      PortHandle,
    __reserved ULONG Flags,
    __in HANDLE      ResourceID
    );

NTSTATUS NTAPI
NtAlpcDeleteSectionView(
    __in HANDLE      PortHandle,
    __reserved ULONG Flags,
    __in PVOID       ViewBase
    );

NTSTATUS NTAPI
NtAlpcDeleteSecurityContext(
    __in HANDLE      PortHandle,
    __reserved ULONG Flags,
    __in HANDLE      ContextHandle
    );

NTSTATUS NTAPI
NtAlpcDisconnectPort(
    __in HANDLE PortHandle,
    __in ULONG  Flags
    );

NTSTATUS NTAPI
NtAlpcImpersonateClientOfPort(
    __in HANDLE         PortHandle,
    __in PPORT_MESSAGE  PortMessage,
    __reserved PVOID    Reserved
    );

NTSTATUS NTAPI
NtAlpcOpenSenderProcess(
    __out HANDLE            ProcessHandle,
    __in HANDLE             PortHandle,
    __in PPORT_MESSAGE      PortMessage,
    __reserved ULONG        Flags,
    __in ACCESS_MASK        Access,
    __in POBJECT_ATTRIBUTES ObjectAttribute
    );

NTSTATUS NTAPI
NtAlpcOpenSenderThread(
    __out HANDLE            ThreadHandle,
    __in HANDLE             PortHandle,
    __in PPORT_MESSAGE      PortMessage,
    __reserved ULONG        Flags,
    __in ACCESS_MASK        Access,
    __in POBJECT_ATTRIBUTES ObjectAttributes
    );

NTSTATUS NTAPI
NtAlpcQueryInformation(
    __in HANDLE                      PortHandle,
    __in ALPC_PORT_INFORMATION_CLASS PortInformationClass,
    __out_bcount(Length) PVOID       PortInformation,
    __in ULONG                       Length,
    __out_opt PULONG                 ReturnLength
    );

NTSTATUS NTAPI
NtAlpcQueryInformationMessage(
    __in HANDLE                         PortHandle,
    __in PPORT_MESSAGE                  PortMessage,
    __in ALPC_MESSAGE_INFORMATION_CLASS MessageInformationClass,
    __out_bcount(Length) PVOID          MessageInformation,
    __in ULONG                          Length,
    __out_opt PULONG                    ReturnLength
    );

NTSTATUS NTAPI
NtAlpcRevokeSecurityContext(
    __in HANDLE      PortHandle,
    __reserved ULONG Flags,
    __in HANDLE      ContextHandle
    );

NTSTATUS NTAPI
NtAlpcSendWaitReceivePort(
    __in HANDLE                             PortHandle,
    __in ULONG                              Flags,
    __in_opt PPORT_MESSAGE                  SendMessage,
    __inout_opt PALPC_MESSAGE_ATTRIBUTES    SendMessageAttributes,
    __inout_opt PPORT_MESSAGE               ReceiveMessage,
    __inout_opt PULONG                      BufferLength,
    __inout_opt PALPC_MESSAGE_ATTRIBUTES    ReceiveMessageAttributes,
    __in_opt PLARGE_INTEGER                 TimeOut
    );

NTSTATUS NTAPI
NtAlpcSetInformation(
    __in HANDLE                      PortHandle
    __in ALPC_PORT_INFORMATION_CLASS PortInformationClass,
    __in_bcount(Length) PVOID        PortInformation,
    __in ULONG                       Length
    );
