/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was generated from a ReactOS header to make
 ***   information necessary for userspace to call into the Windows
 ***   kernel available to Dr. Memory.  It contains only constants,
 ***   structures, and macros generated from the original header, and
 ***   thus, contains no copyrightable information.
 ***
 ****************************************************************************
 ****************************************************************************/

/* from reactos/include/reactos/winsock/msafdlib.h */

#ifndef __MSAFDLIB_H
#define __MSAFDLIB_H

/* Socket State */
typedef enum _SOCKET_STATE
{
    SocketUndefined = -1,
    SocketOpen,
    SocketBound,
    SocketBoundUdp,
    SocketConnected,
    SocketClosed
} SOCKET_STATE, *PSOCKET_STATE;

/*
 * Shared Socket Information.
 * It's called shared because we send it to Kernel-Mode for safekeeping
 */
typedef struct _SOCK_SHARED_INFO {
    SOCKET_STATE                State;
    INT                            AddressFamily;
    INT                            SocketType;
    INT                            Protocol;
    INT                            SizeOfLocalAddress;
    INT                            SizeOfRemoteAddress;
    struct linger                LingerData;
    ULONG                        SendTimeout;
    ULONG                        RecvTimeout;
    ULONG                        SizeOfRecvBuffer;
    ULONG                        SizeOfSendBuffer;
    struct {
        BOOLEAN                    Listening:1;
        BOOLEAN                    Broadcast:1;
        BOOLEAN                    Debug:1;
        BOOLEAN                    OobInline:1;
        BOOLEAN                    ReuseAddresses:1;
        BOOLEAN                    ExclusiveAddressUse:1;
        BOOLEAN                    NonBlocking:1;
        BOOLEAN                    DontUseWildcard:1;
        BOOLEAN                    ReceiveShutdown:1;
        BOOLEAN                    SendShutdown:1;
        BOOLEAN                    UseDelayedAcceptance:1;
        BOOLEAN                    UseSAN:1;
        /* timurrrr: based on XP 32-bit vs Win7 observations: i#375 */
        BOOLEAN                    HasGUID:1;
    }; // Flags
    DWORD                        CreateFlags;
    DWORD                        CatalogEntryId;
    DWORD                        ServiceFlags1;
    DWORD                        ProviderFlags;
    GROUP                        GroupID;
    DWORD                        GroupType;
    INT                            GroupPriority;
    INT                            SocketLastError;
    HWND                        hWnd;
    LONG                        Unknown;
    DWORD                        SequenceNumber;
    UINT                        wMsg;
    LONG                        AsyncEvents;
    LONG                        AsyncDisabledEvents;
} SOCK_SHARED_INFO, *PSOCK_SHARED_INFO;

/* The blob of data we send to Kernel-Mode for safekeeping */
/* i#375 observations: on 5.1, SOCKET_CONTEXT doesn't contain GUID in the middle */
typedef struct _SOCKET_CONTEXT_NOGUID {
    SOCK_SHARED_INFO SharedData;
    ULONG SizeOfHelperData;
    ULONG Padding;
    /* Plus SOCKADDR LocalAddress;  presumably var-len */
    /* Plus SOCKADDR RemoteAddress; presumably var-len */
    /* Plus Helper Data */
} SOCKET_CONTEXT_NOGUID, *PSOCKET_CONTEXT_NOGUID;

typedef struct _SOCKET_CONTEXT {
    SOCK_SHARED_INFO SharedData;
    GUID Guid; /* bruening: observed on XP 64-bit and win7 (i#375) */
    ULONG SizeOfHelperData;
    ULONG Padding;
    /* Plus SOCKADDR LocalAddress;  presumably var-len */
    /* Plus SOCKADDR RemoteAddress; presumably var-len */
    /* Plus Helper Data */
} SOCKET_CONTEXT, *PSOCKET_CONTEXT;

#endif /* __MSAFDLIB_H */
