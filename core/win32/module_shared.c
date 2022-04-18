/* **********************************************************
 * Copyright (c) 2010-2021 Google, Inc.  All rights reserved.
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

/*
 * module_shared.c
 * Windows DLL routines that are shared between the core, preinject, and
 * drmarker-using code like libutil.
 * It's a pain to link module.c with non-core targets like preinject,
 * so we split these routines out here.
 * Note that NOT_DYNAMORIO_CORE_PROPER still links ntdll.c, while
 * NOT_DYNAMORIO_CORE (i.e., libutil) does not (since it is a pain
 * to link both ntdll.lib and a libc.lib).
 */

#include <stdio.h> /* sscanf */

#include "configure.h"
#if defined(NOT_DYNAMORIO_CORE)
#    define ASSERT(x)
#    define ASSERT_NOT_REACHED()
#    define ASSERT_NOT_IMPLEMENTED(x)
#    define DODEBUG(x)
#    define DOCHECK(n, x)
#    define DECLARE_NEVERPROT_VAR(var, ...) var = __VA_ARGS__;
#    define ALIGN_BACKWARD(x, alignment) (((ULONG_PTR)x) & (~((alignment)-1)))
#    define PAGE_SIZE 4096
#else
/* we include globals.h mainly for ASSERT, even though we're
 * used by preinject.
 * preinject just defines its own d_r_internal_error!
 */
#    include "../globals.h"
#    include "os_private.h"
#    if !defined(NOT_DYNAMORIO_CORE_PROPER)
#        include "../module_shared.h" /* for is_in_code_section() */
#    endif
#    include "instrument.h" /* dr_lookup_module_by_name */
#endif

#include "ntdll.h"

/* We have to hack away things we use here that won't work for non-core */
#if defined(NOT_DYNAMORIO_CORE_PROPER) || defined(NOT_DYNAMORIO_CORE)
#    undef strcasecmp
#    define strcasecmp _stricmp
#    define wcscasecmp _wcsicmp
#    undef TRY_EXCEPT_ALLOW_NO_DCONTEXT
#    define TRY_EXCEPT_ALLOW_NO_DCONTEXT(dc, try, except) try
#    undef ASSERT_OWN_NO_LOCKS
#    define ASSERT_OWN_NO_LOCKS() /* who cares if not the core */
#    undef ASSERT_CURIOSITY
#    define ASSERT_CURIOSITY(x) /* who cares if not the core */
/* since not including os_shared.h (this is impl in ntdll.c): */
bool
is_readable_without_exception(const byte *pc, size_t size);
/* allow converting functions to and from data pointers */
#    pragma warning(disable : 4055)
#    pragma warning(disable : 4054)
#    define convert_data_to_function(func) ((generic_func_t)(func))
#    undef LOG       /* remove preinject's LOG */
#    define LOG(...) /* nothing */
#endif

#define MAX_FUNCNAME_SIZE 128

#if !defined(NOT_DYNAMORIO_CORE) && !defined(NOT_DYNAMORIO_CORE_PROPER)
#    include "instrument.h"
typedef struct _pe_symbol_export_iterator_t {
    dr_symbol_export_t info;

    byte *mod_base;
    size_t mod_size;
    IMAGE_EXPORT_DIRECTORY *exports;
    size_t exports_size;
    PULONG functions; /* array of RVAs */
    PUSHORT ordinals;
    PULONG fnames; /* array of RVAs */
    uint idx;
    bool hasnext; /* set to false on error or end */
} pe_symbol_export_iterator_t;
#endif

/* This routine was moved here from os.c since we need it for
 * get_proc_address_64 (via get_module_exports_directory_*()) for preinject
 * and drmarker, neither of which link os.c.
 */
/* is_readable_without_exception checks to see that all bytes with addresses
 * from pc to pc+size-1 are readable and that reading from there won't
 * generate an exception.  this is a stronger check than
 * !not_readable() below.
 * FIXME : beware of multi-thread races, just because this returns true,
 * doesn't mean another thread can't make the region unreadable between the
 * check here and the actual read later.  See d_r_safe_read() as an alt.
 */
/* throw-away buffer */
DECLARE_NEVERPROT_VAR(static char is_readable_buf[4 /*efficient read*/], { 0 });
bool
is_readable_without_exception(const byte *pc, size_t size)
{
    /* Case 7967: NtReadVirtualMemory is significantly faster than
     * NtQueryVirtualMemory (probably even for large regions where NtQuery can
     * walk by mbi.RegionSize but we have to walk by page size).  We don't care
     * if multiple threads write into the buffer at once.  Nearly all of our
     * calls ask about areas smaller than a page.
     */
    byte *check_pc = (byte *)ALIGN_BACKWARD(pc, PAGE_SIZE);
    if (size > (size_t)((byte *)POINTER_MAX - pc))
        size = (byte *)POINTER_MAX - pc;
    do {
        size_t bytes_read = 0;
#if defined(NOT_DYNAMORIO_CORE)
        if (!ReadProcessMemory(NT_CURRENT_PROCESS, check_pc, is_readable_buf,
                               sizeof(is_readable_buf), (SIZE_T *)&bytes_read) ||
            bytes_read != sizeof(is_readable_buf)) {
#else
        if (!nt_read_virtual_memory(NT_CURRENT_PROCESS, check_pc, is_readable_buf,
                                    sizeof(is_readable_buf), &bytes_read) ||
            bytes_read != sizeof(is_readable_buf)) {
#endif
            return false;
        }
        check_pc += PAGE_SIZE;
    } while (check_pc != 0 /*overflow*/ && check_pc < pc + size);
    return true;
}

#if defined(WINDOWS) && !defined(NOT_DYNAMORIO_CORE)
/* Image entry point is stored at,
 * PEB->DOS_HEADER->NT_HEADER->OptionalHeader.AddressOfEntryPoint.
 * Handles both 32-bit and 64-bit remote processes.
 */
uint64
get_remote_process_entry(HANDLE process_handle, OUT bool *x86_code)
{
    uint64 peb_base;
    /* Handle the two possible widths of peb.ImageBaseAddress: */
    union {
        uint64 dos64;
        uint dos32;
    } dos_ptr;
    IMAGE_DOS_HEADER dos;
    IMAGE_NT_HEADERS nt;
    bool res;
    size_t nbytes;
    bool peb_is_32 = IF_X64_ELSE(false, is_32bit_process(process_handle));
    /* Read peb.ImageBaseAddress. */
    peb_base = get_peb_maybe64(process_handle);
    res = read_remote_memory_maybe64(
        process_handle,
        peb_base + (peb_is_32 ? X86_IMAGE_BASE_PEB_OFFSET : X64_IMAGE_BASE_PEB_OFFSET),
        &dos_ptr, sizeof(dos_ptr), &nbytes);
    if (!res || nbytes != sizeof(dos_ptr))
        return 0;
    uint64 dos_base = peb_is_32 ? dos_ptr.dos32 : dos_ptr.dos64;
    res =
        read_remote_memory_maybe64(process_handle, dos_base, &dos, sizeof(dos), &nbytes);
    if (!res || nbytes != sizeof(dos))
        return 0;
    res = read_remote_memory_maybe64(process_handle, dos_base + dos.e_lfanew, &nt,
                                     sizeof(nt), &nbytes);
    if (!res || nbytes != sizeof(nt))
        return 0;
    /* IMAGE_NT_HEADERS.FileHeader == IMAGE_NT_HEADERS64.FileHeader */
    *x86_code = nt.FileHeader.Machine == IMAGE_FILE_MACHINE_I386;
    ASSERT(BOOLS_MATCH(is_32bit_process(process_handle), *x86_code));
    return dos_base + (size_t)OPT_HDR(&nt, AddressOfEntryPoint);
}
#endif

/* returns NULL if exports directory doesn't exist
 * if exports_size != NULL returns also exports section size
 * assumes base_addr is a safe is_readable_pe_base()
 *
 * NOTE - only verifies readability of the IMAGE_EXPORT_DIRECTORY, does not verify target
 * readability of any RVAs it contains (for that use get_module_exports_directory_check
 * or verify in the caller at usage). Xref case 9717.
 */
static IMAGE_EXPORT_DIRECTORY *
get_module_exports_directory_common(app_pc base_addr,
                                    size_t *exports_size /* OPTIONAL OUT */
                                        _IF_NOT_X64(bool ldr64))
{
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base_addr;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base_addr + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY *expdir;
    ASSERT(dos->e_magic == IMAGE_DOS_SIGNATURE);
    ASSERT(nt != NULL && nt->Signature == IMAGE_NT_SIGNATURE);
#ifndef X64
    if (ldr64) {
        expdir = ((IMAGE_OPTIONAL_HEADER64 *)&(nt->OptionalHeader))->DataDirectory +
            IMAGE_DIRECTORY_ENTRY_EXPORT;
    } else
#endif
        expdir = OPT_HDR(nt, DataDirectory) + IMAGE_DIRECTORY_ENTRY_EXPORT;

        /* avoid preinject link issues: we don't have is_readable_pe_base */
#if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)
    /* callers should have done this in release builds */
    ASSERT(is_readable_pe_base(base_addr));
#endif

    /* RVA conversions are trivial only for MEM_IMAGE */
    DODEBUG({
        MEMORY_BASIC_INFORMATION mbi;
        size_t len = query_virtual_memory(base_addr, &mbi, sizeof(mbi));
        ASSERT(len == sizeof(mbi));
        /* We do see MEM_MAPPED PE files: case 7947 */
        if (mbi.Type != MEM_IMAGE) {
            LOG(THREAD_GET, LOG_SYMBOLS, 1,
                "get_module_exports_directory(base_addr=" PFX "): !MEM_IMAGE\n",
                base_addr);
            ASSERT_CURIOSITY(expdir == NULL || expdir->Size == 0);
        }
    });

    LOG(THREAD_GET, LOG_SYMBOLS, 5,
        "get_module_exports_directory(base_addr=" PFX ", expdir=" PFX ")\n", base_addr,
        expdir);

    if (expdir != NULL) {
        ULONG size = expdir->Size;
        ULONG exports_vaddr = expdir->VirtualAddress;

        LOG(THREAD_GET, LOG_SYMBOLS, 5,
            "get_module_exports_directory(base_addr=" PFX ") expdir=" PFX
            " size=%d exports_vaddr=%d\n",
            base_addr, expdir, size, exports_vaddr);

        /* not all DLLs have exports - e.g. drpreinject.dll, or
         * shdoclc.dll in notepad help */
        if (size > 0) {
            IMAGE_EXPORT_DIRECTORY *exports =
                (IMAGE_EXPORT_DIRECTORY *)(base_addr + exports_vaddr);
            ASSERT_CURIOSITY(size >= sizeof(IMAGE_EXPORT_DIRECTORY));
            if (is_readable_without_exception((app_pc)exports,
                                              sizeof(IMAGE_EXPORT_DIRECTORY))) {
                if (exports_size != NULL)
                    *exports_size = size;
                ASSERT_CURIOSITY(exports->Characteristics == 0);
                return exports;
            } else {
                ASSERT_CURIOSITY(false && "bad exports directory, partial map?" ||
                                 EXEMPT_TEST("win32.partial_map.exe"));
            }
        }
    } else
        ASSERT_CURIOSITY(false && "no exports directory");

    return NULL;
}

/* Same as get_module_exports_directory except also verifies that the functions (and,
 * if check_names, ordinals and fnames) arrays are readable. NOTE - does not verify that
 * the RVA names pointed to by fnames are themselves readable strings. */
static IMAGE_EXPORT_DIRECTORY *
get_module_exports_directory_check_common(app_pc base_addr,
                                          size_t *exports_size /*OPTIONAL OUT*/,
                                          bool check_names _IF_NOT_X64(bool ldr64))
{
    IMAGE_EXPORT_DIRECTORY *exports =
        get_module_exports_directory_common(base_addr, exports_size _IF_NOT_X64(ldr64));
    if (exports != NULL) {
        PULONG functions = (PULONG)(base_addr + exports->AddressOfFunctions);
        PUSHORT ordinals = (PUSHORT)(base_addr + exports->AddressOfNameOrdinals);
        PULONG fnames = (PULONG)(base_addr + exports->AddressOfNames);
        if (exports->NumberOfFunctions > 0) {
            if (!is_readable_without_exception(
                    (byte *)functions, exports->NumberOfFunctions * sizeof(*functions))) {
                ASSERT_CURIOSITY(false &&
                                     "ill-formed exports directory, unreadable "
                                     "functions array, partial map?" ||
                                 EXEMPT_TEST("win32.partial_map.exe"));
                return NULL;
            }
        }
        if (exports->NumberOfNames > 0 && check_names) {
            ASSERT_CURIOSITY(exports->NumberOfFunctions > 0 &&
                             "ill-formed exports directory");
            if (!is_readable_without_exception(
                    (byte *)ordinals, exports->NumberOfNames * sizeof(*ordinals)) ||
                !is_readable_without_exception(
                    (byte *)fnames, exports->NumberOfNames * sizeof(*fnames))) {
                ASSERT_CURIOSITY(false &&
                                     "ill-formed exports directory, unreadable "
                                     "ordinal or names array, partial map?" ||
                                 EXEMPT_TEST("win32.partial_map.exe"));
                return NULL;
            }
        }
    }
    return exports;
}

/* XXX - this walk is similar to that used in several other module.c
 * functions, we should look into sharing. Also, like almost all of the
 * module.c routines this could be racy with app memory deallocations.
 * XXX - We could also allow wildcards and, if desired, extend it to
 * return multiple matching addresses.
 */
/* Interface is similar to msdn GetProcAddress, takes in a module handle
 * (this is just the allocation base of the module) and either a name
 * or an ordinal and returns the address of the export with said name
 * or ordinal.  Returns NULL on failure.
 * Only one of name and ordinal should be specified: the other should be
 * NULL (name) or UINT_MAX (ordinal).
 * NOTE - will return NULL for forwarded exports, exports pointing outside of
 * the module and for exports not in a code section (FIXME - is this the
 * behavior we want?). Name is case insensitive.
 */
static generic_func_t
get_proc_address_common(module_base_t lib, const char *name,
                        uint ordinal _IF_NOT_X64(bool ldr64), const char **forwarder OUT)
{
    app_pc module_base;
    size_t exports_size;
    uint i;
    IMAGE_EXPORT_DIRECTORY *exports;
    PULONG functions; /* array of RVAs */
    PUSHORT ordinals;
    PULONG fnames;       /* array of RVAs */
    uint ord = UINT_MAX; /* the ordinal to use */
    app_pc func;

    /* avoid non-core issues: we don't have get_allocation_size or dcontexts */
#if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)
    size_t module_size;
    dcontext_t *dcontext = get_thread_private_dcontext();
#endif

    if (forwarder != NULL)
        *forwarder = NULL;
    ASSERT((name != NULL && *name != '\0' && ordinal == UINT_MAX) ||
           (name == NULL && ordinal < UINT_MAX)); /* verify valid args */
    if (lib == NULL || (ordinal == UINT_MAX && (name == NULL || *name == '\0')))
        return NULL;

        /* avoid non-core issues: we don't have get_allocation_size or is_readable_pe_base
         */
#if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)
    /* FIXME - get_allocation_size and is_readable_pe_base are expensive
     * operations, we could put the onus on the caller to only pass in a
     * valid module_handle_t/pe_base and just assert instead if performance of
     * this routine becomes a concern, esp. since the caller has likely
     * already done it. */
    module_size = get_allocation_size(lib, &module_base);
    if (!is_readable_pe_base(module_base))
        return NULL;
#else
    module_base = (app_pc)lib;
#endif

    exports = get_module_exports_directory_check_common(module_base, &exports_size,
                                                        true _IF_NOT_X64(ldr64));

    /* NB: There are some DLLs (like System32\profapi.dll) that have no named
     * exported functions names, only ordinals. As a result, the only correct
     * checks we can do here are on the presence and size of the export table
     * and the presence and count of the function export list.
     */
    if (exports == NULL || exports_size == 0 || exports->AddressOfFunctions == 0 ||
        exports->NumberOfFunctions == 0) {
        LOG(GLOBAL, LOG_INTERP, 1, "%s: module 0x%08x doesn't have any exports\n",
            __FUNCTION__, module_base);
        return NULL;
    }

    /* avoid preinject issues: doesn't have module_size */
#if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)
    /* sanity checks, split up for readability */
    /* The DLL either exports nothing or has a sane combination of export
     * table address and function count.
     */
    ASSERT(exports->AddressOfFunctions == 0 ||
           (exports->AddressOfFunctions < module_size && exports->NumberOfFunctions > 0));

    /* The DLL either exports no names for its functions or has a sane combination
     * of name and ordinal table addresses and counts.
     */
    ASSERT((exports->AddressOfNames == 0 && exports->AddressOfNameOrdinals == 0) ||
           (exports->AddressOfNames < module_size &&
            exports->AddressOfNameOrdinals < module_size && exports->NumberOfNames > 0));
#endif

    functions = (PULONG)(module_base + exports->AddressOfFunctions);
    ordinals = (PUSHORT)(module_base + exports->AddressOfNameOrdinals);
    fnames = (PULONG)(module_base + exports->AddressOfNames);

    if (ordinal < UINT_MAX) {
        /* The functions array is indexed by the ordinal minus the base, to
         * support ordinals starting at 1 (i#1866).
         */
        ord = ordinal - exports->Base;
    } else if (name[0] == '#') {
        /* Ordinal forwarders are formatted as #XXX, when XXX is a positive
         * base-10 integer.
         */
        if (sscanf(name, "#%u", &ord) != 1) {
            ASSERT_CURIOSITY(false && "non-numeric ordinal forwarder");
            return NULL;
        }

        /* Like raw ordinals, these are offset from the export base.
         */
        ord -= exports->Base;
    } else {
        /* FIXME - linear walk, if this routine becomes performance critical we
         * we should use a binary search. */
        bool match = false;
        for (i = 0; i < exports->NumberOfNames; i++) {
            char *export_name = (char *)(module_base + fnames[i]);
            ASSERT_CURIOSITY((app_pc)export_name > module_base && /* sanity check */
                                 (app_pc)export_name < module_base + module_size ||
                             EXEMPT_TEST("win32.partial_map.exe"));
            /* FIXME - xref case 9717, we haven't verified that export_name string is
             * safely readable (might not be the case for improperly formed or partially
             * mapped module) and the try will only protect us if we have a thread_private
             * dcontext. Could use is_string_readable_without_exception(), but that may be
             * too much of a perf hit for the no private dcontext case. */
            TRY_EXCEPT_ALLOW_NO_DCONTEXT(
                dcontext, { match = (strcasecmp(name, export_name) == 0); },
                {
                    ASSERT_CURIOSITY_ONCE(false && "Exception during get_proc_address" ||
                                          EXEMPT_TEST("win32.partial_map.exe"));
                });
            if (match) {
                /* we have a match */
                ord = ordinals[i];
                break;
            }
        }
        if (!match) {
            /* export name wasn't found */
            return NULL;
        }
    }

    /* note - function array is indexed by ordinal */
    if (ord >= exports->NumberOfFunctions) {
        ASSERT_CURIOSITY(false && "invalid ordinal index");
        return NULL;
    }
    func = (app_pc)(module_base + functions[ord]);
    if (func == module_base) {
        /* entries can be 0 when no code/data is exported for that
         * ordinal */
        ASSERT_CURIOSITY(false && "get_proc_addr of name with empty export");
        return NULL;
    }
    /* avoid non-core issues: we don't have module_size */
#if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)
    if (func < module_base || func >= module_base + module_size) {
        /* FIXME - export isn't in the module, should we still return
         * it?  Will shimeng.dll or the like ever do this to replace
         * a function?  For now we return NULL. Xref case 9717, can also
         * happen for a partial map in which case NULL is the right thing
         * to return. */
        if (is_in_ntdll(func)) {
            /* i#: more recent loaders patch forwarded functions.
             * Since we don't make a private copy of user32.dll, we
             * hit this when a private lib imports from one of the
             * couple of user32 routines that forward to ntdll.
             */
            return convert_data_to_function(func);
        }
        ASSERT_CURIOSITY(false &&
                             "get_proc_addr export location "
                             "outside of module bounds" ||
                         EXEMPT_TEST("win32.partial_map.exe"));
        return NULL;
    }
#endif
    if (func >= (app_pc)exports && func < (app_pc)exports + exports_size) {
        /* FIXME - is forwarded function, should we still return it
         * or return the target? Check - what does GetProcAddress do?
         * For now we return NULL. Looking up the target would require
         * a get_module_handle call which might not be safe here.
         * With current and planned usage we shouldn' be looking these
         * up anyways. */
        if (forwarder != NULL) {
            /* func should point at something like "NTDLL.strlen" */
            *forwarder = (const char *)func;
            return NULL;
        } else {
            ASSERT_NOT_IMPLEMENTED(false && "get_proc_addr export is forwarded");
            return NULL;
        }
    }
    /* avoid non-core issues: we don't have is_in_code_section */
#if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)
#endif
    /* get around warnings converting app_pc to generic_func_t */
    return convert_data_to_function(func);
}

IMAGE_EXPORT_DIRECTORY *
get_module_exports_directory(app_pc base_addr, size_t *exports_size /* OPTIONAL OUT */)
{
    return get_module_exports_directory_common(base_addr,
                                               exports_size _IF_NOT_X64(false));
}

IMAGE_EXPORT_DIRECTORY *
get_module_exports_directory_check(app_pc base_addr,
                                   size_t *exports_size /*OPTIONAL OUT*/,
                                   bool check_names)
{
    return get_module_exports_directory_check_common(base_addr, exports_size,
                                                     check_names _IF_NOT_X64(false));
}

generic_func_t
d_r_get_proc_address(module_base_t lib, const char *name)
{
    return get_proc_address_common(lib, name, UINT_MAX _IF_NOT_X64(false), NULL);
}

#ifndef NOT_DYNAMORIO_CORE /* else need get_own_peb() alternate */

/* could be linked w/ non-core but only used by loader.c so far */
generic_func_t
get_proc_address_ex(module_base_t lib, const char *name, const char **forwarder OUT)
{
    return get_proc_address_common(lib, name, UINT_MAX _IF_NOT_X64(false), forwarder);
}

/* could be linked w/ non-core but only used by loader.c so far */
generic_func_t
get_proc_address_by_ordinal(module_base_t lib, uint ordinal, const char **forwarder OUT)
{
    return get_proc_address_common(lib, NULL, ordinal _IF_NOT_X64(false), forwarder);
}

#    if !defined(NOT_DYNAMORIO_CORE) && !defined(NOT_DYNAMORIO_CORE_PROPER)

generic_func_t
get_proc_address_resolve_forward(module_base_t lib, const char *name)
{
    /* We match GetProcAddress and follow forwarded exports (i#428).
     * Not doing this inside d_r_get_proc_address() b/c I'm not certain the core
     * never relies on the answer being inside the asked-about module.
     */
    const char *forwarder, *forwfunc;
    char forwmodpath[MAXIMUM_PATH];
    generic_func_t func = get_proc_address_ex(lib, name, &forwarder);
    module_data_t *forwmod;
    /* XXX: this is based on loader.c's privload_process_one_import(): should
     * try to share some of the code
     */
    while (func == NULL && forwarder != NULL) {
        forwfunc = strchr(forwarder, '.') + 1;
        /* XXX: forwarder string constraints are not documented and
         * all I've seen look like this: "NTDLL.RtlAllocateHeap".
         * so I've never seen a full filename or path.
         * but there could still be extra dots somewhere: watch for them.
         */
        if (forwfunc == (char *)(ptr_int_t)1 || strchr(forwfunc + 1, '.') != NULL) {
            CLIENT_ASSERT(false, "unexpected forwarder string");
            return NULL;
        }
        if (forwfunc - forwarder + strlen("dll") >= BUFFER_SIZE_ELEMENTS(forwmodpath)) {
            CLIENT_ASSERT(false, "import string too long");
            LOG(GLOBAL, LOG_INTERP, 1, "%s: import string %s too long\n", __FUNCTION__,
                forwarder);
            return NULL;
        }
        snprintf(forwmodpath, forwfunc - forwarder, "%s", forwarder);
        snprintf(forwmodpath + (forwfunc - forwarder), strlen("dll"), "dll");
        forwmodpath[forwfunc - 1 /*'.'*/ - forwarder + strlen(".dll")] = '\0';
        LOG(GLOBAL, LOG_INTERP, 3, "\tforwarder %s => %s %s\n", forwarder, forwmodpath,
            forwfunc);
        forwmod = dr_lookup_module_by_name(forwmodpath);
        if (forwmod == NULL) {
            LOG(GLOBAL, LOG_INTERP, 1,
                "%s: unable to load forwarder for %s\n" __FUNCTION__, forwarder);
            return NULL;
        }
        /* should be listed as import; don't want to inc ref count on each forw */
        func = get_proc_address_ex(forwmod->start, forwfunc, &forwarder);
        dr_free_module_data(forwmod);
    }
    return func;
}

DR_API
dr_symbol_export_iterator_t *
dr_symbol_export_iterator_start(module_handle_t handle)
{
    pe_symbol_export_iterator_t *iter;
    byte *base_check;

    iter = global_heap_alloc(sizeof(*iter) HEAPACCT(ACCT_CLIENT));
    memset(iter, 0, sizeof(*iter));
    iter->mod_base = (byte *)handle;
    iter->mod_size = get_allocation_size(iter->mod_base, &base_check);
    if (base_check != iter->mod_base || !is_readable_pe_base(base_check))
        goto error;
    iter->exports = get_module_exports_directory_check_common(
        iter->mod_base, &iter->exports_size, true _IF_NOT_X64(false));
    if (iter->exports == NULL || iter->exports_size == 0 ||
        iter->exports->AddressOfNames >= iter->mod_size ||
        iter->exports->AddressOfFunctions >= iter->mod_size ||
        iter->exports->AddressOfNameOrdinals >= iter->mod_size)
        goto error;

    iter->functions = (PULONG)(iter->mod_base + iter->exports->AddressOfFunctions);
    iter->ordinals = (PUSHORT)(iter->mod_base + iter->exports->AddressOfNameOrdinals);
    iter->fnames = (PULONG)(iter->mod_base + iter->exports->AddressOfNames);
    iter->idx = 0;
    iter->hasnext = (iter->idx < iter->exports->NumberOfNames);

    return (dr_symbol_export_iterator_t *)iter;

error:
    global_heap_free(iter, sizeof(*iter) HEAPACCT(ACCT_CLIENT));
    return NULL;
}

DR_API
bool
dr_symbol_export_iterator_hasnext(dr_symbol_export_iterator_t *dr_iter)
{
    pe_symbol_export_iterator_t *iter = (pe_symbol_export_iterator_t *)dr_iter;
    return (iter != NULL && iter->hasnext);
}

DR_API
dr_symbol_export_t *
dr_symbol_export_iterator_next(dr_symbol_export_iterator_t *dr_iter)
{
    pe_symbol_export_iterator_t *iter = (pe_symbol_export_iterator_t *)dr_iter;
    dcontext_t *dcontext = get_thread_private_dcontext();

    CLIENT_ASSERT(iter != NULL, "invalid parameter");
    CLIENT_ASSERT(iter->hasnext, "dr_symbol_export_iterator_next: !hasnext");
    CLIENT_ASSERT(iter->idx < iter->exports->NumberOfNames, "export iter internal error");

    memset(&iter->info, 0, sizeof(iter->info));
    iter->info.name = (char *)(iter->mod_base + iter->fnames[iter->idx]);
    if ((app_pc)iter->info.name < iter->mod_base ||
        (app_pc)iter->info.name >= iter->mod_base + iter->mod_size)
        return NULL;

    iter->info.ordinal = iter->ordinals[iter->idx];
    if (iter->info.ordinal >= iter->exports->NumberOfFunctions)
        return NULL;
    iter->info.addr = (app_pc)(iter->mod_base + iter->functions[iter->info.ordinal]);
    if (iter->info.addr == iter->mod_base) {
        /* see get_proc_address_ex: this means there's no export */
        return NULL;
    }
    if (iter->info.addr < iter->mod_base ||
        iter->info.addr >= iter->mod_base + iter->mod_size) {
        /* an already-patched forward -- we leave as is */
    } else if (iter->info.addr >= (app_pc)iter->exports &&
               iter->info.addr < (app_pc)iter->exports + iter->exports_size) {
        iter->info.forward = (const char *)iter->info.addr;
        iter->info.addr = NULL;
    }
    iter->info.is_code = true;
    iter->idx++;
    iter->hasnext = (iter->idx < iter->exports->NumberOfNames);

    return &iter->info;
}

DR_API
void
dr_symbol_export_iterator_stop(dr_symbol_export_iterator_t *dr_iter)
{
    pe_symbol_export_iterator_t *iter = (pe_symbol_export_iterator_t *)dr_iter;
    if (iter == NULL)
        return;
    global_heap_free(iter, sizeof(*iter) HEAPACCT(ACCT_CLIENT));
}

#    endif /* core proper */

/* returns NULL if no loader module is found
 * N.B.: walking loader data structures at random times is dangerous! See
 * get_ldr_module_by_pc in module.c for code to grab the ldr lock (which is
 * also unsafe).  Here we presume that we already own the ldr lock and that
 * the ldr list is consistent, which should be the case for preinject (the only
 * user).  FIXME stick this in module.c with get_ldr_module_by_pc, would need
 * to get module.c compiled into preinjector which is a significant hassle.
 */
LDR_MODULE *
get_ldr_module_by_name(wchar_t *name)
{
    PEB *peb = get_own_peb();
    PEB_LDR_DATA *ldr = peb->LoaderData;
    LIST_ENTRY *e, *mark;
    LDR_MODULE *mod;
    uint traversed = 0; /* a simple infinite loop break out */

    /* Now, you'd think these would actually be in memory order, but they
     * don't seem to be for me!
     */
    mark = &ldr->InMemoryOrderModuleList;

    for (e = mark->Flink; e != mark; e = e->Flink) {
        mod = (LDR_MODULE *)((char *)e - offsetof(LDR_MODULE, InMemoryOrderModuleList));
        /* NOTE - for comparison we could use pe_name or mod->BaseDllName.
         * Our current usage is just to get user32.dll for which BaseDllName
         * is prob. better (can't rename user32, and a random dll could have
         * user32.dll as a pe_name).  If wanted to be extra certain could
         * check FullDllName for %systemroot%/system32/user32.dll as that
         * should ensure uniqueness.
         */
        ASSERT(mod->BaseDllName.Length <= mod->BaseDllName.MaximumLength &&
               mod->BaseDllName.Buffer != NULL);
        if (wcscasecmp(name, mod->BaseDllName.Buffer) == 0) {
            return mod;
        }

        if (traversed++ > MAX_MODULE_LIST_INFINITE_LOOP_THRESHOLD) {
            /* Only caller (preinject) should hold the ldr lock and the ldr
             * state should be consistent so we don't expect to get stuck. */
            ASSERT_NOT_REACHED();
            /* TODO: In case we ever hit this we may want to retry the
             * traversal once more */
            return NULL;
        }
    }
    return NULL;
}

bool
ldr_module_statically_linked(LDR_MODULE *mod)
{
    /* The ldr uses -1 as the load count for statically linked dlls
     * (signals to not bother to keep track of the load count/never
     * unload).  It doesn't appear to ever use this value for non-
     * statically linked dlls (including user32.dll if late loaded).
     *
     * i#1522: However, on Win8, they renamed the LoadCount field to
     * ObsoleteLoadCount, and it seems that many statically linked dlls have
     * a positive value.  There are 2 other fields: LDR_PROCESS_STATIC_IMPORT
     * in the Flags ("ProcessStaticImport" bitfield in PDB types), and
     * LoadReasonStaticDependency.  Looking at real data, though, the fields
     * are very confusingly used, so for now we accept any of the 3.
     */
    bool win8plus = false;
#    if defined(NOT_DYNAMORIO_CORE) || defined(NOT_DYNAMORIO_CORE_PROPER)
    PEB *peb = get_own_peb();
    win8plus = ((peb->OSMajorVersion == 6 && peb->OSMinorVersion >= 2) ||
                peb->OSMajorVersion > 6);
#    else
    win8plus = (get_os_version() >= WINDOWS_VERSION_8);
#    endif
    if (win8plus) {
        return (mod->LoadCount == -1 || TEST(LDR_PROCESS_STATIC_IMPORT, mod->Flags) ||
                mod->LoadReason == LoadReasonStaticDependency ||
                mod->LoadReason == LoadReasonStaticForwarderDependency);
    } else
        return (mod->LoadCount == -1);
}
#endif /* !NOT_DYNAMORIO_CORE */

/****************************************************************************/
#ifndef X64

/* PR 271719: Access x64 loader data from WOW64.
 * We duplicate a bunch of data structures and code here, but this is cleaner
 * than compiling the original code as x64 and hacking the build process to
 * get it linked: stick as char[] inside code section or something.
 */

typedef struct ALIGN_VAR(8) _LIST_ENTRY_64 {
    uint64 /* struct _LIST_ENTRY_64 * */ Flink;
    uint64 /* struct _LIST_ENTRY_64 * */ Blink;
} LIST_ENTRY_64;

/* UNICODE_STRING_64 is in ntdll.h */

/* module information filled by the loader */
typedef struct ALIGN_VAR(8) _PEB_LDR_DATA_64 {
    ULONG Length;
    BOOLEAN Initialized;
    PVOID SsHandle;
    uint SsHandle_hi;
    LIST_ENTRY_64 InLoadOrderModuleList;
    LIST_ENTRY_64 InMemoryOrderModuleList;
    LIST_ENTRY_64 InInitializationOrderModuleList;
} PEB_LDR_DATA_64;

/* Note that these lists are walked through corresponding LIST_ENTRY pointers
 * i.e., for InInit*Order*, Flink points 16 bytes into the LDR_MODULE structure
 */
typedef struct ALIGN_VAR(8) _LDR_MODULE_64 {
    LIST_ENTRY_64 InLoadOrderModuleList;
    LIST_ENTRY_64 InMemoryOrderModuleList;
    LIST_ENTRY_64 InInitializationOrderModuleList;
    uint64 BaseAddress;
    uint64 EntryPoint;
    ULONG SizeOfImage;
    int padding;
    UNICODE_STRING_64 FullDllName;
    UNICODE_STRING_64 BaseDllName;
    ULONG Flags;
    SHORT LoadCount;
    SHORT TlsIndex;
    LIST_ENTRY_64 HashTableEntry; /* see notes for LDR_MODULE */
    ULONG TimeDateStamp;
} LDR_MODULE_64;

typedef void (*void_func_t)();

#    define MAX_MODNAME_SIZE 128

/* in drlibc_x86.asm */
extern int
switch_modes_and_load(void *ntdll64_LdrLoadDll, UNICODE_STRING_64 *lib, HANDLE *result);

/* Here and not in ntdll.c b/c libutil targets link to this file but not
 * ntdll.c
 */
uint64
get_own_x64_peb(void)
{
    /* __readgsqword is not supported for 32-bit */
    /* We assume the x64 PEB is in the low 4GB (else we'll need syscall to
     * get its value).
     */
    uint peb64, peb64_hi;
    if (!is_wow64_process(NT_CURRENT_PROCESS)) {
        ASSERT_NOT_REACHED();
        return 0;
    }
    __asm {
        mov eax, dword ptr gs:X64_PEB_TIB_OFFSET
        mov peb64, eax
        mov eax, dword ptr gs:(X64_PEB_TIB_OFFSET+4)
        mov peb64_hi, eax
    }
    ;
    ASSERT(peb64_hi == 0); /* Though could we even read it if it were high? */
    return (uint64)peb64;
}

#    ifdef NOT_DYNAMORIO_CORE
/* This is not in the headers exported to libutil */
process_id_t
get_process_id(void);
#    endif

static bool
read64(uint64 addr, size_t sz, void *buf)
{
    size_t got;
    HANDLE proc = NT_CURRENT_PROCESS;
    NTSTATUS res;
    /* On Win10, passing NT_CURRENT_PROCESS results in STATUS_INVALID_HANDLE
     * (pretty strange).
     */
#    if !defined(NOT_DYNAMORIO_CORE) && !defined(NOT_DYNAMORIO_CORE_PROPER)
    if (get_os_version() >= WINDOWS_VERSION_10)
        proc = process_handle_from_id(get_process_id());
#    else
    /* We don't have easy access to version info or PEB so we always use a real handle */
    proc = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE,
                       GetCurrentProcessId());
#    endif
    res = nt_wow64_read_virtual_memory64(proc, addr, buf, sz, &got);
    if (proc != NT_CURRENT_PROCESS) {
#    if !defined(NOT_DYNAMORIO_CORE) && !defined(NOT_DYNAMORIO_CORE_PROPER)
        close_handle(proc);
#    else
        CloseHandle(proc);
#    endif
    }
    return (NT_SUCCESS(res) && got == sz);
}

static uint64
get_ldr_data_64(void)
{
    uint64 peb64 = get_own_x64_peb();
    uint64 ldr64 = 0;
    if (!read64(peb64 + X64_LDR_PEB_OFFSET, sizeof(ldr64), &ldr64))
        return 0;
    return ldr64;
}

/* Pass either name or base.
 *
 * XXX: this can be racy, accessing app loader data structs!  Use with care.
 * Caller should synchronize w/ other threads, and avoid calling while app
 * holds the x64 loader lock.
 */
static bool
get_ldr_module_64(const wchar_t *name, uint64 base, LDR_MODULE_64 *out)
{
    /* Be careful: we can't directly de-ref any ptrs b/c they can be >4GB */
    uint64 ldr_addr = get_ldr_data_64();
    PEB_LDR_DATA_64 ldr;
    uint64 e_addr, mark_addr;
    LIST_ENTRY_64 e, mark;
    LDR_MODULE_64 mod;
    wchar_t local_buf[MAX_MODNAME_SIZE];
    uint traversed = 0; /* a simple infinite loop break out */

    if (ldr_addr == 0 || !read64(ldr_addr, sizeof(ldr), &ldr))
        return false;

    /* Now, you'd think these would actually be in memory order, but they
     * don't seem to be for me!
     */
    mark_addr = ldr_addr + offsetof(PEB_LDR_DATA_64, InMemoryOrderModuleList);
    if (!read64(mark_addr, sizeof(mark), &mark))
        return false;

    for (e_addr = mark.Flink; e_addr != mark_addr; e_addr = e.Flink) {
        if (!read64(e_addr, sizeof(e), &e) ||
            !read64(e_addr - offsetof(LDR_MODULE_64, InMemoryOrderModuleList),
                    sizeof(mod), &mod))
            return false;
        ASSERT(mod.BaseDllName.Length <= mod.BaseDllName.MaximumLength &&
               mod.BaseDllName.u.Buffer64 != 0);
        if (name != NULL) {
            int len = MIN(mod.BaseDllName.Length, BUFFER_SIZE_BYTES(local_buf));
            if (!read64(mod.BaseDllName.u.Buffer64, len, local_buf))
                return false;
            if (len < BUFFER_SIZE_BYTES(local_buf))
                local_buf[len / sizeof(wchar_t)] = L'\0';
            else
                NULL_TERMINATE_BUFFER(local_buf);
            if (wcscasecmp(name, local_buf) == 0) {
                memcpy(out, &mod, sizeof(mod));
                return true;
            }
        } else if (base != 0 && base == mod.BaseAddress) {
            memcpy(out, &mod, sizeof(mod));
            return true;
        }

        if (traversed++ > MAX_MODULE_LIST_INFINITE_LOOP_THRESHOLD) {
            /* Only caller (preinject) should hold the ldr lock and the ldr
             * state should be consistent so we don't expect to get stuck. */
            ASSERT_NOT_REACHED();
            /* TODO: In case we ever hit this we may want to retry the
             * traversal once more */
            return false;
        }
    }
    return false;
}

/* returns NULL if no loader module is found
 * N.B.: walking loader data structures at random times is dangerous! See
 * get_ldr_module_by_pc in module.c for code to grab the ldr lock (which is
 * also unsafe).  Here we presume that we already own the ldr lock and that
 * the ldr list is consistent, which should be the case for preinject (the only
 * user).  FIXME stick this in module.c with get_ldr_module_by_pc, would need
 * to get module.c compiled into preinjector which is a significant hassle.
 *
 * This is now used by more than just preinjector, and it's up to the caller
 * to synchronize and avoid calling while the app holds the x64 loader lock.
 */
uint64
get_module_handle_64(const wchar_t *name)
{
    /* Be careful: we can't directly de-ref any ptrs b/c they can be >4GB */
    LDR_MODULE_64 mod;
    if (!get_ldr_module_64(name, 0, &mod))
        return 0;
    return mod.BaseAddress;
}

uint64
get_proc_address_64(uint64 lib, const char *name)
{
    /* Because we have to handle 64-bit addresses, we can't share
     * get_proc_address_common().  We thus have a specialized routine here.
     * We ignore forwarders and ordinals.
     */
    size_t exports_size;
    IMAGE_DOS_HEADER dos;
    IMAGE_NT_HEADERS64 nt;
    IMAGE_DATA_DIRECTORY *expdir;
    IMAGE_EXPORT_DIRECTORY exports;
    uint i;
    PULONG functions; /* array of RVAs */
    PUSHORT ordinals;
    PULONG fnames;       /* array of RVAs */
    uint ord = UINT_MAX; /* the ordinal to use */
    uint64 func = 0;
    char local_buf[MAX_FUNCNAME_SIZE];

    if (!read64(lib, sizeof(dos), &dos) || !read64(lib + dos.e_lfanew, sizeof(nt), &nt))
        return 0;
    ASSERT(dos.e_magic == IMAGE_DOS_SIGNATURE);
    ASSERT(nt.Signature == IMAGE_NT_SIGNATURE);
    expdir = &nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    exports_size = expdir->Size;
    if (exports_size <= 0 ||
        !read64(lib + expdir->VirtualAddress, MIN(exports_size, sizeof(exports)),
                &exports))
        return 0;
    if (exports.NumberOfNames == 0 || exports.AddressOfNames == 0)
        return 0;

#    if defined(NOT_DYNAMORIO_CORE) || defined(NOT_DYNAMORIO_CORE_PROPER)
    functions =
        (PULONG)HeapAlloc(GetProcessHeap(), 0, exports.NumberOfFunctions * sizeof(ULONG));
    ordinals =
        (PUSHORT)HeapAlloc(GetProcessHeap(), 0, exports.NumberOfNames * sizeof(USHORT));
    fnames =
        (PULONG)HeapAlloc(GetProcessHeap(), 0, exports.NumberOfNames * sizeof(ULONG));
#    else
    functions = (PULONG)global_heap_alloc(exports.NumberOfFunctions *
                                          sizeof(ULONG) HEAPACCT(ACCT_OTHER));
    ordinals = (PUSHORT)global_heap_alloc(exports.NumberOfNames *
                                          sizeof(USHORT) HEAPACCT(ACCT_OTHER));
    fnames = (PULONG)global_heap_alloc(exports.NumberOfNames *
                                       sizeof(ULONG) HEAPACCT(ACCT_OTHER));
#    endif
    if (read64(lib + exports.AddressOfFunctions,
               exports.NumberOfFunctions * sizeof(ULONG), functions) &&
        read64(lib + exports.AddressOfNameOrdinals,
               exports.NumberOfNames * sizeof(USHORT), ordinals) &&
        read64(lib + exports.AddressOfNames, exports.NumberOfNames * sizeof(ULONG),
               fnames)) {
        bool match = false;
        for (i = 0; i < exports.NumberOfNames; i++) {
            if (!read64(lib + fnames[i], BUFFER_SIZE_BYTES(local_buf), local_buf))
                break;
            NULL_TERMINATE_BUFFER(local_buf);
            if (strcasecmp(name, local_buf) == 0) {
                match = true;
                ord = ordinals[i];
                break;
            }
        }
        if (match && ord < exports.NumberOfFunctions && functions[ord] != 0 &&
            /* We don't support forwarded functions */
            (functions[ord] < expdir->VirtualAddress ||
             functions[ord] >= expdir->VirtualAddress + exports_size))
            func = lib + functions[ord];
    }
#    if defined(NOT_DYNAMORIO_CORE) || defined(NOT_DYNAMORIO_CORE_PROPER)
    HeapFree(GetProcessHeap(), 0, functions);
    HeapFree(GetProcessHeap(), 0, ordinals);
    HeapFree(GetProcessHeap(), 0, fnames);
#    else
    global_heap_free(functions,
                     exports.NumberOfFunctions * sizeof(ULONG) HEAPACCT(ACCT_OTHER));
    global_heap_free(ordinals,
                     exports.NumberOfNames * sizeof(USHORT) HEAPACCT(ACCT_OTHER));
    global_heap_free(fnames, exports.NumberOfNames * sizeof(ULONG) HEAPACCT(ACCT_OTHER));
#    endif
    return func;
}

/* Excluding from libutil b/c it doesn't need it and it would be a pain
 * to switch _snwprintf, etc. to work w/ UNICODE.
 * Up to caller to synchronize and avoid interfering w/ app.
 */
#    ifndef NOT_DYNAMORIO_CORE
HANDLE
load_library_64(const char *path)
{
    uint64 ntdll64;
    HANDLE result;
    int success;
    byte *ntdll64_LoadLibrary;

    /* We hand-build our UNICODE_STRING_64 rather than jumping through
     * hoops to call ntdll64's RtlInitUnicodeString */
    UNICODE_STRING_64 us;
    wchar_t wpath[MAXIMUM_PATH + 1];
    _snwprintf(wpath, BUFFER_SIZE_ELEMENTS(wpath), L"%S", path);
    NULL_TERMINATE_BUFFER(wpath);

    ASSERT((wcslen(wpath) + 1) * sizeof(wchar_t) <= USHRT_MAX);
    us.Length = (USHORT)wcslen(wpath) * sizeof(wchar_t);
    /* If not >= 2 bytes larger then STATUS_INVALID_PARAMETER ((NTSTATUS)0xC000000DL) */
    us.MaximumLength = (USHORT)(wcslen(wpath) + 1) * sizeof(wchar_t);
    us.u.b32.Buffer32 = wpath;
    us.u.b32.Buffer32_hi = 0;

    /* this is racy, but it's up to the caller to synchronize */
    ntdll64 = get_module_handle_64(L"ntdll.dll");
    /* XXX i#1633: this routine does not yet support ntdll64 > 4GB */
    if (ntdll64 > UINT_MAX || ntdll64 == 0)
        return NULL;

    LOG(THREAD_GET, LOG_LOADER, 3, "Found ntdll64 at " UINT64_FORMAT_STRING " %s\n",
        ntdll64, path);
    /* There is no kernel32 so we use LdrLoadDll.
     * 32-bit GetProcAddress is doing some header checks and fails,
     * Our 32-bit get_proc_address does work though.
     */
    ntdll64_LoadLibrary = (byte *)(uint)get_proc_address_64(ntdll64, "LdrLoadDll");
    LOG(THREAD_GET, LOG_LOADER, 3, "Found ntdll64!LdrLoadDll at 0x%08x\n",
        ntdll64_LoadLibrary);
    if (ntdll64_LoadLibrary == NULL)
        return NULL;

    /* XXX: the WOW64 x64 loader refuses to load kernel32.dll via a name
     * check versus ntdll!Kernel32String (pre-Win7) or ntdll!LdrpKernel32DllName
     * (Win7).  That's not an exported symbol so we can't robustly locate it
     * to work around it (in tests, disabling the check leads to successfully
     * loading kernel32, though its entry point fails on Vista+).
     */
    success = switch_modes_and_load(ntdll64_LoadLibrary, &us, &result);
    LOG(THREAD_GET, LOG_LOADER, 3, "Loaded at 0x%08x with success 0x%08x\n", result,
        success);
    if (success >= 0) {
        /* preinject doesn't have get_os_version() but it only loads DR */
#        if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)
        if (get_os_version() >= WINDOWS_VERSION_VISTA) {
            /* The WOW64 x64 loader on Vista+ does not seem to
             * call any entry points so we do so here.
             *
             * FIXME i#979: we should walk the Ldr list afterward to see what
             * dependent libs were loaded so we can call their entry points.
             *
             * FIXME i#979: we should check for the Ldr entry existing already to
             * avoid calling the entry point twice!
             */
            LDR_MODULE_64 mod;
            dr_auxlib64_routine_ptr_t entry;
            DEBUG_DECLARE(bool ok =)
            get_ldr_module_64(NULL, (uint64)result, &mod);
            ASSERT(ok);
            entry = (dr_auxlib64_routine_ptr_t)mod.EntryPoint;
            if (entry != 0) {
                if (dr_invoke_x64_routine(entry, 3, result, DLL_PROCESS_ATTACH, NULL))
                    return result;
                else {
                    LOG(THREAD_GET, LOG_LOADER, 1, "init routine for %s failed!\n", path);
                    free_library_64(result);
                }
            } else
                return result;
        } else
#        endif
            return result;
    }
    return NULL;
}

bool
free_library_64(HANDLE lib)
{
    uint64 ntdll64_LdrUnloadDll;
    int res;
    uint64 ntdll64 = get_module_handle_64(L"ntdll.dll");
    /* XXX i#1633: we don't yet support ntdll64 > 4GB (need to update code below) */
    if (ntdll64 > UINT_MAX || ntdll64 == 0)
        return false;
    ntdll64_LdrUnloadDll = get_proc_address_64(ntdll64, "LdrUnloadDll");
    invoke_func64_t args = { ntdll64_LdrUnloadDll, (uint64)lib };
    res = switch_modes_and_call(&args);
    return (res >= 0);
}

size_t
nt_get_context64_size(void)
{
    static size_t context64_size;
    if (context64_size == 0) {
        uint64 ntdll64_RtlGetExtendedContextLength;
        int len = 0;
        uint64 len_param = (uint64)&len;
        uint64 ntdll64 = get_module_handle_64(L"ntdll.dll");
        ASSERT(ntdll64 != 0);
        ntdll64_RtlGetExtendedContextLength =
            get_proc_address_64(ntdll64, "RtlGetExtendedContextLength");
        invoke_func64_t args = { ntdll64_RtlGetExtendedContextLength, CONTEXT_XSTATE,
                                 len_param };
        NTSTATUS res = switch_modes_and_call(&args);
        ASSERT(NT_SUCCESS(res));
        /* Add 16 so we can align it forward to 16. */
        context64_size = len + 16;
    }
    return context64_size;
}

bool
thread_get_context_64(HANDLE thread, CONTEXT_64 *cxt64)
{
    /* i#1035, DrMem i#1685: we could use a mode switch and then a raw 64-bit syscall,
     * which would be simpler than all this manipulating of PE structures beyond
     * our direct reach, but we need PE parsing for drmarker anyway and use the
     * same routines here.
     */
    uint64 ntdll64_GetContextThread;
    NTSTATUS res;
    uint64 ntdll64 = get_module_handle_64(L"ntdll.dll");
    if (ntdll64 == 0)
        return false;
    ntdll64_GetContextThread = get_proc_address_64(ntdll64, "NtGetContextThread");
    invoke_func64_t args = { ntdll64_GetContextThread, (uint64)thread, (uint64)cxt64 };
    res = switch_modes_and_call(&args);
    return NT_SUCCESS(res);
}

bool
thread_set_context_64(HANDLE thread, CONTEXT_64 *cxt64)
{
    uint64 ntdll64_SetContextThread;
    NTSTATUS res;
    uint64 ntdll64 = get_module_handle_64(L"ntdll.dll");
    if (ntdll64 == 0)
        return false;
    ntdll64_SetContextThread = get_proc_address_64(ntdll64, "NtSetContextThread");
    invoke_func64_t args = { ntdll64_SetContextThread, (uint64)thread, (uint64)cxt64 };
    res = switch_modes_and_call(&args);
    return NT_SUCCESS(res);
}

bool
remote_protect_virtual_memory_64(HANDLE process, uint64 base, size_t size, uint prot,
                                 uint *old_prot)
{
    uint64 ntdll64_ProtectVirtualMemory;
    NTSTATUS res;
    uint64 ntdll64 = get_module_handle_64(L"ntdll.dll");
    if (ntdll64 == 0)
        return false;
    uint64 size64 = size;
    uint64 *size_ptr = &size64;
    uint64 mybase = base;
    uint64 *base_ptr = &mybase;
    ntdll64_ProtectVirtualMemory = get_proc_address_64(ntdll64, "NtProtectVirtualMemory");
    invoke_func64_t args = { ntdll64_ProtectVirtualMemory,
                             (uint64)process,
                             (uint64)base_ptr,
                             (uint64)size_ptr,
                             prot,
                             (uint64)old_prot };
    res = switch_modes_and_call(&args);
    return NT_SUCCESS(res);
}

NTSTATUS
remote_query_virtual_memory_64(HANDLE process, uint64 addr,
                               MEMORY_BASIC_INFORMATION64 *mbi, size_t mbilen,
                               uint64 *got)
{
    uint64 ntdll64_QueryVirtualMemory;
    NTSTATUS res;
    uint64 ntdll64 = get_module_handle_64(L"ntdll.dll");
    if (ntdll64 == 0)
        return false;
    ntdll64_QueryVirtualMemory = get_proc_address_64(ntdll64, "NtQueryVirtualMemory");
    invoke_func64_t args = { ntdll64_QueryVirtualMemory,
                             (uint64)process,
                             addr,
                             MemoryBasicInformation,
                             (uint64)mbi,
                             mbilen,
                             (uint64)got };
    res = switch_modes_and_call(&args);
    return NT_SUCCESS(res);
}
#    endif /* !NOT_DYNAMORIO_CORE */
#endif     /* !X64 */

#ifndef NOT_DYNAMORIO_CORE
bool
remote_protect_virtual_memory_maybe64(HANDLE process, uint64 base, size_t size, uint prot,
                                      uint *old_prot)
{
#    ifdef X64
    return nt_remote_protect_virtual_memory(process, (void *)base, size, prot, old_prot);
#    else
    return remote_protect_virtual_memory_64(process, base, size, prot, old_prot);
#    endif
}

NTSTATUS
remote_query_virtual_memory_maybe64(HANDLE process, uint64 addr,
                                    MEMORY_BASIC_INFORMATION64 *mbi, size_t mbilen,
                                    uint64 *got)
{
#    ifdef X64
    return nt_remote_query_virtual_memory(process, (void *)addr,
                                          (MEMORY_BASIC_INFORMATION *)mbi, mbilen, got);
#    else
    return remote_query_virtual_memory_64(process, addr, mbi, mbilen, got);
#    endif
}
#endif

/* Excluding from libutil b/c it doesn't need it and is_32bit_process() and
 * read_remote_memory_maybe64() aren't exported to libutil.
 */
#ifndef NOT_DYNAMORIO_CORE
static bool
read_remote_maybe64(HANDLE process, uint64 addr, size_t bufsz, void *buf)
{
    size_t num_read;
    return read_remote_memory_maybe64(process, addr, buf, bufsz, &num_read) &&
        num_read == bufsz;
}

uint64
find_remote_dll_base(HANDLE phandle, bool find64bit, char *dll_name)
{
    MEMORY_BASIC_INFORMATION64 mbi;
    uint64 got;
    NTSTATUS res;
    uint64 addr = 0;
    char name[MAXIMUM_PATH];
    do {
        res = remote_query_virtual_memory_maybe64(phandle, addr, &mbi, sizeof(mbi), &got);
        if (got != sizeof(mbi) || !NT_SUCCESS(res))
            break;
#    if VERBOSE
        print_file(STDERR, "0x%I64x-0x%I64x type=0x%x state=0x%x\n", mbi.BaseAddress,
                   mbi.BaseAddress + mbi.RegionSize, mbi.Type, mbi.State);
#    endif
        if (mbi.Type == MEM_IMAGE && mbi.BaseAddress == mbi.AllocationBase) {
            bool is_64;
            if (get_remote_dll_short_name(phandle, mbi.BaseAddress, name,
                                          BUFFER_SIZE_ELEMENTS(name), &is_64)) {
#    if VERBOSE
                print_file(STDERR, "found |%s| @ 0x%I64x 64=%d\n", name, mbi.BaseAddress,
                           is_64);
#    endif
                if (_strcmpi(name, dll_name) == 0 && BOOLS_MATCH(find64bit, is_64))
                    return mbi.BaseAddress;
            }
        }
        if (addr + mbi.RegionSize < addr)
            break;
        /* XXX - this check is needed because otherwise, for 32-bit targets on a 64
         * bit machine, this loop doesn't return if the dll is not loaded.
         * When addr passes 0x800000000000 remote_query_virtual_memory_maybe64
         * returns the previous mbi (ending at 0x7FFFFFFF0000).
         * For now just return if the addr is not inside the mbi region. */
        if (mbi.BaseAddress + mbi.RegionSize < addr)
            break;
        addr += mbi.RegionSize;
    } while (true);
    return 0;
}

/* Handles 32-bit or 64-bit remote processes.
 * Ignores forwarders and ordinals.
 */
uint64
get_remote_proc_address(HANDLE process, uint64 remote_base, const char *name)
{
    uint64 lib = remote_base;
    size_t exports_size;
    IMAGE_DOS_HEADER dos;
    IMAGE_NT_HEADERS64 nt64;
    IMAGE_NT_HEADERS32 nt32;
    IMAGE_DATA_DIRECTORY *expdir;
    IMAGE_EXPORT_DIRECTORY exports;
    uint i;
    PULONG functions; /* array of RVAs */
    PUSHORT ordinals;
    PULONG fnames;       /* array of RVAs */
    uint ord = UINT_MAX; /* the ordinal to use */
    uint64 func = 0;
    char local_buf[MAX_FUNCNAME_SIZE];

    if (!read_remote_maybe64(process, lib, sizeof(dos), &dos))
        return 0;
    ASSERT(dos.e_magic == IMAGE_DOS_SIGNATURE);
    if (!read_remote_maybe64(process, lib + dos.e_lfanew, sizeof(nt64), &nt64))
        return 0;
    ASSERT(nt64.Signature == IMAGE_NT_SIGNATURE);
    if (nt64.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        if (!read_remote_maybe64(process, lib + dos.e_lfanew, sizeof(nt32), &nt32))
            return 0;
        ASSERT(nt32.Signature == IMAGE_NT_SIGNATURE);
        expdir = &nt32.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    } else {
        expdir = &nt64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    }
    exports_size = expdir->Size;
    if (exports_size <= 0 ||
        !read_remote_maybe64(process, lib + expdir->VirtualAddress,
                             MIN(exports_size, sizeof(exports)), &exports))
        return 0;
    if (exports.NumberOfNames == 0 || exports.AddressOfNames == 0)
        return 0;

#    if defined(NOT_DYNAMORIO_CORE) || defined(NOT_DYNAMORIO_CORE_PROPER)
    functions =
        (PULONG)HeapAlloc(GetProcessHeap(), 0, exports.NumberOfFunctions * sizeof(ULONG));
    ordinals =
        (PUSHORT)HeapAlloc(GetProcessHeap(), 0, exports.NumberOfNames * sizeof(USHORT));
    fnames =
        (PULONG)HeapAlloc(GetProcessHeap(), 0, exports.NumberOfNames * sizeof(ULONG));
#    else
    functions = (PULONG)global_heap_alloc(exports.NumberOfFunctions *
                                          sizeof(ULONG) HEAPACCT(ACCT_OTHER));
    ordinals = (PUSHORT)global_heap_alloc(exports.NumberOfNames *
                                          sizeof(USHORT) HEAPACCT(ACCT_OTHER));
    fnames = (PULONG)global_heap_alloc(exports.NumberOfNames *
                                       sizeof(ULONG) HEAPACCT(ACCT_OTHER));
#    endif
    if (read_remote_maybe64(process, lib + exports.AddressOfFunctions,
                            exports.NumberOfFunctions * sizeof(ULONG), functions) &&
        read_remote_maybe64(process, lib + exports.AddressOfNameOrdinals,
                            exports.NumberOfNames * sizeof(USHORT), ordinals) &&
        read_remote_maybe64(process, lib + exports.AddressOfNames,
                            exports.NumberOfNames * sizeof(ULONG), fnames)) {
        bool match = false;
        for (i = 0; i < exports.NumberOfNames; i++) {
            if (!read_remote_maybe64(process, lib + fnames[i],
                                     BUFFER_SIZE_BYTES(local_buf), local_buf))
                break;
            NULL_TERMINATE_BUFFER(local_buf);
            if (strcasecmp(name, local_buf) == 0) {
                match = true;
                ord = ordinals[i];
                break;
            }
        }
        if (match && ord < exports.NumberOfFunctions && functions[ord] != 0 &&
            /* We don't support forwarded functions */
            (functions[ord] < expdir->VirtualAddress ||
             functions[ord] >= expdir->VirtualAddress + exports_size))
            func = lib + functions[ord];
    }
#    if defined(NOT_DYNAMORIO_CORE) || defined(NOT_DYNAMORIO_CORE_PROPER)
    HeapFree(GetProcessHeap(), 0, functions);
    HeapFree(GetProcessHeap(), 0, ordinals);
    HeapFree(GetProcessHeap(), 0, fnames);
#    else
    global_heap_free(functions,
                     exports.NumberOfFunctions * sizeof(ULONG) HEAPACCT(ACCT_OTHER));
    global_heap_free(ordinals,
                     exports.NumberOfNames * sizeof(USHORT) HEAPACCT(ACCT_OTHER));
    global_heap_free(fnames, exports.NumberOfNames * sizeof(ULONG) HEAPACCT(ACCT_OTHER));
#    endif
    return func;
}

/* Handles 32-bit or 64-bit remote processes. */
bool
get_remote_dll_short_name(HANDLE process, uint64 remote_base, OUT char *name,
                          size_t name_len, OUT bool *is_64)
{
    uint64 lib = remote_base;
    size_t exports_size;
    IMAGE_DOS_HEADER dos;
    IMAGE_NT_HEADERS64 nt64;
    IMAGE_NT_HEADERS32 nt32;
    IMAGE_DATA_DIRECTORY *expdir;
    IMAGE_EXPORT_DIRECTORY exports;
    if (!read_remote_maybe64(process, lib, sizeof(dos), &dos))
        return false;
    if (dos.e_magic != IMAGE_DOS_SIGNATURE)
        return false;
    if (!read_remote_maybe64(process, lib + dos.e_lfanew, sizeof(nt64), &nt64))
        return false;
    if (nt64.Signature != IMAGE_NT_SIGNATURE)
        return false;
    if (nt64.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        if (!read_remote_maybe64(process, lib + dos.e_lfanew, sizeof(nt32), &nt32))
            return 0;
        ASSERT(nt32.Signature == IMAGE_NT_SIGNATURE);
        expdir = &nt32.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (is_64 != NULL)
            *is_64 = false;
    } else {
        expdir = &nt64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (is_64 != NULL)
            *is_64 = true;
    }
    exports_size = expdir->Size;
    if (exports_size <= 0 ||
        !read_remote_maybe64(process, lib + expdir->VirtualAddress,
                             MIN(exports_size, sizeof(exports)), &exports))
        return false;
    if (exports.Name == 0 ||
        !read_remote_maybe64(process, lib + exports.Name, name_len, name))
        return false;
    name[name_len - 1] = '\0';
    return true;
}
#endif

/****************************************************************************/
