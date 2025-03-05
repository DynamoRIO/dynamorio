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

#define ALPC_MESSAGE_SECURITY_ATTRIBUTE 0x80000000
#define ALPC_MESSAGE_VIEW_ATTRIBUTE     0x40000000
#define ALPC_MESSAGE_CONTEXT_ATTRIBUTE  0x20000000
#define ALPC_MESSAGE_HANDLE_ATTRIBUTE   0x10000000

typedef struct _ALPC_CONTEXT_ATTRIBUTES
{
    PVOID PortContext;
    PVOID MessageContext;
    ULONG SequenceNumber;
    ULONG MessageID;
    ULONG CallbackID;
} ALPC_CONTEXT_ATTRIBUTES, *PALPC_CONTEXT_ATTRIBUTES;

typedef struct _ALPC_DATA_VIEW
{
    ULONG Flags;
    HANDLE SectionHandle;
    PVOID ViewBase;
    SIZE_T ViewSize;
} ALPC_DATA_VIEW, *PALPC_DATA_VIEW;

typedef struct _ALPC_SECURITY_ATTRIBUTES
{
    ULONG Flags;
    PSECURITY_QUALITY_OF_SERVICE SecurityQos;
    HANDLE ContextHandle;
    ULONG Reserved1;
    ULONG Reserved2;
} ALPC_SECURITY_ATTRIBUTES, *PALPC_SECURITY_ATTRIBUTES;

typedef struct _ALPC_HANDLE_ATTRIBUTES
{
    ULONG Flags;
    HANDLE Handle;
    ULONG ObjectType;
    ACCESS_MASK DesiredAccess;
} ALPC_HANDLE_ATTRIBUTES, *PALPC_HANDLE_ATTRIBUTES;


/***************************************************************************
 * from pdb files
 */
typedef struct _ALPC_PORT_ATTRIBUTES
{
    ULONG Flags;
    SECURITY_QUALITY_OF_SERVICE SecurityQos;
    SIZE_T MaxMessageLength;
    SIZE_T MemoryBandwidth;
    SIZE_T MaxPoolUsage;
    SIZE_T MaxSectionSize;
    SIZE_T MaxViewSize;
    SIZE_T MaxTotalSectionSize;
    ULONG DupObjectTypes;
#ifdef X64
    ULONG Reserved;
#endif
} ALPC_PORT_ATTRIBUTES, *PALPC_PORT_ATTRIBUTES;

typedef struct _ALPC_MESSAGE_ATTRIBUTES
{
    ULONG AllocatedAttributes;
    ULONG ValidAttributes;
} ALPC_MESSAGE_ATTRIBUTES, *PALPC_MESSAGE_ATTRIBUTES;

typedef enum _ALPC_PORT_INFORMATION_CLASS
{
    AlpcBasicInformation,
    AlpcPortInformation,
    AlpcAssociateCompletionPortInformation,
    AlpcConnectedSIDInformation,
    AlpcServerInformation,
    AlpcMessageZoneInformation,
    AlpcRegisterCompletionListInformation,
    AlpcUnregisterCompletionListInformation,
    AlpcAdjustCompletionListConcurrencyCountInformation,
    AlpcRegisterCallbackInformation,
    AlpcCompletionListRundownInformation
} ALPC_PORT_INFORMATION_CLASS;

typedef enum _ALPC_MESSAGE_INFORMATION_CLASS
{
    AlpcMessageSidInformation,
    AlpcMessageTokenModifiedIdInformation
} ALPC_MESSAGE_INFORMATION_CLASS;
