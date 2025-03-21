/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was generated from Windows SDK and DDK headers to make
 ***   information necessary for userspace to call into the Windows
 ***   kernel available to Dr. Memory.  It contains only constants,
 ***   structures, and macros generated from the original header, and
 ***   thus, contains no copyrightable information.
 ***
 ****************************************************************************
 ****************************************************************************/

#ifndef _WINDEFS_H_
#define _WINDEFS_H_ 1

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define WIN_ALLOC_SIZE (64*1024)

/* We statically link with ntdll.lib from DDK.
 * We can only do this for routines we want to invoke in a client/privlib
 * context, though.
 */
#define GET_NTDLL(NtFunction, signature) NTSYSAPI NTSTATUS NTAPI NtFunction signature

/* For routines we want to invoke in an app context, we must dynamically
 * look them up to avoid DR's privlib import redirection.
 */
#define DECLARE_NTDLL(NtFunction, signature) \
    typedef NTSYSAPI NTSTATUS (NTAPI * NtFunction##_t) signature;      \
    static NtFunction##_t NtFunction

#define CBRET_INTERRUPT_NUM 0x2b

/***************************************************************************
 * from ntdef.h
 */

typedef enum _NT_PRODUCT_TYPE {
    NtProductWinNt = 1,
    NtProductLanManNt,
    NtProductServer
} NT_PRODUCT_TYPE, *PNT_PRODUCT_TYPE;

/***************************************************************************
 * from ntddk.h
 */

typedef struct _RTL_BITMAP {
    ULONG SizeOfBitMap;                     // Number of bits in bit map
    PULONG Buffer;                          // Pointer to the bit map itself
} RTL_BITMAP;
typedef RTL_BITMAP *PRTL_BITMAP;

#define PROCESSOR_FEATURE_MAX 64

typedef enum _ALTERNATIVE_ARCHITECTURE_TYPE {
    StandardDesign,                 // None == 0 == standard design
    NEC98x86,                       // NEC PC98xx series on X86
    EndAlternatives                 // past end of known alternatives
} ALTERNATIVE_ARCHITECTURE_TYPE;

typedef struct _KSYSTEM_TIME {
    ULONG LowPart;
    LONG High1Time;
    LONG High2Time;
} KSYSTEM_TIME, *PKSYSTEM_TIME;

#ifndef _WIN32_WINNT_WIN7
/* Win7-specific, in SDK 7.0 WINNT.h */

# define MAXIMUM_XSTATE_FEATURES             64

typedef struct _XSTATE_FEATURE {
    DWORD Offset;
    DWORD Size;
} XSTATE_FEATURE, *PXSTATE_FEATURE;

typedef struct _XSTATE_CONFIGURATION {
    // Mask of enabled features
    DWORD64 EnabledFeatures;

    // Total size of the save area
    DWORD Size;

    DWORD OptimizedSave : 1;

    // List of features (
    XSTATE_FEATURE Features[MAXIMUM_XSTATE_FEATURES];

} XSTATE_CONFIGURATION, *PXSTATE_CONFIGURATION;

typedef struct _PROCESSOR_NUMBER {
    WORD Group;
    BYTE Number;
    BYTE Reserved;
} PROCESSOR_NUMBER, *PPROCESSOR_NUMBER;
#endif /* _WIN32_WINNT_WIN7 */

typedef struct _KUSER_SHARED_DATA {

    //
    // Current low 32-bit of tick count and tick count multiplier.
    //
    // N.B. The tick count is updated each time the clock ticks.
    //

    ULONG TickCountLowDeprecated;
    ULONG TickCountMultiplier;

    //
    // Current 64-bit interrupt time in 100ns units.
    //

    volatile KSYSTEM_TIME InterruptTime;

    //
    // Current 64-bit system time in 100ns units.
    //

    volatile KSYSTEM_TIME SystemTime;

    //
    // Current 64-bit time zone bias.
    //

    volatile KSYSTEM_TIME TimeZoneBias;

    //
    // Support image magic number range for the host system.
    //
    // N.B. This is an inclusive range.
    //

    USHORT ImageNumberLow;
    USHORT ImageNumberHigh;

    //
    // Copy of system root in Unicode
    //

    WCHAR NtSystemRoot[ 260 ];

    //
    // Maximum stack trace depth if tracing enabled.
    //

    ULONG MaxStackTraceDepth;

    //
    // Crypto Exponent
    //

    ULONG CryptoExponent;

    //
    // TimeZoneId
    //

    ULONG TimeZoneId;

    ULONG LargePageMinimum;
    ULONG Reserved2[ 7 ];

    //
    // product type
    //

    NT_PRODUCT_TYPE NtProductType;
    BOOLEAN ProductTypeIsValid;

    //
    // NT Version. Note that each process sees a version from its PEB, but
    // if the process is running with an altered view of the system version,
    // the following two fields are used to correctly identify the version
    //

    ULONG NtMajorVersion;
    ULONG NtMinorVersion;

    //
    // Processor Feature Bits
    //

    BOOLEAN ProcessorFeatures[PROCESSOR_FEATURE_MAX];

    //
    // Reserved fields - do not use
    //
    ULONG Reserved1;
    ULONG Reserved3;

    //
    // Time slippage while in debugger
    //

    volatile ULONG TimeSlip;

    //
    // Alternative system architecture.  Example: NEC PC98xx on x86
    //

    ALTERNATIVE_ARCHITECTURE_TYPE AlternativeArchitecture;

    //
    // If the system is an evaluation unit, the following field contains the
    // date and time that the evaluation unit expires. A value of 0 indicates
    // that there is no expiration. A non-zero value is the UTC absolute time
    // that the system expires.
    //

    LARGE_INTEGER SystemExpirationDate;

    //
    // Suite Support
    //

    ULONG SuiteMask;

    //
    // TRUE if a kernel debugger is connected/enabled
    //

    BOOLEAN KdDebuggerEnabled;


    //
    // Current console session Id. Always zero on non-TS systems
    //
    volatile ULONG ActiveConsoleId;

    //
    // Force-dismounts cause handles to become invalid. Rather than
    // always probe handles, we maintain a serial number of
    // dismounts that clients can use to see if they need to probe
    // handles.
    //

    volatile ULONG DismountCount;

    //
    // This field indicates the status of the 64-bit COM+ package on the system.
    // It indicates whether the Itermediate Language (IL) COM+ images need to
    // use the 64-bit COM+ runtime or the 32-bit COM+ runtime.
    //

    ULONG ComPlusPackage;

    //
    // Time in tick count for system-wide last user input across all
    // terminal sessions. For MP performance, it is not updated all
    // the time (e.g. once a minute per session). It is used for idle
    // detection.
    //

    ULONG LastSystemRITEventTickCount;

    //
    // Number of physical pages in the system.  This can dynamically
    // change as physical memory can be added or removed from a running
    // system.
    //

    ULONG NumberOfPhysicalPages;

    //
    // True if the system was booted in safe boot mode.
    //

    BOOLEAN SafeBootMode;

    //
    // The following field is used for Heap  and  CritSec Tracing
    // The last bit is set for Critical Sec Collision tracing and
    // second Last bit is for Heap Tracing
    // Also the first 16 bits are used as counter.
    //

    ULONG TraceLogging;

    //
    // Depending on the processor, the code for fast system call
    // will differ, the following buffer is filled with the appropriate
    // code sequence and user mode code will branch through it.
    //
    // (32 bytes, using ULONGLONG for alignment).
    //
    // N.B. The following two fields are only used on 32-bit systems.
    //

    ULONGLONG   Fill0;          // alignment
    ULONGLONG   SystemCall[4];

    //
    // The 64-bit tick count.
    //

    union {
        volatile KSYSTEM_TIME TickCount;
        volatile ULONG64 TickCountQuad;
    };

    /********************* below here is Vista-only ********************
     * FIXME: should we avoid false pos by having Windows-version-specific
     * struct defs?  Not bothering for now.
     */

    //
    // Cookie for encoding pointers system wide.
    //

    ULONG Cookie;

    //
    // Client id of the process having the focus in the current
    // active console session id.
    //

    LONGLONG ConsoleSessionForegroundProcessId;

    //
    // Shared information for Wow64 processes.
    //

#define MAX_WOW64_SHARED_ENTRIES 16
    ULONG Wow64SharedInformation[MAX_WOW64_SHARED_ENTRIES];

    //
    // The following field is used for ETW user mode global logging
    // (UMGL).
    //

    USHORT UserModeGlobalLogger[8];
    ULONG HeapTracingPid[2];
    ULONG CritSecTracingPid[2];

    //
    // Settings that can enable the use of Image File Execution Options
    // from HKCU in addition to the original HKLM.
    //

    ULONG ImageFileExecutionOptions;

    //
    // This represents the affinity of active processors in the system.
    // This is updated by the kernel as processors are added\removed from
    // the system.
    //

    union {
        ULONGLONG AffinityPad;
        KAFFINITY ActiveProcessorAffinity;
    };

    //
    // Current 64-bit interrupt time bias in 100ns units.
    //

    volatile ULONG64 InterruptTimeBias;

    /********************* below here is Win7-only *********************/
    //
    // Current 64-bit performance counter bias in processor cycles.
    //

    volatile ULONG64 TscQpcBias;

    //
    // Number of active processors and groups.
    //

    volatile ULONG ActiveProcessorCount;
    volatile USHORT ActiveGroupCount;
    USHORT Reserved4;

    //
    // This value controls the AIT Sampling rate.
    //

    volatile ULONG AitSamplingValue;
    volatile ULONG AppCompatFlag;

    //
    // Relocation diff for ntdll (native and wow64).
    //

    ULONGLONG SystemDllNativeRelocation;
    ULONG SystemDllWowRelocation;

    //
    // Extended processor state configuration
    //

    ULONG XStatePad[1];
    XSTATE_CONFIGURATION XState;

} KUSER_SHARED_DATA, *PKUSER_SHARED_DATA;

/***************************************************************************
 * from winternl.h and pdb files
 */

typedef LONG NTSTATUS;
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#define NT_CURRENT_PROCESS ( (HANDLE) -1 )

typedef struct _UNICODE_STRING {
    /* Length field is size in bytes not counting final 0 */
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING;
typedef UNICODE_STRING *PUNICODE_STRING;

typedef struct _IO_STATUS_BLOCK {
  union {
    NTSTATUS Status;
    PVOID Pointer;
  } StatusPointer;
  ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

typedef struct _OBJECT_ATTRIBUTES {
    ULONG Length;
    HANDLE RootDirectory;
    PUNICODE_STRING ObjectName;
    ULONG Attributes;
    PVOID SecurityDescriptor;
    PVOID SecurityQualityOfService;
} OBJECT_ATTRIBUTES;
typedef OBJECT_ATTRIBUTES *POBJECT_ATTRIBUTES;

typedef struct _RTL_USER_PROCESS_PARAMETERS {
    ULONG MaximumLength;
    ULONG Length;
    ULONG Flags;
    ULONG DebugFlags;
    PVOID ConsoleHandle;
    ULONG ConsoleFlags;
    HANDLE StdInputHandle;
    HANDLE StdOutputHandle;
    HANDLE StdErrorHandle;
    UNICODE_STRING CurrentDirectoryPath;
    HANDLE CurrentDirectoryHandle;
    UNICODE_STRING DllPath;
    UNICODE_STRING ImagePathName;
    UNICODE_STRING CommandLine;
    PVOID Environment;
    ULONG StartingPositionLeft;
    ULONG StartingPositionTop;
    ULONG Width;
    ULONG Height;
    ULONG CharWidth;
    ULONG CharHeight;
    ULONG ConsoleTextAttributes;
    ULONG WindowFlags;
    ULONG ShowWindowFlags;
    UNICODE_STRING WindowTitle;
    UNICODE_STRING DesktopName;
    UNICODE_STRING ShellInfo;
    UNICODE_STRING RuntimeData;
    // RTL_DRIVE_LETTER_CURDIR DLCurrentDirectory[0x20]
} RTL_USER_PROCESS_PARAMETERS, *PRTL_USER_PROCESS_PARAMETERS;

#define TLS_EXPANSION_BITMAP_SLOTS 1024


/* The layout here is from ntdll pdb on win8, though we
 * changed some PVOID types to more specific types.
 * XXX: should share with dynamorio's ntdll.h.
 */
typedef struct _PEB {                                     /* offset: 32bit / 64bit */
    BOOLEAN                      InheritedAddressSpace;           /* 0x000 / 0x000 */
    BOOLEAN                      ReadImageFileExecOptions;        /* 0x001 / 0x001 */
    BOOLEAN                      BeingDebugged;                   /* 0x002 / 0x002 */
#if 0
    /* x64 xpsp2 lists this as a bitfield but compiler only accepts int bitfields: */
    BOOLEAN                      ImageUsesLargePages:1;           /* 0x003 / 0x003 */
    BOOLEAN                      SpareBits:7;                     /* 0x003 / 0x003 */
#else
    BOOLEAN                      ImageUsesLargePages;             /* 0x003 / 0x003 */
#endif
    HANDLE                       Mutant;                          /* 0x004 / 0x008 */
    PVOID                        ImageBaseAddress;                /* 0x008 / 0x010 */
    PVOID /* PPEB_LDR_DATA */    LoaderData;                      /* 0x00c / 0x018 */
    PVOID /* PRTL_USER_PROCESS_PARAMETERS */ ProcessParameters;   /* 0x010 / 0x020 */
    PVOID                        SubSystemData;                   /* 0x014 / 0x028 */
    PVOID                        ProcessHeap;                     /* 0x018 / 0x030 */
    PVOID /* PRTL_CRITICAL_SECTION */ FastPebLock;                /* 0x01c / 0x038 */
#if 0
    /* x64 xpsp2 lists these fields as: */
    PVOID                        AtlThunkSListPtr;                /* 0x020 / 0x040 */
    PVOID                        SparePtr2;                       /* 0x024 / 0x048 */
#else
    /* xpsp2 and earlier */
    PVOID /* PPEBLOCKROUTINE */  FastPebLockRoutine;              /* 0x020 / 0x040 */
    PVOID /* PPEBLOCKROUTINE */  FastPebUnlockRoutine;            /* 0x024 / 0x048 */
#endif
    DWORD                        EnvironmentUpdateCount;          /* 0x028 / 0x050 */
    PVOID                        KernelCallbackTable;             /* 0x02c / 0x058 */
#if 0
    /* x64 xpsp2 lists these fields as: */
    DWORD                        SystemReserved[1];               /* 0x030 / 0x060 */
    DWORD                        SpareUlong;                      /* 0x034 / 0x064 */
#else
    /* xpsp2 and earlier */
    DWORD                        EvengLogSection;                 /* 0x030 / 0x060 */
    DWORD                        EventLog;                        /* 0x034 / 0x064 */
#endif
    PVOID /* PPEB_FREE_BLOCK */  FreeList;                        /* 0x038 / 0x068 */
    DWORD                        TlsExpansionCounter;             /* 0x03c / 0x070 */
    PRTL_BITMAP                  TlsBitmap;                       /* 0x040 / 0x078 */
    DWORD                        TlsBitmapBits[2];                /* 0x044 / 0x080 */
    PVOID                        ReadOnlySharedMemoryBase;        /* 0x04c / 0x088 */
    PVOID                        ReadOnlySharedMemoryHeap;        /* 0x050 / 0x090 */
    PVOID /* PPVOID */           ReadOnlyStaticServerData;        /* 0x054 / 0x098 */
    PVOID                        AnsiCodePageData;                /* 0x058 / 0x0a0 */
    PVOID                        OemCodePageData;                 /* 0x05c / 0x0a8 */
    PVOID                        UnicodeCaseTableData;            /* 0x060 / 0x0b0 */
    DWORD                        NumberOfProcessors;              /* 0x064 / 0x0b8 */
    DWORD                        NtGlobalFlag;                    /* 0x068 / 0x0bc */
    LARGE_INTEGER                CriticalSectionTimeout;          /* 0x070 / 0x0c0 */
    UINT_PTR                     HeapSegmentReserve;              /* 0x078 / 0x0c8 */
    UINT_PTR                     HeapSegmentCommit;               /* 0x07c / 0x0d0 */
    UINT_PTR                     HeapDeCommitTotalFreeThreshold;  /* 0x080 / 0x0d8 */
    UINT_PTR                     HeapDeCommitFreeBlockThreshold;  /* 0x084 / 0x0e0 */
    DWORD                        NumberOfHeaps;                   /* 0x088 / 0x0e8 */
    DWORD                        MaximumNumberOfHeaps;            /* 0x08c / 0x0ec */
    PVOID /* PPVOID */           ProcessHeaps;                    /* 0x090 / 0x0f0 */
    PVOID                        GdiSharedHandleTable;            /* 0x094 / 0x0f8 */
    PVOID                        ProcessStarterHelper;            /* 0x098 / 0x100 */
    DWORD                        GdiDCAttributeList;              /* 0x09c / 0x108 */
    PVOID /* PRTL_CRITICAL_SECTION */ LoaderLock;                 /* 0x0a0 / 0x110 */
    DWORD                        OSMajorVersion;                  /* 0x0a4 / 0x118 */
    DWORD                        OSMinorVersion;                  /* 0x0a8 / 0x11c */
    WORD                         OSBuildNumber;                   /* 0x0ac / 0x120 */
    WORD                         OSCSDVersion;                    /* 0x0ae / 0x122 */
    DWORD                        OSPlatformId;                    /* 0x0b0 / 0x124 */
    DWORD                        ImageSubsystem;                  /* 0x0b4 / 0x128 */
    DWORD                        ImageSubsystemMajorVersion;      /* 0x0b8 / 0x12c */
    DWORD                        ImageSubsystemMinorVersion;      /* 0x0bc / 0x130 */
    UINT_PTR                     ImageProcessAffinityMask;        /* 0x0c0 / 0x138 */
#ifdef X64
    DWORD                        GdiHandleBuffer[60];             /* 0x0c4 / 0x140 */
#else
    DWORD                        GdiHandleBuffer[34];             /* 0x0c4 / 0x140 */
#endif
    PVOID                        PostProcessInitRoutine;          /* 0x14c / 0x230 */
    PRTL_BITMAP                  TlsExpansionBitmap;              /* 0x150 / 0x238 */
    DWORD                        TlsExpansionBitmapBits[32];      /* 0x154 / 0x240 */
    DWORD                        SessionId;                       /* 0x1d4 / 0x2c0 */
    ULARGE_INTEGER               AppCompatFlags;                  /* 0x1d8 / 0x2c8 */
    ULARGE_INTEGER               AppCompatFlagsUser;              /* 0x1e0 / 0x2d0 */
    PVOID                        pShimData;                       /* 0x1e8 / 0x2d8 */
    PVOID                        AppCompatInfo;                   /* 0x1ec / 0x2e0 */
    UNICODE_STRING               CSDVersion;                      /* 0x1f0 / 0x2e8 */
    PVOID                        ActivationContextData;           /* 0x1f8 / 0x2f8 */
    PVOID                        ProcessAssemblyStorageMap;       /* 0x1fc / 0x300 */
    PVOID                        SystemDefaultActivationContextData;/* 0x200 / 0x308 */
    PVOID                        SystemAssemblyStorageMap;        /* 0x204 / 0x310 */
    UINT_PTR                     MinimumStackCommit;              /* 0x208 / 0x318 */
    PVOID /* PPVOID */           FlsCallback;                     /* 0x20c / 0x320 */
    LIST_ENTRY                   FlsListHead;                     /* 0x210 / 0x328 */
    PVOID                        FlsBitmap;                       /* 0x218 / 0x338 */
    DWORD                        FlsBitmapBits[4];                /* 0x21c / 0x340 */
    DWORD                        FlsHighIndex;                    /* 0x22c / 0x350 */
    PVOID                        WerRegistrationData;             /* 0x230 / 0x358 */
    PVOID                        WerShipAssertPtr;                /* 0x234 / 0x360 */
    PVOID                        pUnused;                         /* 0x238 / 0x368 */
    PVOID                        pImageHeaderHash;                /* 0x23c / 0x370 */
    union {
        ULONG                    TracingFlags;                    /* 0x240 / 0x378 */
        struct {
            ULONG                HeapTracingEnabled:1;            /* 0x240 / 0x378 */
            ULONG                CritSecTracingEnabled:1;         /* 0x240 / 0x378 */
            ULONG                LibLoaderTracingEnabled:1;       /* 0x240 / 0x378 */
            ULONG                SpareTracingBits:29;             /* 0x240 / 0x378 */
        };
    };
    ULONG64                      CsrServerReadOnlySharedMemoryBase;/*0x248 / 0x380 */
} PEB, *PPEB;

typedef struct _CLIENT_ID {
    /* These are numeric ids */
    HANDLE UniqueProcess;
    HANDLE UniqueThread;
} CLIENT_ID;
typedef CLIENT_ID *PCLIENT_ID;

typedef struct _GDI_TEB_BATCH
{
    ULONG  Offset;
    HANDLE HDC;
    ULONG  Buffer[0x136];
} GDI_TEB_BATCH;

/* The layout here is from ntdll pdb on win8.
 * XXX: should share with dynamorio's ntdll.h.
 */
typedef struct _TEB {                               /* offset: 32bit / 64bit */
    /* We lay out NT_TIB, which is declared in winnt.h */
    PVOID /* PEXCEPTION_REGISTRATION_RECORD */ExceptionList;/* 0x000 / 0x000 */
    PVOID                     StackBase;                    /* 0x004 / 0x008 */
    PVOID                     StackLimit;                   /* 0x008 / 0x010 */
    PVOID                     SubSystemTib;                 /* 0x00c / 0x018 */
    union {
        PVOID                 FiberData;                    /* 0x010 / 0x020 */
        DWORD                 Version;                      /* 0x010 / 0x020 */
    };
    PVOID                     ArbitraryUserPointer;         /* 0x014 / 0x028 */
    struct _TEB*              Self;                         /* 0x018 / 0x030 */
    PVOID                     EnvironmentPointer;           /* 0x01c / 0x038 */
    CLIENT_ID                 ClientId;                     /* 0x020 / 0x040 */
    PVOID                     ActiveRpcHandle;              /* 0x028 / 0x050 */
    PVOID                     ThreadLocalStoragePointer;    /* 0x02c / 0x058 */
    PEB*                      ProcessEnvironmentBlock;      /* 0x030 / 0x060 */
    DWORD                     LastErrorValue;               /* 0x034 / 0x068 */
    DWORD                     CountOfOwnedCriticalSections; /* 0x038 / 0x06c */
    PVOID                     CsrClientThread;              /* 0x03c / 0x070 */
    PVOID                     Win32ThreadInfo;              /* 0x040 / 0x078 */
    DWORD                     User32Reserved[26];           /* 0x044 / 0x080 */
    DWORD                     UserReserved[5];              /* 0x0ac / 0x0e8 */
    PVOID                     WOW32Reserved;                /* 0x0c0 / 0x100 */
    DWORD                     CurrentLocale;                /* 0x0c4 / 0x108 */
    DWORD                     FpSoftwareStatusRegister;     /* 0x0c8 / 0x10c */
    PVOID /* kernel32 data */ SystemReserved1[54];          /* 0x0cc / 0x110 */
    LONG                      ExceptionCode;                /* 0x1a4 / 0x2c0 */
    PVOID                     ActivationContextStackPointer;/* 0x1a8 / 0x2c8 */
#ifdef X64
    byte                      SpareBytes1[28];              /* 0x1ac / 0x2d0 */
#else
    byte                      SpareBytes1[40];              /* 0x1ac / 0x2d0 */
#endif
    GDI_TEB_BATCH             GdiTebBatch;                  /* 0x1d4 / 0x2f0 */
    CLIENT_ID                 RealClientId;                 /* 0x6b4 / 0x7d8 */
    PVOID                     GdiCachedProcessHandle;       /* 0x6bc / 0x7e8 */
    DWORD                     GdiClientPID;                 /* 0x6c0 / 0x7f0 */
    DWORD                     GdiClientTID;                 /* 0x6c4 / 0x7f4 */
    PVOID                     GdiThreadLocalInfo;           /* 0x6c8 / 0x7f8 */
    UINT_PTR                  Win32ClientInfo[62];          /* 0x6cc / 0x800 */
    PVOID                     glDispatchTable[233];         /* 0x7c4 / 0x9f0 */
    UINT_PTR                  glReserved1[29];              /* 0xb68 / 0x1138 */
    PVOID                     glReserved2;                  /* 0xbdc / 0x1220 */
    PVOID                     glSectionInfo;                /* 0xbe0 / 0x1228 */
    PVOID                     glSection;                    /* 0xbe4 / 0x1230 */
    PVOID                     glTable;                      /* 0xbe8 / 0x1238 */
    PVOID                     glCurrentRC;                  /* 0xbec / 0x1240 */
    PVOID                     glContext;                    /* 0xbf0 / 0x1248 */
    DWORD                     LastStatusValue;              /* 0xbf4 / 0x1250 */
    UNICODE_STRING            StaticUnicodeString;          /* 0xbf8 / 0x1258 */
    WORD                      StaticUnicodeBuffer[261];     /* 0xc00 / 0x1268 */
    PVOID                     DeallocationStack;            /* 0xe0c / 0x1478 */
    PVOID                     TlsSlots[64];                 /* 0xe10 / 0x1480 */
    LIST_ENTRY                TlsLinks;                     /* 0xf10 / 0x1680 */
    PVOID                     Vdm;                          /* 0xf18 / 0x1690 */
    PVOID                     ReservedForNtRpc;             /* 0xf1c / 0x1698 */
    PVOID                     DbgSsReserved[2];             /* 0xf20 / 0x16a0 */
    DWORD                     HardErrorMode;                /* 0xf28 / 0x16b0 */
    PVOID                     Instrumentation[14];          /* 0xf2c / 0x16b8 */
    PVOID                     SubProcessTag;                /* 0xf64 / 0x1728 */
    PVOID                     EtwTraceData;                 /* 0xf68 / 0x1730 */
    PVOID                     WinSockData;                  /* 0xf6c / 0x1738 */
    DWORD                     GdiBatchCount;                /* 0xf70 / 0x1740 */
    byte                      InDbgPrint;                   /* 0xf74 / 0x1744 */
    byte                      FreeStackOnTermination;       /* 0xf75 / 0x1745 */
    byte                      HasFiberData;                 /* 0xf76 / 0x1746 */
    byte                      IdealProcessor;               /* 0xf77 / 0x1747 */
    DWORD                     GuaranteedStackBytes;         /* 0xf78 / 0x1748 */
    PVOID                     ReservedForPerf;              /* 0xf7c / 0x1750 */
    PVOID                     ReservedForOle;               /* 0xf80 / 0x1758 */
    DWORD                     WaitingOnLoaderLock;          /* 0xf84 / 0x1760 */
    UINT_PTR                  SparePointer1;                /* 0xf88 / 0x1768 */
    UINT_PTR                  SoftPatchPtr1;                /* 0xf8c / 0x1770 */
    UINT_PTR                  SoftPatchPtr2;                /* 0xf90 / 0x1778 */
    PVOID /* PPVOID */        TlsExpansionSlots;            /* 0xf94 / 0x1780 */
#ifdef X64
    PVOID                     DeallocationBStore;           /* ----- / 0x1788 */
    PVOID                     BStoreLimit;                  /* ----- / 0x1790 */
#endif
    DWORD                     ImpersonationLocale;          /* 0xf98 / 0x1798 */
    DWORD                     IsImpersonating;              /* 0xf9c / 0x179c */
    PVOID                     NlsCache;                     /* 0xfa0 / 0x17a0 */
    PVOID                     pShimData;                    /* 0xfa4 / 0x17a8 */
    DWORD                     HeapVirtualAffinity;          /* 0xfa8 / 0x17b0 */
    PVOID                     CurrentTransactionHandle;     /* 0xfac / 0x17b8 */
    PVOID                     ActiveFrame;                  /* 0xfb0 / 0x17c0 */
    PVOID                     FlsData;                      /* 0xfb4 / 0x17c8 */
#ifndef PRE_VISTA_TEB /* pre-vs-post-Vista: we'll have to make a union if we care */
    PVOID                     PreferredLanguages;           /* 0xfb8 / 0x17d0 */
    PVOID                     UserPrefLanguages;            /* 0xfbc / 0x17d8 */
    PVOID                     MergedPrefLanguages;          /* 0xfc0 / 0x17e0 */
    ULONG                     MuiImpersonation;             /* 0xfc4 / 0x17e8 */
    union {
        USHORT                CrossTebFlags;                /* 0xfc8 / 0x17ec */
        USHORT                SpareCrossTebFlags:16;        /* 0xfc8 / 0x17ec */
    };
    union
    {
        USHORT                SameTebFlags;                 /* 0xfca / 0x17ee */
        struct {
            USHORT            SafeThunkCall:1;              /* 0xfca / 0x17ee */
            USHORT            InDebugPrint:1;               /* 0xfca / 0x17ee */
            USHORT            HasFiberData2:1;              /* 0xfca / 0x17ee */
            USHORT            SkipThreadAttach:1;           /* 0xfca / 0x17ee */
            USHORT            WerInShipAssertCode:1;        /* 0xfca / 0x17ee */
            USHORT            RanProcessInit:1;             /* 0xfca / 0x17ee */
            USHORT            ClonedThread:1;               /* 0xfca / 0x17ee */
            USHORT            SuppressDebugMsg:1;           /* 0xfca / 0x17ee */
            USHORT            DisableUserStackWalk:1;       /* 0xfca / 0x17ee */
            USHORT            RtlExceptionAttached:1;       /* 0xfca / 0x17ee */
            USHORT            InitialThread:1;              /* 0xfca / 0x17ee */
            USHORT            SessionAware:1;               /* 0xfca / 0x17ee */
            USHORT            SpareSameTebBits:4;           /* 0xfca / 0x17ee */
        };
    };
    PVOID                     TxnScopeEntercallback;        /* 0xfcc / 0x17f0 */
    PVOID                     TxnScopeExitCAllback;         /* 0xfd0 / 0x17f8 */
    PVOID                     TxnScopeContext;              /* 0xfd4 / 0x1800 */
    ULONG                     LockCount;                    /* 0xfd8 / 0x1808 */
    ULONG                     SpareUlong0;                  /* 0xfdc / 0x180c */
    PVOID                     ResourceRetValue;             /* 0xfe0 / 0x1810 */
    PVOID                     ReservedForWdf;               /* 0xfe4 / 0x1818 */
#else /* pre-Vista: */
    byte                      SafeThunkCall;                /* 0xfb8 / 0x17d0 */
    byte                      BooleanSpare[3];              /* 0xfb9 / 0x17d1 */
#endif
} TEB;

typedef struct _PORT_SECTION_WRITE {
    ULONG Length;
    HANDLE SectionHandle;
    ULONG SectionOffset;
    ULONG ViewSize;
    PVOID ViewBase;
    PVOID TargetViewBase;
} PORT_SECTION_WRITE, *PPORT_SECTION_WRITE;

typedef struct _PORT_SECTION_READ {
    ULONG Length;
    ULONG ViewSize;
    ULONG ViewBase;
} PORT_SECTION_READ, *PPORT_SECTION_READ;

typedef struct _FILE_USER_QUOTA_INFORMATION {
    ULONG NextEntryOffset;
    ULONG SidLength;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER QuotaUsed;
    LARGE_INTEGER QuotaThreshold;
    LARGE_INTEGER QuotaLimit;
    SID Sid[1];
} FILE_USER_QUOTA_INFORMATION, *PFILE_USER_QUOTA_INFORMATION;

typedef struct _FILE_QUOTA_LIST_INFORMATION {
    ULONG NextEntryOffset;
    ULONG SidLength;
    SID Sid[1];
} FILE_QUOTA_LIST_INFORMATION, *PFILE_QUOTA_LIST_INFORMATION;

typedef struct _USER_STACK {
    PVOID FixedStackBase;
    PVOID FixedStackLimit;
    PVOID ExpandableStackBase;
    PVOID ExpandableStackLimit;
    PVOID ExpandableStackBottom;
} USER_STACK, *PUSER_STACK;

typedef
VOID
(*PTIMER_APC_ROUTINE) (
    __in PVOID TimerContext,
    __in ULONG TimerLowValue,
    __in LONG TimerHighValue
    );

/***************************************************************************
 * from wdm.h
 */

typedef struct _FILE_BASIC_INFORMATION {
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    ULONG FileAttributes;
} FILE_BASIC_INFORMATION, *PFILE_BASIC_INFORMATION;

typedef struct _FILE_NETWORK_OPEN_INFORMATION {
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER AllocationSize;
    LARGE_INTEGER EndOfFile;
    ULONG FileAttributes;
} FILE_NETWORK_OPEN_INFORMATION, *PFILE_NETWORK_OPEN_INFORMATION;

typedef struct _FILE_FULL_EA_INFORMATION {
    ULONG NextEntryOffset;
    UCHAR Flags;
    UCHAR EaNameLength;
    USHORT EaValueLength;
    CHAR EaName[1];
} FILE_FULL_EA_INFORMATION, *PFILE_FULL_EA_INFORMATION;

typedef struct _KEY_VALUE_ENTRY {
    PUNICODE_STRING ValueName;
    ULONG           DataLength;
    ULONG           DataOffset;
    ULONG           Type;
} KEY_VALUE_ENTRY, *PKEY_VALUE_ENTRY;

typedef
VOID
(*PKNORMAL_ROUTINE) (
    DR_PARAM_IN PVOID NormalContext,
    DR_PARAM_IN PVOID SystemArgument1,
    DR_PARAM_IN PVOID SystemArgument2
    );

typedef
VOID
(NTAPI *PIO_APC_ROUTINE) (
    DR_PARAM_IN PVOID ApcContext,
    DR_PARAM_IN PIO_STATUS_BLOCK IoStatusBlock,
    DR_PARAM_IN ULONG Reserved
    );

#ifdef X64
# define PORT_MAXIMUM_MESSAGE_LENGTH 512
#else
# define PORT_MAXIMUM_MESSAGE_LENGTH 256
#endif

/***************************************************************************
 * from ntifs.h
 */

typedef short CSHORT;
#define LPC_SIZE_T SIZE_T
#define LPC_CLIENT_ID CLIENT_ID

typedef struct _PORT_MESSAGE {
    union {
        struct {
            CSHORT DataLength;
            CSHORT TotalLength;
        } s1;
        ULONG Length;
    } u1;
    union {
        struct {
            CSHORT Type;
            CSHORT DataInfoOffset;
        } s2;
        ULONG ZeroInit;
    } u2;
    union {
        LPC_CLIENT_ID ClientId;
        double DoNotUseThisField;       // Force quadword alignment
    };
    ULONG MessageId;
    union {
        LPC_SIZE_T ClientViewSize;          // Only valid on LPC_CONNECTION_REQUEST message
        ULONG CallbackId;                   // Only valid on LPC_REQUEST message
    } u3;
//  UCHAR Data[];
} PORT_MESSAGE, *PPORT_MESSAGE;

typedef struct _FILE_GET_EA_INFORMATION {
    ULONG NextEntryOffset;
    UCHAR EaNameLength;
    CHAR EaName[1];
} FILE_GET_EA_INFORMATION, *PFILE_GET_EA_INFORMATION;

#if defined(USE_LPC6432)
#define LPC_CLIENT_ID CLIENT_ID64
#define LPC_SIZE_T ULONGLONG
#define LPC_PVOID ULONGLONG
#define LPC_HANDLE ULONGLONG
#else
#define LPC_CLIENT_ID CLIENT_ID
#define LPC_SIZE_T SIZE_T
#define LPC_PVOID PVOID
#define LPC_HANDLE HANDLE
#endif

typedef struct _PORT_VIEW {
    ULONG Length;
    LPC_HANDLE SectionHandle;
    ULONG SectionOffset;
    LPC_SIZE_T ViewSize;
    LPC_PVOID ViewBase;
    LPC_PVOID ViewRemoteBase;
} PORT_VIEW, *PPORT_VIEW;

typedef struct _REMOTE_PORT_VIEW {
    ULONG Length;
    LPC_SIZE_T ViewSize;
    LPC_PVOID ViewBase;
} REMOTE_PORT_VIEW, *PREMOTE_PORT_VIEW;


/***************************************************************************
 * from winsdk-6.1.6000/Include/Evntrace.h (issues including it directly)
 */

//
// Trace header for all legacy events.
//

typedef struct _EVENT_TRACE_HEADER {        // overlays WNODE_HEADER
    USHORT          Size;                   // Size of entire record
    union {
        USHORT      FieldTypeFlags;         // Indicates valid fields
        struct {
            UCHAR   HeaderType;             // Header type - internal use only
            UCHAR   MarkerFlags;            // Marker - internal use only
        };
    };
    union {
        ULONG       Version;
        struct {
            UCHAR   Type;                   // event type
            UCHAR   Level;                  // trace instrumentation level
            USHORT  Version;                // version of trace record
        } Class;
    };
    ULONG           ThreadId;               // Thread Id
    ULONG           ProcessId;              // Process Id
    LARGE_INTEGER   TimeStamp;              // time when event happens
    union {
        GUID        Guid;                   // Guid that identifies event
        ULONGLONG   GuidPtr;                // use with WNODE_FLAG_USE_GUID_PTR
    };
    union {
        struct {
            ULONG   KernelTime;             // Kernel Mode CPU ticks
            ULONG   UserTime;               // User mode CPU ticks
        };
        ULONG64     ProcessorTime;          // Processor Clock
        struct {
            ULONG   ClientContext;          // Reserved
            ULONG   Flags;                  // Event Flags
        };
    };
} EVENT_TRACE_HEADER, *PEVENT_TRACE_HEADER;


/***************************************************************************
 * UNKNOWN
 * Can't find these anywhere
 */

typedef struct _CHANNEL_MESSAGE {
    ULONG unknown;
} CHANNEL_MESSAGE, *PCHANNEL_MESSAGE;

/***************************************************************************
 * from DynamoRIO core/win32/ntdll.h
 */

/* Speculated arg 10 to NtCreateUserProcess.
 * Note the similarities to CreateThreadEx arg 11 below.  Struct starts with size then
 * after that looks kind of like an array of 16 byte (32 on 64-bit) elements corresponding
 * to the IN and OUT informational ptrs.  Each array elment consists of a ?flags? then
 * the sizeof of the IN/OUT ptr buffer then the ptr itself then 0.
 */

typedef enum { /* NOTE - these are speculative */
    THREAD_INFO_ELEMENT_BUFFER_IS_INOUT = 0x00000, /* buffer is ??IN/OUT?? */
    THREAD_INFO_ELEMENT_BUFFER_IS_OUT   = 0x10000, /* buffer is IN (?) */
    THREAD_INFO_ELEMENT_BUFFER_IS_IN    = 0x20000, /* buffer is OUT (?) */
} thread_info_elm_buf_access_t;

typedef enum { /* NOTE - these are speculative */
    THREAD_INFO_ELEMENT_CLIENT_ID       = 0x3, /* buffer is CLIENT_ID - OUT */
    THREAD_INFO_ELEMENT_TEB             = 0x4, /* buffer is TEB * - OUT */
    THREAD_INFO_ELEMENT_NT_PATH_TO_EXE  = 0x5, /* buffer is wchar * path to exe
                                                * [ i.e. L"\??\c:\foo.exe" ] - IN */
    THREAD_INFO_ELEMENT_EXE_STUFF       = 0x6, /* buffer is exe_stuff_t (see above)
                                                * - DR_PARAM_INOUT */
    THREAD_INFO_ELEMENT_UNKNOWN_1       = 0x9, /* Unknown - ptr_uint_t sized
                                                * [ observed 1 ] - IN */
} thread_info_elm_buf_type_t;

typedef struct _thread_info_element_t { /* NOTE - this is speculative */
    ptr_uint_t flags;   /* thread_info_elm_buf_access_t | thread_info_elm_buf_type_t */
    size_t buffer_size; /* sizeof of buffer, in bytes */
    void *buffer;       /* flags determine disposition, could be IN or OUT or both */
    ptr_uint_t unknown;  /* [ observed always 0 ] */
} thread_info_elm_t;

typedef struct _exe_stuff_t { /* NOTE - this is speculative */
    DR_PARAM_OUT void *exe_entrypoint_addr; /* Entry point to the exe being started. */
    // ratio of uint32 to ptr_uint_t assumes no larger changes between 32 and 64-bit
    ptr_uint_t unknown1[3]; // possibly intermixed with uint32s below IN? OUT?
    uint unknown2[8];       // possible intermixed with ptr_uint_ts above IN? OUT?
} exe_stuff_t;

typedef struct _create_proc_thread_info_t { /* NOTE - this is speculative */
    size_t struct_size; /* observed 0x34 or 0x44 (0x68 on 64-bit) = sizeof(this struct) */
    /* Observed - first thread_info_elm_t always
     * flags = 0x20005
     * buffer_size = varies (sizeof buffer string in bytes)
     * buffer = wchar * : nt path to executable i.e. "\??\c:\foo.exe" - IN */
    thread_info_elm_t nt_path_to_exe;
    /* Observed - second thread_info_elm_t always
     * flags = 0x10003
     * buffer_size = sizeof(CLIENT_ID)
     * buffer = PCLIENT_ID : OUT */
    thread_info_elm_t client_id;
    /* Observed - third thread_info_elm_t always
     * flags = 0x6
     * buffer_size = 0x30 (or 0x40 on 64-bit) == sizeof(exe_stuff_t)
     * buffer = exe_stuff_t * : IN/OUT */
    thread_info_elm_t exe_stuff;
    /* While the first three thread_info_elm_t have been present in every call I've seen
     * (and attempts to remove or re-arrange them caused the system call to fail,
     * assuming I managed to do it right), there's more variation in the later fields
     * (sometimes present, sometimes not) - most commonly there'll be nothing or just the
     * TEB * info field (flags = 0x10003) which I've seen here a lot on 32bit. */
#if 0 /* 0 sized array is non-standard extension */
    thread_info_elm_t info[];
#endif
}  create_proc_thread_info_t;

/* Speculated arg 11 to NtCreateThreadEx.  See the similar arg 10 of
 * NtCreateUserProcess above. */
typedef struct _create_thread_info_t { /* NOTE - this is speculative */
    size_t struct_size; /* observed 0x24 (0x48 on 64-bit) == sizeof(this struct) */
    /* Note kernel32!CreateThread hardcodes all the values in this structure and
     * I've never seen any variation elsewhere. Trying to swap the order caused the
     * system call to fail when I tried it (assuming I did it right). */
    /* Observed - always
     * flags = 0x10003
     * buffer_size = sizeof(CLIENT_ID)
     * buffer = PCLIENT_ID : OUT */
    thread_info_elm_t client_id;
    /* Observed - always
     * flags = 0x10004
     * buffer_size = sizeof(CLIENT_ID)
     * buffer = TEB ** : OUT */
    thread_info_elm_t teb;
} create_thread_info_t;


typedef enum _KEY_VALUE_INFORMATION_CLASS {
    KeyValueBasicInformation,
    KeyValueFullInformation,
    KeyValuePartialInformation,
    KeyValueFullInformationAlign64,
    KeyValuePartialInformationAlign64
} KEY_VALUE_INFORMATION_CLASS;

typedef struct _KEY_VALUE_FULL_INFORMATION {
    ULONG   TitleIndex;
    ULONG   Type;
    ULONG   DataOffset;
    ULONG   DataLength;
    ULONG   NameLength;
    WCHAR   Name[1];            // Variable size
//          Data[1]             // Variable size data not declared
} KEY_VALUE_FULL_INFORMATION, *PKEY_VALUE_FULL_INFORMATION;

typedef struct _KEY_VALUE_PARTIAL_INFORMATION {
    ULONG   TitleIndex;
    ULONG   Type;
    ULONG   DataLength;
    UCHAR   Data[1];            // Variable size
} KEY_VALUE_PARTIAL_INFORMATION, *PKEY_VALUE_PARTIAL_INFORMATION;

/* "A process has requested access to an object, but has not been
 * granted those access rights."
 */
#define STATUS_ACCESS_DENIED             ((NTSTATUS)0xC0000022L)
/* "The data was too large to fit into the specified buffer." */
#define STATUS_BUFFER_OVERFLOW           ((NTSTATUS)0x80000005L)
/* "The buffer is too small to contain the entry. No information has
 * been written to the buffer."
 */
#define STATUS_BUFFER_TOO_SMALL          ((NTSTATUS)0xC0000023L)
/* "The specified information record length does not match the length that is
 * required for the specified information class."
 */
#define STATUS_INFO_LENGTH_MISMATCH      ((NTSTATUS)0xC0000004L)

typedef struct _SYSTEM_BASIC_INFORMATION {
    ULONG   Unknown;
    ULONG   MaximumIncrement;
    ULONG   PhysicalPageSize;
    ULONG   NumberOfPhysicalPages;
    ULONG   LowestPhysicalPage;
    ULONG   HighestPhysicalPage;
    ULONG   AllocationGranularity;
    PVOID   LowestUserAddress;
    PVOID   HighestUserAddress;
    ULONG_PTR ActiveProcessors;
    UCHAR   NumberProcessors;
#ifdef X64
    ULONG   Unknown2; /* set to 0: probably just padding to 8-byte max field align */
#endif
} SYSTEM_BASIC_INFORMATION, *PSYSTEM_BASIC_INFORMATION;

/***************************************************************************
 * from SDK winuser.h on later platforms
 */

#ifndef SPI_GETWHEELSCROLLCHARS
# define SPI_GETWHEELSCROLLCHARS   0x006C
# define SPI_SETWHEELSCROLLCHARS   0x006D
#endif

#ifndef SPI_GETAUDIODESCRIPTION
# define SPI_GETAUDIODESCRIPTION   0x0074
# define SPI_SETAUDIODESCRIPTION   0x0075
#endif

#ifndef SPI_GETSCREENSAVESECURE
# define SPI_GETSCREENSAVESECURE   0x0076
# define SPI_SETSCREENSAVESECURE   0x0077
#endif

#ifndef SPI_GETHUNGAPPTIMEOUT
# define SPI_GETHUNGAPPTIMEOUT           0x0078
# define SPI_SETHUNGAPPTIMEOUT           0x0079
# define SPI_GETWAITTOKILLTIMEOUT        0x007A
# define SPI_SETWAITTOKILLTIMEOUT        0x007B
# define SPI_GETWAITTOKILLSERVICETIMEOUT 0x007C
# define SPI_SETWAITTOKILLSERVICETIMEOUT 0x007D
# define SPI_GETMOUSEDOCKTHRESHOLD       0x007E
# define SPI_SETMOUSEDOCKTHRESHOLD       0x007F
# define SPI_GETPENDOCKTHRESHOLD         0x0080
# define SPI_SETPENDOCKTHRESHOLD         0x0081
# define SPI_GETWINARRANGING             0x0082
# define SPI_SETWINARRANGING             0x0083
# define SPI_GETMOUSEDRAGOUTTHRESHOLD    0x0084
# define SPI_SETMOUSEDRAGOUTTHRESHOLD    0x0085
# define SPI_GETPENDRAGOUTTHRESHOLD      0x0086
# define SPI_SETPENDRAGOUTTHRESHOLD      0x0087
# define SPI_GETMOUSESIDEMOVETHRESHOLD   0x0088
# define SPI_SETMOUSESIDEMOVETHRESHOLD   0x0089
# define SPI_GETPENSIDEMOVETHRESHOLD     0x008A
# define SPI_SETPENSIDEMOVETHRESHOLD     0x008B
# define SPI_GETDRAGFROMMAXIMIZE         0x008C
# define SPI_SETDRAGFROMMAXIMIZE         0x008D
# define SPI_GETSNAPSIZING               0x008E
# define SPI_SETSNAPSIZING               0x008F
# define SPI_GETDOCKMOVING               0x0090
# define SPI_SETDOCKMOVING               0x0091
#endif

#ifndef SPI_GETDISABLEOVERLAPPEDCONTENT
# define SPI_GETDISABLEOVERLAPPEDCONTENT     0x1040
# define SPI_SETDISABLEOVERLAPPEDCONTENT     0x1041
# define SPI_GETCLIENTAREAANIMATION          0x1042
# define SPI_SETCLIENTAREAANIMATION          0x1043
# define SPI_GETCLEARTYPE                    0x1048
# define SPI_SETCLEARTYPE                    0x1049
# define SPI_GETSPEECHRECOGNITION            0x104A
# define SPI_SETSPEECHRECOGNITION            0x104B
#endif

#ifndef SPI_GETMINIMUMHITRADIUS
# define SPI_GETMINIMUMHITRADIUS             0x2014
# define SPI_SETMINIMUMHITRADIUS             0x2015
# define SPI_GETMESSAGEDURATION              0x2016
# define SPI_SETMESSAGEDURATION              0x2017
#endif

#endif /* _WINDEFS_H_ */
