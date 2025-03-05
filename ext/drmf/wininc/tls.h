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

#ifndef _WINDOWS_TLS_
#define _WINDOWS_TLS_ 1

typedef struct _THREAD_TLS_INFORMATION {
    ULONG        Flags;
    union {
        PVOID   *TlsVector;
        PVOID    TlsModulePointer;
    };
    ULONG_PTR    ThreadId;
} THREAD_TLS_INFORMATION, * PTHREAD_TLS_INFORMATION;

typedef enum _PROCESS_TLS_REQUEST {
    ProcessTlsReplaceIndex,
    ProcessTlsReplaceVector,
    MaxProcessTlsRequest
} PROCESS_TLS_REQUEST, *PPROCESS_TLS_REQUEST;

typedef struct _PROCESS_TLS_INFORMATION {
    ULONG                  Unknown;
    PROCESS_TLS_REQUEST    TlsRequest;
    ULONG                  ThreadDataCount;
    union {
        ULONG              TlsIndex;
        ULONG              TlsVectorLength;
    };
    THREAD_TLS_INFORMATION ThreadData[0];
} PROCESS_TLS_INFORMATION, *PPROCESS_TLS_INFORMATION;

#endif /* _WINDOWS_TLS_ */
