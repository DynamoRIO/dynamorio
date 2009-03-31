/* **********************************************************
 * Copyright (c) 2008 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * 
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Code Manipulation API Sample:
 * stats.c
 *
 * Serves as an example of how to create custom statistics and export
 * them in shared memory (without using any Windows API libraries, and
 * thus remaining transparent).  These statistics can be viewed using
 * the provided statistics viewer.  This code also documents the
 * official shared memory layout required by the statistics viewer.
 */

#define _CRT_SECURE_NO_DEPRECATE 1

#include "dr_api.h"
#include <stddef.h> /* for offsetof */
#include <wchar.h> /* _snwprintf */

#ifndef WINDOWS
# error WINDOWS-only!
#endif

#ifdef WINDOWS
# define DISPLAY_STRING(msg) dr_messagebox(msg)
#else
# define DISPLAY_STRING(msg) dr_printf("%s\n", msg);
#endif

/* We export a set of stats in shared memory.
 * drgui.exe reads and displays them.
 */
const char *stat_names[] = {
    /* drgui.exe displays CLIENTSTAT_NAME_MAX_LEN chars for each name */
    "Instructions",
    "Floating point instrs",
    "System calls",
};

/* Be sure to prefix "Global\" to ensure this is visible across sessions,
 * except on NT where Global\ is not supported.
 */
#define CLIENT_SHMEM_KEY_NT_L L"DynamoRIO_Client_Statistics"
#define CLIENT_SHMEM_KEY_L    L"Global\\DynamoRIO_Client_Statistics"
#define CLIENTSTAT_NAME_MAX_LEN 50
#define NUM_STATS (sizeof(stat_names)/sizeof(char*))

/* Statistics are all 64-bit for x64.  At some point we'll add per-stat
 * typing, but for now we have identical types dependent on the platform.
 */
#ifdef X86_64
# error Statistics sample and graphical viewer are not yet supported for 64-bit
typedef int64 stats_int_t;
# define STAT_FORMAT_CODE UINT64_FORMAT_CODE
#else
typedef int stats_int_t;
# define STAT_FORMAT_CODE "d"
#endif

/* we allocate this struct in the shared memory: */
typedef struct _client_stats {
    uint num_stats;
    bool exited;
    uint pid;
    /* we need a copy of all the names here */
    char names[NUM_STATS][CLIENTSTAT_NAME_MAX_LEN];
    stats_int_t num_instrs;
    stats_int_t num_flops;
    stats_int_t num_syscalls;
} client_stats;

/* we directly increment the global counters using a lock prefix */
static client_stats *stats;

/***************************************************************************/
/* shared memory setup */

/* we have multiple shared memories: one that holds the count of
 * statistics instances, and then one per statistics struct */
static HANDLE shared_map_count;
static PVOID shared_view_count;
static int * shared_count;
static HANDLE shared_map;
static PVOID shared_view;
#define KEYNAME_MAXLEN 128
static wchar_t shared_keyname[KEYNAME_MAXLEN];

/* returns current contents of addr and replaces contents with value */
static __declspec(naked) /* no prolog/epilog */
uint
atomic_swap(void *addr, uint value)
{
    __asm {
       /* cl was using ebp even though naked so we hand-code param access */
        mov eax, dword ptr [esp+8] /* value */
        mov ecx, dword ptr [esp+4] /* addr */
        xchg (ecx), eax
        ret
    }
}

typedef LONG NTSTATUS;
#define NT_SUCCESS(Status) ((Status) >= 0)

#define NT_CURRENT_PROCESS ( (HANDLE) -1 )
#define NT_CURRENT_THREAD  ( (HANDLE) -2 )

/* from DDK2003SP1/3790.1830/inc/wnet/ntdef.h */
#define OBJ_CASE_INSENSITIVE    0x00000040L
#define OBJ_OPENIF              0x00000080L

typedef struct _UNICODE_STRING {
    /* Length field is size in bytes not counting final 0 */
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING;
typedef UNICODE_STRING *PUNICODE_STRING;

typedef struct _STRING {
  USHORT  Length;
  USHORT  MaximumLength;
  PCHAR  Buffer;
} ANSI_STRING;
typedef ANSI_STRING *PANSI_STRING;

typedef struct _OBJECT_ATTRIBUTES {
    ULONG Length;
    HANDLE RootDirectory;
    PUNICODE_STRING ObjectName;
    ULONG Attributes;
    PSECURITY_DESCRIPTOR SecurityDescriptor;
    PVOID SecurityQualityOfService;  // Points to type SECURITY_QUALITY_OF_SERVICE
} OBJECT_ATTRIBUTES;
typedef OBJECT_ATTRIBUTES *POBJECT_ATTRIBUTES;

#define InitializeObjectAttributes( p, n, a, r, s ) { \
    (p)->Length = sizeof( OBJECT_ATTRIBUTES );          \
    (p)->RootDirectory = r;                             \
    (p)->Attributes = a;                                \
    (p)->ObjectName = n;                                \
    (p)->SecurityDescriptor = s;                        \
    (p)->SecurityQualityOfService = NULL;               \
    }

/* from ntddk.h */
typedef enum _SECTION_INHERIT {
    ViewShare = 1,
    ViewUnmap = 2
} SECTION_INHERIT;

#define DIRECTORY_ALL_ACCESS (STANDARD_RIGHTS_REQUIRED | 0xF)
#define STATUS_OBJECT_NAME_EXISTS        ((NTSTATUS)0x40000000L)

#define GET_NTDLL(NtFunction, signature) NTSYSAPI NTSTATUS NTAPI NtFunction signature
GET_NTDLL(NtCreateSection, (OUT PHANDLE SectionHandle,
                            IN ACCESS_MASK DesiredAccess,
                            IN POBJECT_ATTRIBUTES ObjectAttributes,
                            IN PLARGE_INTEGER SectionSize OPTIONAL,
                            IN ULONG Protect,
                            IN ULONG Attributes,
                            IN HANDLE FileHandle));
GET_NTDLL(NtMapViewOfSection, (IN HANDLE           SectionHandle,
                               IN HANDLE           ProcessHandle,
                               IN OUT PVOID       *BaseAddress,
                               IN ULONG            ZeroBits,
                               IN SIZE_T           CommitSize,
                               IN OUT PLARGE_INTEGER  SectionOffset OPTIONAL,
                               IN OUT PSIZE_T      ViewSize,
                               IN SECTION_INHERIT  InheritDisposition,
                               IN ULONG            AllocationType,
                               IN ULONG            Protect));
GET_NTDLL(NtUnmapViewOfSection, (IN HANDLE         ProcessHandle,
                                 IN PVOID          BaseAddress));

GET_NTDLL(NtOpenDirectoryObject, (PHANDLE, ACCESS_MASK ,POBJECT_ATTRIBUTES));

GET_NTDLL(RtlInitUnicodeString, (IN OUT PUNICODE_STRING DestinationString,
                                 IN PCWSTR SourceString));

typedef struct _PEB {
    BOOLEAN InheritedAddressSpace;
    BOOLEAN ReadImageFileExecOptions;
    BOOLEAN BeingDebugged;
    BOOLEAN Spare;
    HANDLE Mutant;
    PVOID ImageBaseAddress; 
    PVOID LoaderData;
    PVOID ProcessParameters;
    PVOID SubSystemData;
    PVOID ProcessHeap;
    PVOID FastPebLock;
    PVOID FastPebLockRoutine;
    PVOID FastPebUnlockRoutine;
    ULONG EnvironmentUpdateCount;
    PVOID KernelCallbackTable;
    PVOID EventLogSection;
    PVOID EventLog;
    PVOID FreeList;
    ULONG TlsExpansionCounter;
    PVOID TlsBitmap;
    ULONG TlsBitmapBits[0x2];
    PVOID ReadOnlySharedMemoryBase;
    PVOID ReadOnlySharedMemoryHeap;
    PVOID ReadOnlyStaticServerData;
    PVOID AnsiCodePageData;
    PVOID OemCodePageData;
    PVOID UnicodeCaseTableData;
    ULONG NumberOfProcessors;
    ULONG NtGlobalFlag;
    BYTE Spare2[0x4];
    LARGE_INTEGER CriticalSectionTimeout;
    ULONG HeapSegmentReserve;
    ULONG HeapSegmentCommit; 
    ULONG HeapDeCommitTotalFreeThreshold;
    ULONG HeapDeCommitFreeBlockThreshold;
    ULONG NumberOfHeaps;
    ULONG MaximumNumberOfHeaps;
    PVOID *ProcessHeaps;
    PVOID GdiSharedHandleTable; 
    PVOID ProcessStarterHelper; 
    PVOID GdiDCAttributeList; 
    PRTL_CRITICAL_SECTION LoaderLock;
    ULONG OSMajorVersion;
    ULONG OSMinorVersion;
    ULONG OSBuildNumber;
    ULONG OSPlatformId;
    // ...
} PEB;

typedef struct _TEB {
    PVOID /* PEXCEPTION_REGISTRATION_RECORD */ ExceptionList;
    PVOID StackUserTop;  
    PVOID StackUserBase; 
    PVOID SubSystemTib;  
    ULONG FiberData;     
    PVOID ArbitraryUser; 
    struct _TEB *Self;   
    DWORD EnvironmentPtr;
    DWORD ProcessID;     
    DWORD ThreadID;      
    DWORD RpcHandle;     
    PVOID* TLSArray;     
    PEB* PEBPtr;       
    DWORD LastErrorValue;
    // ...
} TEB;

TEB *
get_TEB(void)
{
    return (TEB *) __readfsdword(offsetof(TEB, Self));
}

static uint
getpid(void)
{
    return get_TEB()->ProcessID;
}

static int
get_last_error(void)
{
    /* RtlGetLastWin32Error is only available on XP+? */
    return get_TEB()->LastErrorValue;
}

static void
set_last_error(int val)
{
    get_TEB()->LastErrorValue = val;
}

static NTSTATUS
wchar_to_unicode(PUNICODE_STRING dst, PCWSTR src)
{
    NTSTATUS res;
    res = RtlInitUnicodeString(dst, src);
    return res;
}

static bool
is_windows_NT(void)
{
    PEB *peb = get_TEB()->PEBPtr;
    DR_ASSERT(peb != NULL);
    return (peb->OSPlatformId == VER_PLATFORM_WIN32_NT && peb->OSMajorVersion == 4);
}

static HANDLE
get_BNO_handle(void)
{
    NTSTATUS res;
    HANDLE h;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING name_unicode;
    res = wchar_to_unicode(&name_unicode, L"\\BaseNamedObjects");
    DR_ASSERT(NT_SUCCESS(res));
    InitializeObjectAttributes(&oa, &name_unicode, OBJ_CASE_INSENSITIVE, NULL, NULL);
    res = NtOpenDirectoryObject(&h, DIRECTORY_ALL_ACCESS &
                                ~(DELETE | WRITE_DAC | WRITE_OWNER),
                                &oa);
    DR_ASSERT(NT_SUCCESS(res));
    return h;
}

static HANDLE
my_CreateFileMappingW(HANDLE file, SECURITY_ATTRIBUTES *attrb, 
                      uint prot, uint size_high, uint size_low, wchar_t *name)
{
    NTSTATUS res;
    HANDLE section;
    UNICODE_STRING section_name_unicode;
    OBJECT_ATTRIBUTES section_attributes;
    uint access;
    LARGE_INTEGER li_size;
    li_size.HighPart = size_high;
    li_size.LowPart = size_low;
    DR_ASSERT(name != NULL);
    res = wchar_to_unicode(&section_name_unicode, name);
    DR_ASSERT(NT_SUCCESS(res));
    if (!NT_SUCCESS(res))
        return NULL;
    if (file == INVALID_HANDLE_VALUE) {
        DR_ASSERT(size_high != 0 || size_low != 0);
        file = NULL;
    }
    /* we ignore dacl for now */
    InitializeObjectAttributes(&section_attributes, &section_name_unicode,
                               OBJ_CASE_INSENSITIVE | OBJ_OPENIF,
                               get_BNO_handle(), NULL);
    access = STANDARD_RIGHTS_REQUIRED | SECTION_QUERY | SECTION_MAP_READ;
    if (prot == PAGE_READWRITE)
        access |= (SECTION_MAP_WRITE | SECTION_MAP_READ);
    res = NtCreateSection(&section, access,
                          &section_attributes,
                          (size_high == 0 && size_low == 0) ? NULL /* entire file */
                          : &li_size,
                          prot, SEC_COMMIT, file);
    if (!NT_SUCCESS(res)) {
        dr_fprintf(STDERR, "Error in my_CreateFileMappingW: "PFX"\n", res);
        DR_ASSERT(false);
        return NULL;
    }
    if (res == STATUS_OBJECT_NAME_EXISTS) {
        set_last_error(ERROR_ALREADY_EXISTS);
    } else
        set_last_error(ERROR_SUCCESS);
    return section;
}

static void *
my_MapViewOfFile(HANDLE section, uint file_prot, uint offs_high, uint offs_low,
                 size_t size)
{
    NTSTATUS res;
    void *map = NULL;
    uint prot;
    LARGE_INTEGER li_offs;
    li_offs.HighPart = offs_high;
    li_offs.LowPart = offs_low;
    /* convert FILE_ flags to PAGE_ flags */
    if ((file_prot & FILE_MAP_WRITE) != 0)
        prot = PAGE_READWRITE;
    else if ((file_prot & FILE_MAP_READ) != 0)
        prot = PAGE_READONLY;
    else if ((file_prot & FILE_MAP_COPY) != 0)
        prot = PAGE_WRITECOPY;
    else
        prot = PAGE_NOACCESS;
    res = NtMapViewOfSection(section, NT_CURRENT_PROCESS, &map, 0,
                             0 /* no commit */, &li_offs, (PSIZE_T) &size, 
                             ViewUnmap, 0, prot);
    if (!NT_SUCCESS(res)) {
        dr_fprintf(STDERR, "Error in my_CreateFileMappingW: "PFX"\n", res);
        DR_ASSERT(false);
        return NULL;
    }
    return map;

}

static void
my_UnmapViewOfFile(HANDLE base)
{
    NTSTATUS res = NtUnmapViewOfSection(NT_CURRENT_PROCESS, base);
    DR_ASSERT(NT_SUCCESS(res));
}

static client_stats *
shared_memory_init(void)
{
    bool is_NT = is_windows_NT();
    int num;
    int pos;
    SECURITY_ATTRIBUTES attrb;
    SECURITY_DESCRIPTOR descrip;
    SECURITY_DESCRIPTOR_CONTROL sd_control = SE_DACL_PRESENT;
    attrb.nLength = sizeof(SECURITY_ATTRIBUTES);
    attrb.bInheritHandle = FALSE;
    /* Set security descriptor by hand (WIN32 API routines to do so are
     * in advapi).  This may not be fully forward compatible.
     */
    /* initialize the descriptor */
    memset(&descrip, 0, sizeof(SECURITY_DESCRIPTOR));
    /* Set the dacl present flag. DACL will be NULL from memset which is as 
     * desired since will give full acccess to everyone: security
     * is not a concern for these stats.
     */
    descrip.Control = sd_control;
    /* set revision appropriately */
    descrip.Revision = SECURITY_DESCRIPTOR_REVISION;
    attrb.lpSecurityDescriptor = &descrip;
    /* We do not want to rely on the registry.
     * Instead, a piece of shared memory with the key base name holds the
     * total number of stats instances.
     */
    shared_map_count =
        my_CreateFileMappingW(INVALID_HANDLE_VALUE, &attrb, 
                              PAGE_READWRITE, 0, sizeof(client_stats), 
                              is_NT ? CLIENT_SHMEM_KEY_NT_L : CLIENT_SHMEM_KEY_L);
    DR_ASSERT(shared_map_count != NULL);
    shared_view_count = 
        my_MapViewOfFile(shared_map_count, FILE_MAP_READ|FILE_MAP_WRITE, 0, 0, 0);
    DR_ASSERT(shared_view_count != NULL);
    shared_count = (int *) shared_view_count;
    /* ASSUMPTION: memory is initialized to 0!
     * otherwise our protocol won't work
     * it's hard to build a protocol to initialize it to 0 -- if you want
     * to add one, feel free, but make sure it's correct
     */
    do {
        pos = (int) atomic_swap(shared_count, (uint) -1);
        /* if get -1 back, someone else is looking at it */
    } while (pos == -1);
    /* now increment it */
    atomic_swap(shared_count, pos+1);

    num = 0;
    while (1) {
        _snwprintf(shared_keyname, KEYNAME_MAXLEN, L"%s.%03d",
                   is_NT ? CLIENT_SHMEM_KEY_NT_L : CLIENT_SHMEM_KEY_L, num);
        shared_map = my_CreateFileMappingW(INVALID_HANDLE_VALUE, &attrb, 
                                           PAGE_READWRITE, 0, 
                                           sizeof(client_stats), 
                                           shared_keyname);
        if (shared_map != NULL && get_last_error() == ERROR_ALREADY_EXISTS) { 
            dr_close_file(shared_map); 
            shared_map = NULL; 
        }
        if (shared_map != NULL)
            break;
        num++;
    }
    dr_log(NULL, LOG_ALL, 1, "Shared memory key is: \"%S\"\n", shared_keyname);
    dr_fprintf(STDERR, "Shared memory key is: \"%S\"\n", shared_keyname);
    shared_view = my_MapViewOfFile(shared_map, FILE_MAP_READ|FILE_MAP_WRITE,
                                   0, 0, 0);
    DR_ASSERT(shared_view != NULL);
    return (client_stats *) shared_view;
}

static void
shared_memory_exit(void)
{
    int pos; 
    stats->exited = true;
    /* close down statistics */
    my_UnmapViewOfFile(shared_view);
    dr_close_file(shared_map);
    /* decrement count, then unmap */
    do {
        pos = atomic_swap(shared_count, (uint) -1);
        /* if get -1 back, someone else is looking at it */
    } while (pos == -1);
    /* now increment it */
    atomic_swap(shared_count, pos-1);
    my_UnmapViewOfFile(shared_view_count);
    dr_close_file(shared_map_count);
}
/***************************************************************************/

static void event_exit(void);
static dr_emit_flags_t event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                                         bool for_trace, bool translating);

static client_id_t my_id;

DR_EXPORT void 
dr_init(client_id_t id)
{
    uint i;

    my_id = id;
    /* make it easy to tell, by looking at log file, which client executed */
    dr_log(NULL, LOG_ALL, 1, "Client 'stats' initializing\n");

    stats = shared_memory_init();
    memset(stats, 0, sizeof(stats));
    stats->num_stats = NUM_STATS;
    stats->pid = getpid();
    for (i=0; i<NUM_STATS; i++) {
        strncpy(stats->names[i], stat_names[i], CLIENTSTAT_NAME_MAX_LEN);
        stats->names[i][CLIENTSTAT_NAME_MAX_LEN-1] = '\0';
    }
    dr_register_bb_event(event_basic_block);
    dr_register_exit_event(event_exit);
}

#ifdef WINDOWS
# define DIRSEP '\\'
#else
# define DIRSEP '/'
#endif

static void 
event_exit(void)
{
    file_t f;
    /* display the results */
    char msg[512];
    char fname[512];
    char *dirsep;
    int len;
    len = dr_snprintf(msg, sizeof(msg)/sizeof(msg[0]),
                      "Instrumentation results:\n"
                      "  saw %" STAT_FORMAT_CODE " flops\n", stats->num_flops);
    DR_ASSERT(len > 0);
    msg[sizeof(msg)/sizeof(msg[0])-1] = '\0';
#ifdef SHOW_RESULTS
    DISPLAY_STRING(msg);
#endif /* SHOW_RESULTS */

    /* On Windows we need an absolute path so we place it in
     * the same directory as our library.
     */
    len = dr_snprintf(fname, sizeof(fname)/sizeof(fname[0]),
                      "%s", dr_get_client_path(my_id));
    DR_ASSERT(len > 0);
    for (dirsep = fname + len; *dirsep != DIRSEP; dirsep--)
        DR_ASSERT(dirsep > fname);
    len = dr_snprintf(dirsep + 1, (sizeof(fname) - (dirsep - fname))/sizeof(fname[0]),
                      "stats.%d.log", dr_get_process_id());
    DR_ASSERT(len > 0);
    fname[sizeof(fname)/sizeof(fname[0])-1] = '\0';
    f = dr_open_file(fname, DR_FILE_WRITE_OVERWRITE);
    DR_ASSERT(f != INVALID_FILE);
    dr_fprintf(f, "%s\n", msg);
    dr_close_file(f);

    shared_memory_exit();
}

static void
insert_inc(void *drcontext, instrlist_t *bb, instr_t *where,
           stats_int_t *addr, uint incby)
{
    instr_t *inc;
    opnd_t immed;
    /* FIXME: we're using stats_int_t, but our inc here does not support 64-bit */
    if (incby <= 0x7f)
        immed = OPND_CREATE_INT8(incby);
    else /* unlikely but possible */
        immed = OPND_CREATE_INT32(incby);
    inc = INSTR_CREATE_add(drcontext, OPND_CREATE_MEM32(REG_NULL, (int)addr), immed);
    /* make it thread-safe (only works if it doesn't straddle a cache line) */
    instr_set_prefix_flag(inc, PREFIX_LOCK);
    DR_ASSERT((((ptr_uint_t)addr) & 0x3) == 0); /* 4-aligned => single cache line */
    instrlist_meta_preinsert(bb, where, inc);
}

static dr_emit_flags_t 
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating)
{
    instr_t *instr;
    int op;
    int num_instrs = 0;
    int num_flops = 0;
    int num_syscalls = 0;
#ifdef VERBOSE
    dr_printf("in dr_basic_block(tag="PFX")\n", tag);
# ifdef VERBOSE_VERBOSE
    instrlist_disassemble(drcontext, tag, bb, STDOUT);
# endif
#endif
    /* count up # flops, then do single increment at end */
    for (instr = instrlist_first(bb); instr != NULL; instr = instr_get_next(instr)) {
        num_instrs++;
        op = instr_get_opcode(instr);
        if (op >= OP_fadd && op <= OP_fcomip) {
            /* FIXME: exclude loads and stores */
#ifdef VERBOSE
            dr_print_instr(drcontext, STDOUT, instr, "Found flop: ");
#endif
            num_flops++;
        }
        if (instr_is_syscall(instr)) {
            num_syscalls++;
        }
    }
    if (num_instrs > 0 || num_flops > 0 || num_syscalls > 0) {
        uint eflags;
        bool need_to_save = true;
        /* insert increment at start, for maximum prob. of not needing
         * to save flags
         */
        for (instr = instrlist_first(bb); instr != NULL;
             instr = instr_get_next(instr)) {
            eflags = instr_get_eflags(instr);
            /* could be more sophisticated and look beyond exit */
            if (instr_is_exit_cti(instr) || (eflags & EFLAGS_READ_6) != 0)
                break;
            if ((eflags & EFLAGS_WRITE_6) == EFLAGS_WRITE_6) {
                need_to_save = false;
                break;
            }
        }
        instr = instrlist_first(bb);
        if (need_to_save)
            dr_save_arith_flags(drcontext, bb, instr, SPILL_SLOT_1);
        if (num_instrs > 0)
            insert_inc(drcontext, bb, instr, &stats->num_instrs, num_instrs);
        if (num_flops > 0)
            insert_inc(drcontext, bb, instr, &stats->num_flops, num_flops);
        if (num_syscalls > 0)
            insert_inc(drcontext, bb, instr, &stats->num_syscalls, num_syscalls);
        if (need_to_save)
            dr_restore_arith_flags(drcontext, bb, instr, SPILL_SLOT_1);
    }
    return DR_EMIT_DEFAULT;
}
