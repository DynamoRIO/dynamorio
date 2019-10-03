/* **********************************************************
 * Copyright (c) 2011-2019 Google, Inc.  All rights reserved.
 * Copyright (c) 2004-2007 VMware, Inc.  All rights reserved.
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

#ifdef X64
#    error 64-bit not yet supported (issue #118)
#endif

#include <windows.h>
#include <stdio.h>
#include <assert.h>

#define RC1_HACK 1

static int verbose = 1;

/* Avoid flushing issues on Windows, and keep warnings and info messages
 * all in the same order, by always going to STDERR
 */
#define PRINT(...) fprintf(stderr, __VA_ARGS__)

#define INFO(level, ...)          \
    do {                          \
        if (verbose >= (level)) { \
            PRINT(__VA_ARGS__);   \
        }                         \
    } while (0)

#define WARN(...) INFO(0, __VA_ARGS__)

/* alignment helpers */
#define ALIGNED(x, alignment) ((((uint)x) & ((alignment)-1)) == 0)
#define ALIGN_FORWARD(x, alignment) ((((uint)x) + ((alignment)-1)) & (~((alignment)-1)))
#define ALIGN_BACKWARD(x, alignment) (((uint)x) & (~((alignment)-1)))
#define PAD(length, alignment) (ALIGN_FORWARD((length), (alignment)) - (length))

/* check if all bits in mask are set in var */
#define TESTALL(mask, var) (((mask) & (var)) == (mask))
/* check if any bit in mask is set in var */
#define TESTANY(mask, var) (((mask) & (var)) != 0)
/* check if a single bit is set in var */
#define TEST TESTANY

#define BUFFER_SIZE_BYTES(buf) sizeof(buf)
#define BUFFER_SIZE_ELEMENTS(buf) (BUFFER_SIZE_BYTES(buf) / sizeof(buf[0]))
#define BUFFER_LAST_ELEMENT(buf) buf[BUFFER_SIZE_ELEMENTS(buf) - 1]
#define NULL_TERMINATE_BUFFER(buf) BUFFER_LAST_ELEMENT(buf) = 0

typedef int bool;
typedef unsigned int uint;
typedef LONG NTSTATUS;
#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)

#define false FALSE
#define true TRUE
#define PAGE_SIZE 0x1000
#define ALLOCATION_GRANULARITY 0x10000
#define INVALID_File (NULL)

#define GET_NTDLL(NtFunction, signature) NTSYSAPI NTSTATUS NTAPI NtFunction signature

typedef struct _UNICODE_STRING {
    /* Length field is size in bytes not counting final 0 */
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
} UNICODE_STRING;
typedef UNICODE_STRING *PUNICODE_STRING;

typedef struct _OBJECT_ATTRIBUTES {
    ULONG Length;
    HANDLE RootDirectory;
    PUNICODE_STRING ObjectName;
    ULONG Attributes;
    PVOID SecurityDescriptor;       // Points to type SECURITY_DESCRIPTOR
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

#define OBJ_CASE_INSENSITIVE 0x00000040L
#define OBJ_KERNEL_HANDLE 0x00000200L
typedef ULONG ACCESS_MASK;

#ifndef X64
#    ifndef _W64
#        define _W64
#    endif
typedef _W64 long LONG_PTR, *PLONG_PTR;
typedef _W64 unsigned long ULONG_PTR, *PULONG_PTR;
#endif
typedef LONG KPRIORITY;
typedef ULONG_PTR KAFFINITY;

typedef struct _PROCESS_BASIC_INFORMATION {
    NTSTATUS ExitStatus;
    char *PebBaseAddress;
    ULONG_PTR AffinityMask;
    KPRIORITY BasePriority;
    ULONG_PTR UniqueProcessId;
    ULONG_PTR InheritedFromUniqueProcessId;
} PROCESS_BASIC_INFORMATION;
typedef PROCESS_BASIC_INFORMATION *PPROCESS_BASIC_INFORMATION;

typedef struct _CLIENT_ID {
    HANDLE UniqueProcess;
    HANDLE UniqueThread;
} CLIENT_ID;
typedef CLIENT_ID *PCLIENT_ID;

typedef struct _THREAD_BASIC_INFORMATION { // Information Class 0
    NTSTATUS ExitStatus;
    PNT_TIB TebBaseAddress;
    CLIENT_ID ClientId;
    KAFFINITY AffinityMask;
    KPRIORITY Priority;
    KPRIORITY BasePriority;
} THREAD_BASIC_INFORMATION, *PTHREAD_BASIC_INFORMATION;

typedef enum {
    MEMORY_RESERVE_ONLY = MEM_RESERVE,
    MEMORY_COMMIT = MEM_RESERVE | MEM_COMMIT
} memory_commit_status_t;

typedef struct _IO_STATUS_BLOCK {
    NTSTATUS Status;
    // PVOID Pointer - Reserved, supposedly in a union with Status. see MSDN
    ULONG Information; // MSDN says it is ULONG_PTR
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

typedef struct _USER_STACK {
    PVOID FixedStackBase;
    PVOID FixedStackLimit;
    PVOID ExpandableStackBase;
    PVOID ExpandableStackLimit;
    PVOID ExpandableStackBottom;
} USER_STACK, *PUSER_STACK;

#define FILE_SYNCHRONOUS_IO_NONALERT 0x00000020

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
    MaxProcessInfoClass
} PROCESSINFOCLASS;

typedef struct _DESCRIPTOR_TABLE_ENTRY {
    ULONG Selector;
    LDT_ENTRY Descriptor;
} DESCRIPTOR_TABLE_ENTRY, *PDESCRIPTOR_TABLE_ENTRY;

/************************************************************************/
static bool
nt_remote_allocate_virtual_memory(HANDLE process, void **base, uint size, uint prot,
                                  memory_commit_status_t commit)
{
    NTSTATUS res;
    GET_NTDLL(NtAllocateVirtualMemory,
              (IN HANDLE ProcessHandle, IN OUT PVOID * BaseAddress, IN ULONG ZeroBits,
               IN OUT PULONG AllocationSize, IN ULONG AllocationType, IN ULONG Protect));
    uint sz = size;
    assert(ALIGNED(*base, PAGE_SIZE));
    res = NtAllocateVirtualMemory(process, base, 0 /* zero bits */, &sz, commit, prot);
    assert(sz >= size);
    return NT_SUCCESS(res);
}

static bool
query_thread_info(HANDLE h, THREAD_BASIC_INFORMATION *info)
{
    NTSTATUS res;
    GET_NTDLL(NtQueryInformationThread,
              (IN HANDLE ThreadHandle, IN THREADINFOCLASS ThreadInformationClass,
               OUT PVOID ThreadInformation, IN ULONG ThreadInformationLength,
               OUT PULONG ReturnLength OPTIONAL));
    int got;
    memset(info, 0, sizeof(THREAD_BASIC_INFORMATION));
    res = NtQueryInformationThread(h, ThreadBasicInformation, info,
                                   sizeof(THREAD_BASIC_INFORMATION), &got);
    assert(!NT_SUCCESS(res) || got == sizeof(THREAD_BASIC_INFORMATION));
    return NT_SUCCESS(res);
}

static bool
query_process_info(HANDLE h, PROCESS_BASIC_INFORMATION *info)
{
    NTSTATUS res;
    GET_NTDLL(NtQueryInformationProcess,
              (IN HANDLE ProcessHandle, IN PROCESSINFOCLASS ProcessInformationClass,
               OUT PVOID ProcessInformation, IN ULONG ProcessInformationLength,
               OUT PULONG ReturnLength OPTIONAL));
    int got;
    memset(info, 0, sizeof(PROCESS_BASIC_INFORMATION));
    res = NtQueryInformationProcess(h, ProcessBasicInformation, info,
                                    sizeof(PROCESS_BASIC_INFORMATION), &got);
    assert(!NT_SUCCESS(res) || got == sizeof(PROCESS_BASIC_INFORMATION));
    return NT_SUCCESS(res);
}

static bool
set_win32_start_addr(HANDLE h, void **start_addr)
{
    NTSTATUS res;
    GET_NTDLL(NtSetInformationThread,
              (IN HANDLE ThreadHandle, IN THREADINFOCLASS ThreadInfoClass,
               IN PVOID ThreadInformation, IN ULONG ThreadInformationLength));
    res = NtSetInformationThread(h, ThreadQuerySetWin32StartAddress, start_addr,
                                 sizeof(*start_addr));
    if (!NT_SUCCESS(res)) {
        INFO(1, "setting thread start addr failed with 0x%08x\n", res);
    }
    return NT_SUCCESS(res);
}

static bool
wchar_to_unicode(PUNICODE_STRING dst, PCWSTR src)
{
    NTSTATUS res;
    GET_NTDLL(RtlInitUnicodeString,
              (IN OUT PUNICODE_STRING DestinationString, IN PCWSTR SourceString));
    res = RtlInitUnicodeString(dst, src);
    return NT_SUCCESS(res);
}

static uint
remove_writecopy(uint prot)
{
    uint other = prot & (PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE);
    prot &= ~(PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE);

    switch (prot) {
    case PAGE_READONLY:
    case PAGE_READWRITE:
    case PAGE_EXECUTE:
    case PAGE_EXECUTE_READ:
    case PAGE_EXECUTE_READWRITE: break;
    case PAGE_WRITECOPY: prot = PAGE_READWRITE; break;
    case PAGE_EXECUTE_WRITECOPY: prot = PAGE_EXECUTE_READWRITE; break;
    }
    return (prot | other);
}

static bool
prot_is_readable(uint prot)
{
    prot &= ~(PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE);

    switch (prot) {
    case PAGE_READONLY:
    case PAGE_READWRITE:
    case PAGE_WRITECOPY:
    case PAGE_EXECUTE:
    case PAGE_EXECUTE_READ:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY: return true;
    }
    return false;
}

static bool
prot_is_writable(uint prot)
{
    prot &= ~(PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE);
    return (prot == PAGE_READWRITE || prot == PAGE_WRITECOPY ||
            prot == PAGE_EXECUTE_READWRITE || prot == PAGE_EXECUTE_WRITECOPY);
}

static char *
mem_state_string(DWORD state)
{
    switch (state) {
    case 0: return "none";
    case MEM_COMMIT: return "COMMIT";
    case MEM_FREE: return "FREE";
    case MEM_RESERVE: return "RESERVE";
    }
    return "<error>";
}

static char *
mem_type_string(DWORD type)
{
    switch (type) {
    case 0: return "none";
    case MEM_IMAGE: return "IMAGE";
    case MEM_MAPPED: return "MAPPED";
    case MEM_PRIVATE: return "PRIVATE";
    }
    return "<error>";
}

static char *
prot_string(DWORD prot)
{
    DWORD ignore_extras = prot & ~(PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE);
    switch (ignore_extras) {
    case PAGE_NOACCESS: return "----";
    case PAGE_READONLY: return "r---";
    case PAGE_READWRITE: return "rw--";
    case PAGE_WRITECOPY: return "rw-c";
    case PAGE_EXECUTE: return "--x-";
    case PAGE_EXECUTE_READ: return "r-x-";
    case PAGE_EXECUTE_READWRITE: return "rwx-";
    case PAGE_EXECUTE_WRITECOPY: return "rwxc";
    }
    return "<error>";
}

void
dump_mbi(MEMORY_BASIC_INFORMATION *mbi)
{
    PRINT("BaseAddress:       %p\n"
          "AllocationBase:    %p\n"
          "AllocationProtect: %08x %s\n"
          "RegionSize:        %08x\n"
          "State:             %08x %s\n"
          "Protect:           %08x %s\n"
          "Type:              %08x %s\n",
          mbi->BaseAddress, mbi->AllocationBase, mbi->AllocationProtect,
          prot_string(mbi->AllocationProtect), mbi->RegionSize, mbi->State,
          mem_state_string(mbi->State), mbi->Protect, prot_string(mbi->Protect),
          mbi->Type, mem_type_string(mbi->Type));
}

/********************************************************************************/

typedef struct _mem_map {
    char *old;
    char *new;
} mem_map;

static int map_count = 0;
static mem_map map[4097];
static int pending_map_count = 0;
static char *pending_map[4097];

static void
add_mapped_addr(char *old, char *new)
{
    assert(map_count < BUFFER_SIZE_ELEMENTS(map));
    map[map_count].new = new;
    map[map_count].old = old;
    map_count++;
}

static void
add_pending_mapped_addr(char *addr)
{
    assert(pending_map_count < BUFFER_SIZE_ELEMENTS(map));
    pending_map[pending_map_count++] = addr;
}

static bool
is_pending_mapped_addr(char *addr)
{
    int i;
    for (i = 0; i < pending_map_count; i++) {
        if (pending_map[i] == addr)
            return true;
    }
    return false;
}

/* we can get NULL from a bad TEB so use this instead */
#define FAIL ((char *)-1)

static char *
get_mapped_addr(char *old)
{
    int i;
    for (i = 0; i < map_count; i++) {
        if (map[i].old == old)
            return map[i].new;
    }
    return FAIL;
}

static char *
get_original_addr(char *new)
{
    int i;
    for (i = 0; i < map_count; i++) {
        if (map[i].new == new)
            return map[i].old;
    }
    return FAIL;
}

/* NOTE - sum users implicitly assume this is exactly 1 page */
static char buf[4096];

/* Note, because of my sloppy file format, type may or may not have a
 * description string, so we skip it and just eat the line, we can't do
 * anything with the type information anyways */
static bool
read_mbi(FILE *file, MEMORY_BASIC_INFORMATION *mbi)
{
    int res;
    char *resc;
    char dummy_buf[128];
    memset(mbi, 0, sizeof(MEMORY_BASIC_INFORMATION));
    res = fscanf(file,
                 "\nBaseAddress=%p\nAllocationBase=%p\n"
                 "AllocationProtect=0x%08x %*s\nRegionSize=0x%08x\n"
                 "State=0x%08x %*s\nProtect=0x%08x %*s\n",
                 &(mbi->BaseAddress), &(mbi->AllocationBase), &(mbi->AllocationProtect),
                 &(mbi->RegionSize), &(mbi->State), &(mbi->Protect));
    if (res != 6)
        return false;
    /* eat the type line */
    resc = fgets(dummy_buf, sizeof(dummy_buf), file);
    assert(resc != NULL);
    return true;
}

static void
print_descriptor(DESCRIPTOR_TABLE_ENTRY *entry)
{
    /* IA-32 SDM Volume 3A, section 3.4.5, "Segment Descriptors".
     * The segment type is determined by the S bit (0=system,
     * 1=code or data) and the 4-bit type field:
     */
    static const char *types[] = { "Data RO             ", "Data RO, acc        ",
                                   "Data R/W,           ", "Data R/W, acc       ",
                                   "Data RO, down       ", "Data RO, down, acc  ",
                                   "Data R/W, down      ", "Data R/W, down, acc ",
                                   "Code EO             ", "Code EO, acc        ",
                                   "Code E/R            ", "Code E/R, acc       ",
                                   "Code EO, conf       ", "Code EO, conf, acc  ",
                                   "Code E/RO, conf     ", "Code E/RO, conf, acc" };

    LDT_ENTRY *descr = &entry->Descriptor;

    uint base = (descr->BaseLow | (descr->HighWord.Bits.BaseMid << 16) |
                 (descr->HighWord.Bits.BaseHi << 24));

    uint limit = descr->LimitLow | (descr->HighWord.Bits.LimitHi << 16);
    if (descr->HighWord.Bits.Granularity == 1) {
        limit = limit << 12 | 0xfff;
    }

    PRINT("\t%04x ", entry->Selector);
    PRINT("%08x ", base);
    PRINT("%08x ", limit);

    /* The definition for LDT_ENTRY combines the S and type fields
     * into a five bit field. */
    if (TEST(0x10, descr->HighWord.Bits.Type)) {
        PRINT("%s ", types[descr->HighWord.Bits.Type & 0xf]);
    } else {
        PRINT("System               ");
    }

    PRINT(" %x  ", descr->HighWord.Bits.Dpl);
    PRINT(" %x  ", descr->HighWord.Bits.Default_Big);
    PRINT("%s  ", descr->HighWord.Bits.Granularity == 1 ? "4kb" : " 1b");
    PRINT(" %x   ", descr->HighWord.Bits.Pres);
    PRINT("%x ", descr->HighWord.Bits.Reserved_0);
    PRINT(" %x\n", descr->HighWord.Bits.Sys);
}

static void
print_descriptors(DESCRIPTOR_TABLE_ENTRY entries[6])
{
    int i;
    PRINT("\n\tSel    Base    Limit            Type        Dpl D/B Gran Pres L Sys\n"
          "\t---- -------- -------- -------------------- --- --- ---- ---- - ---\n");

    for (i = 0; i < 6; i++) {
        /* Intel SDM vol. 3A, section 3.4.2: the first entry in the LDT
         * is null and not used by the processor */
        if (entries[i].Selector != 0) {
            print_descriptor(&entries[i]);
        }
    }
    PRINT("\n");
}

static void
insert_entry(uint selector, uint word1, uint word2, DESCRIPTOR_TABLE_ENTRY entries[6])
{
    int i;

    /* Intel SDM vol. 3A, section 3.4.2: the first entry in the LDT
     * is null and not used by the processor */
    if (selector != 0) {
        for (i = 0; i < 6; i++) {
            /* don't insert duplicates */
            if (entries[i].Selector == selector)
                return;

            /* 0 indicates empty slot in entries[] */
            if (entries[i].Selector == 0) {
                entries[i].Selector = selector;
                *((PULONG)&entries[i].Descriptor) = word1;
                *((PULONG)&entries[i].Descriptor.HighWord) = word2;
                return;
            }
        }
    }
}

/* macro to read descriptor info and insert it in a table */
#define read_descriptor_and_insert(name, reg)                                       \
    {                                                                               \
        fgets(line, sizeof(line), file);                                            \
        res = sscanf(line, name "=0x%04x (0x%08x 0x%08x)\n", &reg, &word1, &word2); \
        assert(res == 3 || res == 1);                                               \
        if (res == 3)                                                               \
            insert_entry(reg, word1, word2, entries);                               \
    }

static void
read_threads(FILE *file, bool create, HANDLE hProc)
{
    HANDLE hThread;
    DWORD thread_id, new_id, res;
    THREAD_BASIC_INFORMATION thread_info;
    CLIENT_ID cid;
    OBJECT_ATTRIBUTES oa;
    USER_STACK stack = { 0 };
    CONTEXT cxt;
    GET_NTDLL(NtCreateThread,
              (OUT PHANDLE ThreadHandle, IN ACCESS_MASK DesiredAccess,
               IN POBJECT_ATTRIBUTES ObjectAttributes, IN HANDLE ProcessHandle,
               OUT PCLIENT_ID ClientId, IN PCONTEXT ThreadContext,
               IN PUSER_STACK UserStack, IN BOOLEAN CreateSuspended));

    if (create) {
        cxt.ContextFlags = CONTEXT_INTEGER | CONTEXT_CONTROL;
        /* initialize, unsafe but should be ok right? */
        GetThreadContext(GetCurrentThread(), &cxt);
        InitializeObjectAttributes(&oa, NULL, OBJ_CASE_INSENSITIVE, NULL, NULL);
    }

    while (fscanf(file, "Thread=0x%08x\n", &thread_id)) {
        bool valid_state = true, valid_handle_state = false;
        uint handle_rights;
        char *teb;
        bool valid_selectors = false;
        uint Cs, Ss, Ds, Es, Fs, Gs;
        DESCRIPTOR_TABLE_ENTRY entries[6] = {
            0,
        };
        uint word1, word2;
        char line[64];
        long pos;
        void *win32_start_addr = 0;

        res = fscanf(file, "TEB=%p\n", &teb);
        assert(res == 1);

        res = fscanf(file, "HandleRights=0x%08x\n", &handle_rights);
        if (res == 1) {
            valid_handle_state = true;
        }

        res = fscanf(file, "Eax=0x%08x, Ebx=0x%08x, Ecx=0x%08x, Edx=0x%08x\n", &cxt.Eax,
                     &cxt.Ebx, &cxt.Ecx, &cxt.Edx);
        if (res == 4) {
            res = fscanf(file, "Esi=0x%08x, Edi=0x%08x, Esp=0x%08x, Ebp=0x%08x\n",
                         &cxt.Esi, &cxt.Edi, &cxt.Esp, &cxt.Ebp);
            assert(res == 4);
            res = fscanf(file, "EFlags=0x%08x, Eip=0x%08x\n\n", &cxt.EFlags, &cxt.Eip);
            assert(res == 2);

            /* look for segment registers and associated descriptors */
            pos = ftell(file);
            fgets(line, sizeof(line), file);
            res = sscanf(line, "Cs=0x%04x (0x%08x 0x%08x)\n", &Cs, &word1, &word2);
            if (res == 3 || res == 1) {
                valid_selectors = true;
                if (res == 3)
                    insert_entry(Cs, word1, word2, entries);

                read_descriptor_and_insert("Ss", Ss);
                read_descriptor_and_insert("Ds", Ds);
                read_descriptor_and_insert("Es", Es);
                read_descriptor_and_insert("Fs", Fs);
                read_descriptor_and_insert("Gs", Gs);
            } else {
                /* put file position back a line */
                fseek(file, pos, SEEK_SET);
            }

            /* look for the win32 start address */
            pos = ftell(file);
            res = fscanf(file, "Win32StartAddr=%p\n", &win32_start_addr);
            if (res != 1) {
                /* set start addr to 0 so we know right away if the thread
                 * doesn't have a start address in the ldmp.
                 */
                win32_start_addr = NULL;

                /* put file position back a line */
                fseek(file, pos, SEEK_SET);
            }
        } else {
            valid_state = false;
            fgets(buf, sizeof(buf), file);
            /* only show error when creating to avoid duplicate messages */
            if (create) {
                WARN("\nError reading thread state for original thread tid=0x%04x\n",
                     thread_id);
                WARN("%s", buf);
            }
            fgets(buf, sizeof(buf), file); /* read in the newline */
            /* clear context integer registers */
            cxt.Eax = 0;
            cxt.Ebx = 0;
            cxt.Ecx = 0;
            cxt.Edx = 0;
            cxt.EFlags = 0;
            cxt.Edi = 0;
            cxt.Esi = 0;
            cxt.Esp = 0;
            cxt.Ebp = 0;
            cxt.Eip = 0;
        }

        if (create) {
            res = NtCreateThread(&hThread, THREAD_ALL_ACCESS, &oa, hProc, &cid, &cxt,
                                 &stack, TRUE);
            assert(NT_SUCCESS(res));
            res = set_win32_start_addr(hThread, &win32_start_addr);
            if (!res)
                WARN("unable to set thread start address to %p\n", win32_start_addr);
            res = query_thread_info(hThread, &thread_info);
            assert(res);
            new_id = (uint)thread_info.ClientId.UniqueThread;
            INFO(1,
                 "created thread tid=0x%04x with TEB=%p original tid=0x%04x with "
                 "TEB=%p\n",
                 new_id, thread_info.TebBaseAddress, thread_id, teb);
            if (valid_selectors) {
                INFO(1, "\tcs=%04x ss=%04x ds=%04x es=%04x fs=%04x gs=%04x\n", Cs, Ss, Ds,
                     Es, Fs, Gs);
            }
            if (entries[0].Selector != 0) {
                print_descriptors(entries);
            }
            if (valid_handle_state) {
                INFO(1, "\tHandleRights=0x%08x\n", handle_rights);
            }
            if (teb == NULL) {
                WARN("\twill be unable to copy over TEB\n");
            } else {
                add_mapped_addr(teb, (char *)thread_info.TebBaseAddress);
            }
            if (!valid_state)
                WARN("\tnew thread's register state is invalid\n\n");
            CloseHandle(hThread);
        } else {
            /* prevent this region from being copied over till we know
             * where to put it */
            add_pending_mapped_addr(teb);
        }
    }
}

static HANDLE
create_process(char *path)
{
    HANDLE hProc, hSection, hFile;
    UNICODE_STRING uexe;
    OBJECT_ATTRIBUTES oa;
    IO_STATUS_BLOCK iosb;
    NTSTATUS res;
    wchar_t abs_path[MAX_PATH];
    wchar_t wpath[MAX_PATH];
    GET_NTDLL(NtCreateProcess,
              (OUT PHANDLE ProcessHandle, IN ACCESS_MASK DesiredAccess,
               IN POBJECT_ATTRIBUTES ObjectAttributes, IN HANDLE InheritFromProcessHandle,
               IN BOOLEAN InheritHandles, IN HANDLE SectionHandle OPTIONAL,
               IN HANDLE DebugPort OPTIONAL, IN HANDLE ExceptionPort OPTIONAL));
    GET_NTDLL(NtOpenFile,
              (OUT PHANDLE FileHandle, IN ACCESS_MASK DesiredAccess,
               IN POBJECT_ATTRIBUTES ObjectAttributes, OUT PIO_STATUS_BLOCK IoStatusBlock,
               IN ULONG ShareAccess, IN ULONG OpenOptions));
    GET_NTDLL(NtCreateSection,
              (OUT PHANDLE SectionHandle, IN ACCESS_MASK DesiredAccess,
               IN POBJECT_ATTRIBUTES ObjectAttributes,
               IN PLARGE_INTEGER SectionSize OPTIONAL, IN ULONG Protect,
               IN ULONG Attributes, IN HANDLE FileHandle));

    /* convert to absolute path which is required for create_process() */
    _snwprintf(wpath, BUFFER_SIZE_ELEMENTS(wpath), L"%hs", path);
    NULL_TERMINATE_BUFFER(wpath);
    GetFullPathName(wpath, BUFFER_SIZE_ELEMENTS(abs_path), abs_path, NULL);
    NULL_TERMINATE_BUFFER(abs_path);
    /* create dummy process */
    _snwprintf(wpath, BUFFER_SIZE_ELEMENTS(wpath), L"\\??\\%s", abs_path);
    NULL_TERMINATE_BUFFER(wpath);
    INFO(2, "dummy exe path = %ls\n", wpath);
    wchar_to_unicode(&uexe, wpath);
    InitializeObjectAttributes(&oa, &uexe, OBJ_CASE_INSENSITIVE, NULL, NULL);
    if (!NT_SUCCESS(NtOpenFile(&hFile, FILE_EXECUTE | SYNCHRONIZE, &oa, &iosb,
                               FILE_SHARE_READ, FILE_SYNCHRONOUS_IO_NONALERT))) {
        WARN("failed to open dummy process exe file\n");
        exit(0);
    }
    oa.ObjectName = 0;
    res = NtCreateSection(&hSection, SECTION_ALL_ACCESS, &oa, 0, PAGE_EXECUTE, SEC_IMAGE,
                          hFile);
    if (!NT_SUCCESS(res)) {
        WARN("failed to create section with error=0x%08x\n", res);
    }
    res = NtCreateProcess(&hProc, PROCESS_ALL_ACCESS, &oa, GetCurrentProcess(), true,
                          hSection, 0, 0);
    if (!NT_SUCCESS(res)) {
        WARN("failed to create dummy process with error=0x%08x\n", res);
        exit(0);
    }
    return hProc;
}

static void *highest_address_copied = NULL;
static bool reached_vsyscall_page = false;

/* if just_mapped then only copies over mapped regions, otherwise copies over
 * non-mapped regions only */
static void
copy_memory(FILE *file, bool just_mapped, HANDLE hProc)
{
    MEMORY_BASIC_INFORMATION mbi;
    DWORD res;
    fpos_t pos;
    bool do_copy;
    while (true) {
        res = fgetpos(file, &pos);
        assert(res == 0);
        res = read_mbi(file, &mbi);
        if (!res)
            break;
        if (mbi.State != MEM_FREE) {
            char *allocation_base = (char *)mbi.AllocationBase;
            uint allocation_protect = mbi.AllocationProtect;
            uint allocation_size = 0;
            char *target;
            fpos_t last_mbi_pos; /* for unknown TEB backing out */

            /* we can't handle write copy flag! remove it */
            allocation_protect = remove_writecopy(allocation_protect);
            INFO(2, "allocation base = %p, protect = 0x%08x\n", allocation_base,
                 allocation_protect);

            do {
                allocation_size += mbi.RegionSize;
                /* scan past data */
                if (prot_is_readable(mbi.Protect)) {
                    /* FIXME : hack, rc1 core doesn't check for guard page so
                     * sometimes memory is copied and sometimes it isn't if
                     * is guard! (why?), post rc1 should check
                     * MEM_COMMIT && !guard && is_readable */
                    assert(mbi.State == MEM_COMMIT);
                    if (!TEST(PAGE_GUARD, mbi.Protect)) {
                        res = fseek(file, mbi.RegionSize, SEEK_CUR);
                        assert(res == 0);
                    } else {
#if RC1_HACK
                        fpos_t hack_pos;
                        MEMORY_BASIC_INFORMATION hack_mbi;
                        res = fgetpos(file, &hack_pos);
                        assert(res == 0);
                        if (!read_mbi(file, &hack_mbi)) {
                            res = fsetpos(file, &hack_pos);
                            assert(res == 0);
                            res = fseek(file, mbi.RegionSize, SEEK_CUR);
                            assert(res == 0);
                        } else {
                            res = fsetpos(file, &hack_pos);
                            assert(res == 0);
                        }
#endif
                    }
                }
                res = fgetpos(file, &last_mbi_pos);
                assert(res == 0);
            } while (read_mbi(file, &mbi) && mbi.State != MEM_FREE &&
                     mbi.AllocationBase == allocation_base);

            /* see if we should copy over this allocation */
            target = get_mapped_addr(allocation_base);
            do_copy = (just_mapped && target != FAIL) ||
                (!just_mapped && target == FAIL &&
                 !is_pending_mapped_addr(allocation_base));
            if (!do_copy) {
                res = fsetpos(file, &last_mbi_pos);
                assert(res == 0);
                continue;
            }

            /* get allocation */
            if (target == FAIL) {
                if (!ALIGNED(allocation_base, ALLOCATION_GRANULARITY)) {
                    /* probably a TEB for a thread that wasn't in the all
                     * threads list at time of ldmp */
                    WARN("Probable TEB for unknown thread region (or x64 PEB/TEB?) "
                         "addr %p size 0x%08x\n",
                         allocation_base, allocation_size);
                }
                target = allocation_base;
                /* FIXME : why can't we use VirtualAllocEx? it fails with
                 * code 487 == INVALID_ADDRESS */
                res = nt_remote_allocate_virtual_memory(
                    hProc, &target, allocation_size, allocation_protect, MEMORY_COMMIT);
                if (!res) {
                    target = NULL;
                    res = nt_remote_allocate_virtual_memory(
                        hProc, &target, allocation_size, allocation_protect,
                        MEMORY_COMMIT);
                }
                if (!res) {
                    WARN("ERROR: unable to allocate memory at %p size 0x%08x, "
                         "SKIPPING\n",
                         allocation_base, allocation_size);
                    res = fsetpos(file, &last_mbi_pos);
                    assert(res == 0);
                    continue;
                }

                assert(target != NULL);
                if (target != allocation_base) {
                    WARN("ERROR: unable to allocate memory at %p size 0x%08x\n\t "
                         "will be copied to %p instead\n",
                         allocation_base, allocation_size, target);
                }
                INFO(2, "target = %p, base = %p\n", target, allocation_base);
            }
            INFO(2, "size=0x%08x\n", allocation_size);
            INFO(2, "target=%p\n", target);

            /* reset file pointer */
            res = fsetpos(file, &pos);
            assert(res == 0);

            /* walk memory */
#define TARGET_ADDR (target + ((char *)mbi.BaseAddress - allocation_base))
            res = fgetpos(file, &pos);
            assert(res == 0);
            while (read_mbi(file, &mbi) && mbi.State != MEM_FREE &&
                   mbi.AllocationBase == allocation_base) {
                if ((uint)mbi.BaseAddress > (uint)highest_address_copied)
                    highest_address_copied = mbi.BaseAddress;
                if (mbi.State == MEM_RESERVE) {
                    res = VirtualFreeEx(hProc, TARGET_ADDR, mbi.RegionSize, MEM_DECOMMIT);
                    if (!res && TARGET_ADDR == (char *)0x7ffe1000) {
                        WARN("unable to make post vsyscall/shared user data page "
                             "0x7ffe1000 reserve, skipping\n");
                        /* in case break out, save position */
                        res = fgetpos(file, &pos);
                        assert(res == 0);
                        continue;
                    }
                    assert(res);
                } else {
                    uint old_prot;
                    assert(mbi.State = MEM_COMMIT);
                    if (!TEST(PAGE_GUARD, mbi.Protect) && prot_is_readable(mbi.Protect)) {
                        uint i;
                        /* copy over memory */
                        res = VirtualProtectEx(hProc, TARGET_ADDR, mbi.RegionSize,
                                               PAGE_READWRITE, &old_prot);
                        if (!res && TARGET_ADDR == (char *)0x7ffe0000) {
                            WARN("unable to copy over vsyscall/shared user data page "
                                 "0x7ffe0000, skipping\n");
                            reached_vsyscall_page = true;
                            res = fseek(file, mbi.RegionSize, SEEK_CUR);
                            assert(res == 0);
                            /* in case break out, save position */
                            res = fgetpos(file, &pos);
                            assert(res == 0);
                            continue;
                        }
                        assert(res);
                        assert(mbi.RegionSize % sizeof(buf) == 0);
                        for (i = 0; i < mbi.RegionSize; i += sizeof(buf)) {
                            uint written;
                            res = fread(buf, 1, sizeof(buf), file);
                            assert(res == sizeof(buf));
                            res = WriteProcessMemory(hProc, TARGET_ADDR + i, buf,
                                                     sizeof(buf), &written);
                            assert(res && written == sizeof(buf));
                        }
                    } else {
#if RC1_HACK
                        /* FIXME : rc1 hack, might have memory from guard page
                         * somehow */
                        fpos_t hack_pos;
                        MEMORY_BASIC_INFORMATION hack_mbi;
                        res = fgetpos(file, &hack_pos);
                        assert(res == 0);
                        if (!read_mbi(file, &hack_mbi)) {
                            res = fsetpos(file, &hack_pos);
                            assert(res == 0);
                            res = fseek(file, mbi.RegionSize, SEEK_CUR);
                            assert(res == 0);
                        } else {
                            res = fsetpos(file, &hack_pos);
                            assert(res == 0);
                        }
#endif
                    }
                    res = VirtualProtectEx(hProc, TARGET_ADDR, mbi.RegionSize,
                                           remove_writecopy(mbi.Protect), &old_prot);
                    assert(res);
                }
                /* in case break out, save position */
                res = fgetpos(file, &pos);
                assert(res == 0);
            }
            /* roll back last mbi */
            res = fsetpos(file, &pos);
            assert(res == 0);
#undef TARGET_ADDR
        }
    }
}

static void
usage(const char *msg)
{
    PRINT("%s\nusage: ldmp [-verbose <N>] <.ldmp file> <dummy executable>\n", msg);
    PRINT("example: bin32/ldmp logs/hello.exe.5124.0000000.ldmp bin32/dummy.exe\n");
    exit(-1);
}

/* creates a debuggable process that matches (roughly) the process that was
 * used to generate the .ldmp file, prints out a mapping of thread_ids from
 * the dump to the new process.
 * ldmp.exe <.ldmp file> <windows path to dummy.exe (absolute, local drive)> */
DWORD __cdecl main(DWORD argc, char *argv[], char *envp[])
{
    FILE *file;
    HANDLE hProc;
    PROCESS_BASIC_INFORMATION info;
    MEMORY_BASIC_INFORMATION mbi;
    char *pb;
    void *drbase = 0;

    DWORD res;
    fpos_t thread_start_pos;
    DWORD i;

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-verbose") == 0) {
                if (i >= argc - 1)
                    usage("-verbose takes an integer");
                verbose = atoi(argv[++i]);
            } else
                usage("unknown option");
        } else
            break;
    }
    if (i >= argc - 1)
        usage("missing arguments");
    INFO(1, "opening ldump file %s\n", argv[i]);
    file = fopen(argv[i], "rb");
    if (file == NULL) {
        WARN("unable to find file %s\n", argv[i]);
        exit(-1);
    }

    hProc = create_process(argv[i + 1]);

    if (GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtCreateThreadEx")) {
        /* XXX i#397: on Vista in the past we saw NtCreateThread fail
         * with STATUS_INVALID_IMAGE_FORMAT, but it seems to be
         * working now on win7 so we'll stick with it rather than
         * cobbling together an approach that uses NtCreateThreadEx.
         */
        WARN("ldmp.exe may not work fully on Vista+ (i#397)\n");
    }

    /* read peb (& message if there is one) */
    res = fscanf(file, "PEB=%p\n", &pb);
    if (res != 1) {
        uint length = 0;
        /* newer format, starts with a message prefaced with number of bytes
         * in message */
        /* case 8912: don't read a newline here as it will gobble the next newline
         * if the msg is empty; instead we add 1 to the length and read the newline
         * w/ the msg.
         */
        res = fscanf(file, "0x%08x", &length);
        assert(res == 1);
        length = length + 1; /* newline after length, see comment above */
        /* FIXME - should never happen, but we could handle by doing this in
         * pieces */
        assert(length < sizeof(buf));
        if (length >= sizeof(buf))
            length = sizeof(buf) - 1;
        res = fread(buf, 1, length, file);
        assert(res == length);
        buf[length] = '\0';
        /* Remember that buf has a newline as its first char */
        INFO(1,
             "\n**************************************************\n"
             "Message:\n%s\n**************************************************\n",
             buf);
        res = fscanf(file, "PEB=%p\n", &pb);
    }
    assert(res == 1);
    res = query_process_info(hProc, &info);
    assert(res);
    INFO(1, "\ncreated dummy process pid=%d with PEB=%p original PEB=%p\n",
         info.UniqueProcessId, info.PebBaseAddress, pb);

    /* read dynamorio.dll base, present only in format after case 5366 */
    if (fscanf(file, "dynamorio.dll=%p\n", &drbase) == 1) {
        /* for custom scripting */
        INFO(1, "\ndynamorio.dll=%p\n", drbase);
        /* or more likely */
        INFO(1,
             "\nRun this command, or attach non-invasively from an existing windbg:\n"
             "windbg -pv -p %d -c '.reload dynamorio.dll=%p'\n\n",
             info.UniqueProcessId, drbase);
    }

    add_mapped_addr(pb, (char *)info.PebBaseAddress);
    /* add shared user data (win2k) / syscall page (xp/03) to map list */
    add_mapped_addr((char *)0x7ffe0000, (char *)0x7ffe0000);

    /* iterate through threads without creating to build up pending
     * teb mappings */
    res = fgetpos(file, &thread_start_pos);
    assert(res == 0);
    read_threads(file, false, hProc);

    INFO(1, "\n");
    /* free memory in dummy process */
    pb = NULL;
    while (VirtualQueryEx(hProc, pb, &mbi, sizeof(mbi)) == sizeof(mbi)) {
        if (mbi.State == MEM_FREE || get_original_addr(mbi.AllocationBase) != FAIL) {
            if ((uint)mbi.BaseAddress + mbi.RegionSize < (uint)mbi.BaseAddress)
                break;
            pb = (char *)mbi.BaseAddress + mbi.RegionSize;
        } else {
            if (!VirtualFreeEx(hProc, mbi.AllocationBase, 0, MEM_RELEASE)) {
                /* try an unmap */
                GET_NTDLL(NtUnmapViewOfSection,
                          (IN HANDLE ProcessHandle, IN PVOID BaseAddress));
                if (!NT_SUCCESS(NtUnmapViewOfSection(hProc, mbi.AllocationBase))) {
                    if (get_original_addr(mbi.AllocationBase) == FAIL) {
                        /* this happens in wow64 w/ the x64 PEB */
                        WARN("Unable to free memory region (x64 PEB?):\n");
                        dump_mbi(&mbi);
                    }
                    pb += mbi.RegionSize;
                } else {
                    INFO(2, "unmapped allocation at %p\n", mbi.AllocationBase);
                }
            } else {
                INFO(2, "freed memory at %p\n", mbi.AllocationBase);
            }
        }
    }
    INFO(1, "finished freeing memory, starting copy over\n");

    /* chomp any error line (such as all threads list freed) */
    if (fscanf(file, "<%512[a-zA-Z1-9 ]>", &buf))
        WARN("%s\n", buf);

    /* copy over directly corresponding (non-mapped) memory regions */
    copy_memory(file, false, hProc);

    /* now create threads (can't do this before since we have no control over
     * where the teb's are placed and they might conflict with one of the
     * copied over regions above */
    res = fsetpos(file, &thread_start_pos);
    assert(res == 0);
    read_threads(file, true, hProc);
    INFO(1, "\n");

    /* now copy over mapped allocations */
    /* first chomp off any error line, we already printed it once above */
    fscanf(file, "<%512[a-zA-Z1-9 ]>", &buf);
    copy_memory(file, true, hProc);

    if (!reached_vsyscall_page) {
        WARN("ERROR: failed to reach shared_user_data/vsyscall page, ldmp likely "
             "truncated.\n"
             "       Memory above %p is likely unavailable or incorrect.\n\n",
             highest_address_copied);
    }

    INFO(1, "finished\n");
    CloseHandle(hProc);
    return 0;
}
