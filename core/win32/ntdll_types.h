/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was generated from Windows NT/2000 Native API
 ***   Reference's ntdll.h as well as from DDK header files to make
 ***   information necessary for userspace to call into the Windows
 ***   kernel available to DynamoRIO.  It contains only constants,
 ***   structures, and macros generated from the original header, and
 ***   thus, contains no copyrightable information.
 ***
 ****************************************************************************
 ****************************************************************************/

/* ntdll types shared with ntdll_imports.c */

#ifndef _NTDLL_TYPES_H_
#define _NTDLL_TYPES_H_ 1

/***************************************************************************
 * structs and defines
 * mostly from either Windows NT/2000 Native API Reference's ntdll.h
 * or from the ddk's header files
 */

/* allow nameless struct/union */
#pragma warning(disable : 4201)

typedef unsigned int uint;
typedef LONG NTSTATUS;
/* make sure to cast to signed in case passed reg_t */
#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)
#define NT_ERROR(Status) ((((ULONG)(Status)) >> 30) == 3)
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001L)

typedef LONG KPRIORITY;

typedef struct _UNICODE_STRING {
    /* Length field is size in bytes not counting final 0 */
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
} UNICODE_STRING;
typedef UNICODE_STRING *PUNICODE_STRING;

typedef struct _STRING {
    USHORT Length;
    USHORT MaximumLength;
    PCHAR Buffer;
} ANSI_STRING;
typedef ANSI_STRING *PANSI_STRING;
typedef ANSI_STRING OEM_STRING;

typedef struct _OBJECT_ATTRIBUTES {
    ULONG Length;
    HANDLE RootDirectory;
    PUNICODE_STRING ObjectName;
    ULONG Attributes;
    PSECURITY_DESCRIPTOR SecurityDescriptor;
    PVOID SecurityQualityOfService; // Points to type SECURITY_QUALITY_OF_SERVICE
} OBJECT_ATTRIBUTES;
typedef OBJECT_ATTRIBUTES *POBJECT_ATTRIBUTES;

#define InitializeObjectAttributes(p, n, a, r, s) \
    {                                             \
        (p)->Length = sizeof(OBJECT_ATTRIBUTES);  \
        (p)->RootDirectory = r;                   \
        (p)->Attributes = a;                      \
        (p)->ObjectName = n;                      \
        (p)->SecurityDescriptor = s;              \
        (p)->SecurityQualityOfService = NULL;     \
    }

/* from DDK2003SP1/3790.1830/inc/wnet/ntdef.h */
#define OBJ_INHERIT 0x00000002L
#define OBJ_PERMANENT 0x00000010L
#define OBJ_EXCLUSIVE 0x00000020L
#define OBJ_CASE_INSENSITIVE 0x00000040L
#define OBJ_OPENIF 0x00000080L
#define OBJ_OPENLINK 0x00000100L
#define OBJ_KERNEL_HANDLE 0x00000200L      /* N.B.: invalid parameter on NT4! */
#define OBJ_FORCE_ACCESS_CHECK 0x00000400L /* N.B.: introduced with Win2003 */

typedef ULONG ACCESS_MASK;

/* from ntddk.h */
typedef enum _SECTION_INHERIT { ViewShare = 1, ViewUnmap = 2 } SECTION_INHERIT;

typedef struct _CLIENT_ID {
    /* These are numeric ids */
    HANDLE UniqueProcess;
    HANDLE UniqueThread;
} CLIENT_ID;
typedef CLIENT_ID *PCLIENT_ID;

typedef enum _MEMORY_INFORMATION_CLASS {
    MemoryBasicInformation,
    MemoryWorkingSetList,
    MemorySectionName,
    MemoryBasicVlmInformation
} MEMORY_INFORMATION_CLASS;

/* from DDK2003SP1/3790.1830/inc/ddk/wnet/ntddk.h */
typedef enum _PROCESSINFOCLASS {
    ProcessBasicInformation,
    ProcessQuotaLimits,
    ProcessIoCounters,
    ProcessVmCounters,
    ProcessTimes,
    ProcessBasePriority,
    ProcessRaisePriority,
    ProcessDebugPort,
    ProcessExceptionPort,
    ProcessAccessToken,
    ProcessLdtInformation,
    ProcessLdtSize,
    ProcessDefaultHardErrorMode,
    ProcessIoPortHandlers, // Note: this is kernel mode only
    ProcessPooledUsageAndLimits,
    ProcessWorkingSetWatch,
    ProcessUserModeIOPL,
    ProcessEnableAlignmentFaultFixup,
    ProcessPriorityClass,
    ProcessWx86Information,
    ProcessHandleCount,
    ProcessAffinityMask,
    ProcessPriorityBoost,
    ProcessDeviceMap,
    ProcessSessionInformation,
    ProcessForegroundInformation,
    ProcessWow64Information,
    /* added after XP+ */
    ProcessImageFileName,
    ProcessLUIDDeviceMapsEnabled,
    ProcessBreakOnTermination,
    ProcessDebugObjectHandle,
    ProcessDebugFlags,
    ProcessHandleTracing,
    ProcessIoPriority,
    ProcessExecuteFlags,
    ProcessTlsInformation, /* previously "ProcessResourceManagement"? */
    ProcessCookie,
    ProcessImageInformation,
    ProcessCycleTime,
    ProcessPagePriority,
    ProcessInstrumentationCallback,
    ProcessThreadStackAllocation,
    ProcessWorkingSetWatchEx,
    ProcessImageFileNameWin32,
    ProcessImageFileMapping,
    ProcessAffinityUpdateMode,
    ProcessMemoryAllocationMode,
    ProcessGroupInformation,
    ProcessTokenVirtualizationEnabled,
    ProcessConsoleHostProcess,
    ProcessWindowInformation,
    MaxProcessInfoClass // MaxProcessInfoClass should always be the last enum
} PROCESSINFOCLASS;

/* Thread Information Classes */
typedef enum _THREADINFOCLASS {
    ThreadBasicInformation,
    ThreadTimes,
    ThreadPriority,
    ThreadBasePriority,
    ThreadAffinityMask,
    ThreadImpersonationToken,
    ThreadDescriptorTableEntry,
    ThreadEnableAlignmentFaultFixup,
    ThreadEventPair_Reusable,
    ThreadQuerySetWin32StartAddress,
    ThreadZeroTlsCell,
    ThreadPerformanceCount,
    ThreadAmILastThread,
    ThreadIdealProcessor,
    ThreadPriorityBoost,
    ThreadSetTlsArrayAddress,
    ThreadIsIoPending,
    ThreadHideFromDebugger,
    MaxThreadInfoClass
} THREADINFOCLASS;

typedef struct _USER_STACK {
    PVOID FixedStackBase;
    PVOID FixedStackLimit;
    PVOID ExpandableStackBase;
    PVOID ExpandableStackLimit;
    PVOID ExpandableStackBottom;
} USER_STACK, *PUSER_STACK;

typedef enum _SECTION_INFORMATION_CLASS {
    SectionBasicInformation,
    SectionImageInformation
} SECTION_INFORMATION_CLASS;

typedef struct _SECTION_BASIC_INFORMATION { // Information Class 0
    PVOID BaseAddress;
    ULONG Attributes;
    LARGE_INTEGER Size;
} SECTION_BASIC_INFORMATION, *PSECTION_BASIC_INFORMATION;

typedef struct _SECTION_IMAGE_INFORMATION { // Information Class 1
    PVOID EntryPoint;
    ULONG Unknown1;
    ULONG StackReserve;
    ULONG StackCommit;
    ULONG Subsystem;
    USHORT MinorSubsystemVersion;
    USHORT MajorSubsystemVersion;
    ULONG Unknown2;
    ULONG Characteristics;
    USHORT ImageNumber;
    BOOLEAN Executable;
    UCHAR Unknown3;
    ULONG Unknown4[3];
} SECTION_IMAGE_INFORMATION, *PSECTION_IMAGE_INFORMATION;

typedef struct _IO_STATUS_BLOCK {
    union {
        NTSTATUS Status;
        PVOID Pointer;
    };
    ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

typedef enum _KPROFILE_SOURCE { ProfileTime } KPROFILE_SOURCE;

typedef enum _OBJECT_INFORMATION_CLASS {
    ObjectBasicInformation,
    ObjectNameInformation,
    ObjectTypeInformation,
    ObjectAllTypesInformation,
    ObjectHandleInformation /* can be queried and set */
} OBJECT_INFORMATION_CLASS;

typedef struct _OBJECT_BASIC_INFORMATION {
    ULONG Attributes;
    ACCESS_MASK GrantedAccess;
    ULONG HandleCount;
    ULONG PointerCount;
    ULONG PagePoolUsage;
    ULONG NonPagedPoolUsage;
    ULONG Reserved[3];
    ULONG NameInformationLength;
    ULONG TypeInformationLength;
    ULONG SecurityDescriptorLength;
    LARGE_INTEGER CreateTime;
} OBJECT_BASIC_INFORMATION, *POBJECT_BASIC_INFORMATION;

/* from DDK2003SP1/3790.1830/inc/ddk/wnet/ntddk.h */
/* Define the file information class values */
typedef enum _FILE_INFORMATION_CLASS {
    FileDirectoryInformation = 1,
    FileFullDirectoryInformation,   // 2
    FileBothDirectoryInformation,   // 3
    FileBasicInformation,           // 4  wdm
    FileStandardInformation,        // 5  wdm
    FileInternalInformation,        // 6
    FileEaInformation,              // 7
    FileAccessInformation,          // 8
    FileNameInformation,            // 9
    FileRenameInformation,          // 10
    FileLinkInformation,            // 11
    FileNamesInformation,           // 12
    FileDispositionInformation,     // 13
    FilePositionInformation,        // 14 wdm
    FileFullEaInformation,          // 15
    FileModeInformation,            // 16
    FileAlignmentInformation,       // 17
    FileAllInformation,             // 18
    FileAllocationInformation,      // 19
    FileEndOfFileInformation,       // 20 wdm
    FileAlternateNameInformation,   // 21
    FileStreamInformation,          // 22
    FilePipeInformation,            // 23
    FilePipeLocalInformation,       // 24
    FilePipeRemoteInformation,      // 25
    FileMailslotQueryInformation,   // 26
    FileMailslotSetInformation,     // 27
    FileCompressionInformation,     // 28
    FileObjectIdInformation,        // 29
    FileCompletionInformation,      // 30
    FileMoveClusterInformation,     // 31
    FileQuotaInformation,           // 32
    FileReparsePointInformation,    // 33
    FileNetworkOpenInformation,     // 34
    FileAttributeTagInformation,    // 35
    FileTrackingInformation,        // 36
    FileIdBothDirectoryInformation, // 37
    FileIdFullDirectoryInformation, // 38
    /* the following types introduced in XP and later */
    FileValidDataLengthInformation, // 39
    FileShortNameInformation,       // 40
    FileMaximumInformation
} FILE_INFORMATION_CLASS,
    *PFILE_INFORMATION_CLASS;

/* Information class structures returned by NtQueryInformationFile
 * and NtQueryAttributesFile
 */
typedef struct _FILE_BASIC_INFORMATION {
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    ULONG FileAttributes;
} FILE_BASIC_INFORMATION, *PFILE_BASIC_INFORMATION;

/* FileSystem types for nt_query_volume_info */
/* should be available in ntifs.h from IFS.
 * This version is from reactos/0.2.9/include/ndk/iotypes.h
 */
typedef enum _FS_INFORMATION_CLASS {
    FileFsVolumeInformation = 1,
    FileFsLabelInformation, /* not documented in IFS */
    FileFsSizeInformation,
    FileFsDeviceInformation,
    FileFsAttributeInformation,
    FileFsControlInformation,
    FileFsFullSizeInformation,
    FileFsObjectIdInformation,
    FileFsDriverPathInformation, /* in reactos */
    FileFsMaximumInformation
} FS_INFORMATION_CLASS,
    *PFS_INFORMATION_CLASS;

typedef struct _FILE_FS_SIZE_INFORMATION {
    LARGE_INTEGER TotalAllocationUnits;
    LARGE_INTEGER AvailableAllocationUnits;
    ULONG SectorsPerAllocationUnit;
    ULONG BytesPerSector;
} FILE_FS_SIZE_INFORMATION, *PFILE_FS_SIZE_INFORMATION;

typedef struct _FILE_FS_DEVICE_INFORMATION {
    ULONG /*DEVICE_TYPE*/ DeviceType;
    ULONG Characteristics;
} FILE_FS_DEVICE_INFORMATION, *PFILE_FS_DEVICE_INFORMATION;

typedef struct _FILE_FS_VOLUME_INFORMATION {
    LARGE_INTEGER VolumeCreationTime;
    ULONG VolumeSerialNumber;
    ULONG VolumeLabelLength;
    BOOLEAN SupportsObjects;
    WCHAR VolumeLabel[1];
} FILE_FS_VOLUME_INFORMATION, *PFILE_FS_VOLUME_INFORMATION;

/* FILE_FS_DEVICE_INFORMATION Characteristics */
#define FILE_REMOVABLE_MEDIA 0x00000001
#define FILE_READ_ONLY_DEVICE 0x00000002
#define FILE_FLOPPY_DISKETTE 0x00000004
#define FILE_WRITE_ONCE_MEDIA 0x00000008
#define FILE_REMOTE_DEVICE 0x00000010

typedef struct _FILE_FS_FULL_SIZE_INFORMATION {
    LARGE_INTEGER TotalAllocationUnits;
    LARGE_INTEGER CallerAvailableAllocationUnits;
    LARGE_INTEGER ActualAvailableAllocationUnits;
    ULONG SectorsPerAllocationUnit;
    ULONG BytesPerSector;
} FILE_FS_FULL_SIZE_INFORMATION, *PFILE_FS_FULL_SIZE_INFORMATION;

typedef struct _FILE_NETWORK_OPEN_INFORMATION {
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER AllocationSize;
    LARGE_INTEGER EndOfFile;
    ULONG FileAttributes;
} FILE_NETWORK_OPEN_INFORMATION, *PFILE_NETWORK_OPEN_INFORMATION;

typedef enum _KEY_VALUE_INFORMATION_CLASS {
    KeyValueBasicInformation,
    KeyValueFullInformation,
    KeyValuePartialInformation,
    KeyValueFullInformationAlign64,
    KeyValuePartialInformationAlign64
} KEY_VALUE_INFORMATION_CLASS;

typedef struct _KEY_VALUE_FULL_INFORMATION {
    ULONG TitleIndex;
    ULONG Type;
    ULONG DataOffset;
    ULONG DataLength;
    ULONG NameLength;
    WCHAR Name[1]; // Variable size
    //          Data[1]             // Variable size data not declared
} KEY_VALUE_FULL_INFORMATION, *PKEY_VALUE_FULL_INFORMATION;

typedef struct _KEY_VALUE_PARTIAL_INFORMATION {
    ULONG TitleIndex;
    ULONG Type;
    ULONG DataLength;
    UCHAR Data[1]; // Variable size
} KEY_VALUE_PARTIAL_INFORMATION, *PKEY_VALUE_PARTIAL_INFORMATION;

typedef enum _KEY_INFORMATION_CLASS {
    KeyBasicInformation,
    KeyNodeInformation,
    KeyFullInformation,
    KeyNameInformation
} KEY_INFORMATION_CLASS;

typedef struct _KEY_NAME_INFORMATION {
    ULONG NameLength;
    WCHAR Name[1]; // Variable size
} KEY_NAME_INFORMATION, *PKEY_NAME_INFORMATION;

typedef enum _SYSTEM_INFORMATION_CLASS {
    SystemBasicInformation = 0,
    SystemProcessorInformation = 1,
    SystemPerformanceInformation = 2,
    SystemTimeOfDayInformation = 3,
    SystemProcessesAndThreadsInformation = 5,
    SystemProcessorTimes = 8,
    SystemGlobalFlag = 9,
    SystemModuleInformation = 11,
    SystemLockInformation = 12,
    SystemHandleInformation = 16,
    SystemObjectInformation = 17
    /* many more, see Nebbett */
} SYSTEM_INFORMATION_CLASS;

/* Event types and functions */
typedef enum _EVENT_TYPE {
    /* manual-reset event - used for broadcasting to multiple waiting threads */
    NotificationEvent,
    /* automatically changes state to non-signaled after releasing a waiting thread */
    SynchronizationEvent
} EVENT_TYPE,
    *PEVENT_TYPE;

/* we don't actually use this but for cleanliness sake, is from ntddk.h */
typedef VOID(NTAPI *PIO_APC_ROUTINE)(IN PVOID ApcContext,
                                     IN PIO_STATUS_BLOCK IoStatusBlock,
                                     IN ULONG Reserved);

typedef VOID(NTAPI *PKNORMAL_ROUTINE)(IN PVOID NormalContext, IN PVOID SystemArgument1,
                                      IN PVOID SystemArgument2);

/* From http://undocumented.ntinternals.net/UserMode/Structures/
 * RTL_USER_PROCESS_PARAMETERS.html
 */

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

/* NtCreateNamedPipeFilePipe: TypeMessage parameter */
#define FILE_PIPE_BYTE_STREAM_TYPE 0x00000000
#define FILE_PIPE_MESSAGE_TYPE 0x00000001

/* NtCreateNamedPipeFilePipe: ReadmodeMessage parameter */
#define FILE_PIPE_BYTE_STREAM_MODE 0x00000000
#define FILE_PIPE_MESSAGE_MODE 0x00000001

/* NtCreateNamedPipeFilePipe: Nonblocking parameter */
#define FILE_PIPE_QUEUE_OPERATION 0x00000000
#define FILE_PIPE_COMPLETE_OPERATION 0x00000001

#define FILE_DEVICE_CONSOLE 0x00000050

/***************************************************************************
 * convenience enums
 */
typedef enum {
    MEMORY_RESERVE_ONLY = MEM_RESERVE,
    MEMORY_COMMIT = MEM_RESERVE | MEM_COMMIT
} memory_commit_status_t;

#endif /* _NTDLL_TYPES_H_ */
