/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was generated from a Windows DDK header to make
 ***   information necessary for userspace to call into the Windows
 ***   kernel available to Dr. Memory.  It contains only constants,
 ***   structures, and macros generated from the original header, and
 ***   thus, contains no copyrightable information.
 ***
 ****************************************************************************
 ****************************************************************************/

#ifndef _WDMDDK_
#define _WDMDDK_

/***************************************************************************
 * from DDK/WDK wdm.h
 */

//
// Define the file information class values
//
// WARNING:  The order of the following values are assumed by the I/O system.
//           Any changes made here should be reflected there as well.
//

typedef enum _FILE_INFORMATION_CLASS {
    FileDirectoryInformation         = 1,
    FileFullDirectoryInformation,   // 2
    FileBothDirectoryInformation,   // 3
    FileBasicInformation,           // 4
    FileStandardInformation,        // 5
    FileInternalInformation,        // 6
    FileEaInformation,              // 7
    FileAccessInformation,          // 8
    FileNameInformation,            // 9
    FileRenameInformation,          // 10
    FileLinkInformation,            // 11
    FileNamesInformation,           // 12
    FileDispositionInformation,     // 13
    FilePositionInformation,        // 14
    FileFullEaInformation,          // 15
    FileModeInformation,            // 16
    FileAlignmentInformation,       // 17
    FileAllInformation,             // 18
    FileAllocationInformation,      // 19
    FileEndOfFileInformation,       // 20
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
    FileValidDataLengthInformation, // 39
    FileShortNameInformation,       // 40
    FileIoCompletionNotificationInformation, // 41
    FileIoStatusBlockRangeInformation,       // 42
    FileIoPriorityHintInformation,           // 43
    FileSfioReserveInformation,              // 44
    FileSfioVolumeInformation,               // 45
    FileHardLinkInformation,                 // 46
    FileProcessIdsUsingFileInformation,      // 47
    FileNormalizedNameInformation,           // 48
    FileNetworkPhysicalNameInformation,      // 49
    FileIdGlobalTxDirectoryInformation,      // 50
    FileIsRemoteDeviceInformation,           // 51
    FileAttributeCacheInformation,           // 52
    FileNumaNodeInformation,                 // 53
    FileStandardLinkInformation,             // 54
    FileRemoteProtocolInformation,           // 55
    FileMaximumInformation
} FILE_INFORMATION_CLASS, *PFILE_INFORMATION_CLASS;

typedef struct _FILE_POSITION_INFORMATION {
    LARGE_INTEGER CurrentByteOffset;
} FILE_POSITION_INFORMATION, *PFILE_POSITION_INFORMATION;

//
// Support to set priority hints on a filehandle.
//

typedef enum _IO_PRIORITY_HINT {
    IoPriorityVeryLow = 0,          // Defragging, content indexing and other background I/Os
    IoPriorityLow,                  // Prefetching for applications.
    IoPriorityNormal,               // Normal I/Os
    IoPriorityHigh,                 // Used by filesystems for checkpoint I/O
    IoPriorityCritical,             // Used by memory manager. Not available for applications.
    MaxIoPriorityTypes
} IO_PRIORITY_HINT;

typedef struct _FILE_IO_PRIORITY_HINT_INFORMATION {
    IO_PRIORITY_HINT   PriorityHint;
} FILE_IO_PRIORITY_HINT_INFORMATION, *PFILE_IO_PRIORITY_HINT_INFORMATION;


typedef enum _KEY_INFORMATION_CLASS {
    KeyBasicInformation,
    KeyNodeInformation,
    KeyFullInformation,
    KeyNameInformation,
    KeyCachedInformation,
    KeyFlagsInformation,
    KeyVirtualizationInformation,
    KeyHandleTagsInformation,
    MaxKeyInfoClass  // MaxKeyInfoClass should always be the last enum
} KEY_INFORMATION_CLASS;

typedef enum _KEY_SET_INFORMATION_CLASS {
    KeyWriteTimeInformation,
    KeyWow64FlagsInformation,
    KeyControlFlagsInformation,
    KeySetVirtualizationInformation,
    KeySetDebugInformation,
    KeySetHandleTagsInformation,
    MaxKeySetInfoClass  // MaxKeySetInfoClass should always be the last enum
} KEY_SET_INFORMATION_CLASS;

typedef enum _IO_CONTAINER_INFORMATION_CLASS {
    IoSessionStateInformation, // 0 - Session State Information
    IoMaxContainerInformationClass
} IO_CONTAINER_INFORMATION_CLASS;

typedef enum _TRACE_INFORMATION_CLASS {
    TraceIdClass,
    TraceHandleClass,
    TraceEnableFlagsClass,
    TraceEnableLevelClass,
    GlobalLoggerHandleClass,
    EventLoggerHandleClass,
    AllLoggerHandlesClass,
    TraceHandleByNameClass,
    LoggerEventsLostClass,
    TraceSessionSettingsClass,
    LoggerEventsLoggedClass,
    MaxTraceInformationClass
} TRACE_INFORMATION_CLASS;

//
// Section Information Structures.
//

typedef enum _SECTION_INHERIT {
    ViewShare = 1,
    ViewUnmap = 2
} SECTION_INHERIT;


/***************************************************************************
 * Derived independently
 */

#if (_WIN32_WINNT < _WIN32_WINNT_WIN7) || (_MSC_VER < 1600)
typedef enum _POWER_REQUEST_TYPE {
    PowerRequestDisplayRequired,
    PowerRequestSystemRequired,
    PowerRequestAwayModeRequired
} POWER_REQUEST_TYPE, *PPOWER_REQUEST_TYPE;
#endif

#if (_MSC_VER < 1500)
typedef enum {
    SetPowerSettingValue = SystemPowerLoggingEntry + 1,
    NotifyUserPowerSetting,
    PowerInformationLevelUnused0,
    PowerInformationLevelUnused1,
    SystemVideoState,
    TraceApplicationPowerMessage,
    TraceApplicationPowerMessageEnd,
    ProcessorPerfStates,
    ProcessorIdleStates,
    ProcessorCap,
    SystemWakeSource,
    SystemHiberFileInformation,
    TraceServicePowerMessage,
    ProcessorLoad,
    PowerShutdownNotification,
    MonitorCapabilities,
#endif
#if (_MSC_VER == 1500)
typedef enum {
    MonitorCapabilities = PowerShutdownNotification + 1,
#endif
#if (_MSC_VER <= 1500)
    SessionPowerInit,
    SessionDisplayState,
    PowerRequestCreate,
    PowerRequestAction,
    GetPowerRequestList,
    ProcessorInformationEx,
    NotifyUserModeLegacyPowerEvent,
    GroupPark,
    ProcessorIdleDomains,
    WakeTimerList,
    SystemHiberFileSize,
    PowerInformationLevelMaximum
} POWER_INFORMATION_LEVEL;
#endif

#ifndef POWER_REQUEST_CONTEXT_SIMPLE_STRING
# define POWER_REQUEST_CONTEXT_SIMPLE_STRING    0x00000001
# define POWER_REQUEST_CONTEXT_DETAILED_STRING  0x00000002
#endif

/* Used by NtPowerInformation.PowerRequestCreate, which is used by
 * kernel32!PowerCreateRequest
 */
typedef struct _POWER_REQUEST_CREATE {
    /* First two fields match those in REASON_CONTEXT */
    ULONG Version;
    DWORD Flags;
    /* XXX: it seems that REASON_CONTEXT.Reason.Detailed.* is all just
     * ignored and only the name of the module is passed to the kernel
     * (name of exe, if NULL).  Why have the array then?  Where is the
     * resource ID passed?
     */
    UNICODE_STRING ReasonString;
} POWER_REQUEST_CREATE;

/* Used by NtPowerInformation.PowerRequestAction, which is used by
 * kernel32!Power{Set,Clear}Request
 */
typedef struct _POWER_REQUEST_ACTION {
    HANDLE PowerRequest;
    POWER_REQUEST_TYPE RequestType;
    BOOLEAN Unknown1;
    PVOID Unknown2;
} POWER_REQUEST_ACTION;


#endif /* _WDMDDK_ */
