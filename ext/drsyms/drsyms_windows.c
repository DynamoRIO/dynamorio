/* **********************************************************
 * Copyright (c) 2011-2012 Google, Inc.  All rights reserved.
 * Copyright (c) 2009-2010 VMware, Inc.  All rights reserved.
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

/* DRSyms DynamoRIO Extension */

/* Symbol lookup for Windows
 *
 * This library is intended to support both online (in-process) symbol
 * loading and sideline (out-of-process via communication with a symbol
 * server) symbol access.
 *
 * Uses dbghelp.dll, which comes with Windows 2000+ as version 5.0.
 * However, 5.0 does not have SymFromAddr.  Plus, XP's 5.2 has
 * SymFromName but it doesn't work (returns error every time).
 * So, we rely on redistributing 6.x+.
 * 6.3+ is required for SymSearch, but the VS2005sp1 headers have 6.1.
 *
 * We do not use SymInitialize's feature of loading symbols for all
 * modules in a process as we do not need our own nor DR's symbols
 * (xref PR 463897).
 *
 * TODO PR 463897: support symbol stores of downloaded Windows system pdbs
 *
 * TODO PR 463897: be more robust about handling failures packing in
 * loaded modules.  E.g., today we will probably fail if passed two
 * .exe's (non-relocatable).  See further comments in load_module()
 * below.
 */

#ifdef WINDOWS
# define _CRT_SECURE_NO_DEPRECATE 1
#endif

/* We use the DR API's mutex and heap whether as a client utility library
 * or (via DR standalone API) in a symbol server process
 */
#include "dr_api.h"

#include "drsyms_private.h"

#include <windows.h>
#include <dbghelp.h>
#include <stdio.h> /* _vsnprintf */

/* We use the Container Extension's hashtable */
#include "hashtable.h"

#include "drsyms.h"
#include "wininc/dia2.h"  /* for BasicType and SymTagEnum */

/* SymSearch is not present in VS2005sp1 headers */
typedef BOOL (__stdcall *func_SymSearch_t)
    (__in HANDLE hProcess,
     __in ULONG64 BaseOfDll,
     __in_opt DWORD Index,
     __in_opt DWORD SymTag,
     __in_opt PCSTR Mask,
     __in_opt DWORD64 Address,
     __in PSYM_ENUMERATESYMBOLS_CALLBACK EnumSymbolsCallback,
     __in_opt PVOID UserContext,
     __in DWORD Options);
/* only valid for dbghelp 6.6+ */
#ifndef SYMSEARCH_ALLITEMS
# define SYMSEARCH_ALLITEMS 0x08
#endif

typedef struct _mod_entry_t {
    /* whether to use pecoff table + unix-style debug info, or use dbghelp */
    bool use_pecoff_symtable;
    union {
        void *pecoff_data;
        DWORD64 load_base; /* for dbghelp */
    } u;
} mod_entry_t;

/* All dbghelp routines are un-synchronized so we provide our own synch */
static void *symbol_lock;

/* Hashtable for mapping module paths to addresses */
#define MODTABLE_HASH_BITS 8
static hashtable_t modtable;

/* For debugging */
static bool verbose = false;

/* Sideline server support */
static const wchar_t *shmid;

#define IS_SIDELINE (shmid != 0)

/* We assume that the DWORD64 type used by dbghelp for module base addresses
 * is fine to be truncated to a 32-bit void* for 32-bit code
 */
static DWORD64 next_load = 0x11000000;

static void
unload_module(HANDLE proc, DWORD64 base);
static size_t
demangle_symbol(char *dst OUT, size_t dst_sz, const char *mangled, uint flags);

static void
modtable_entry_free(void *p)
{
    mod_entry_t *mod = (mod_entry_t *) p;
    if (mod->use_pecoff_symtable)
        drsym_unix_unload(mod->u.pecoff_data);
    else
        unload_module(GetCurrentProcess(), mod->u.load_base);
    dr_global_free(mod, sizeof(*mod));
}

DR_EXPORT
drsym_error_t
drsym_init(const wchar_t *shmid_in)
{
    shmid = shmid_in;

    symbol_lock = dr_recurlock_create();

    if (IS_SIDELINE) {
        /* FIXME NYI: establish connection with sideline server via
         * shared memory specified by shmid
         */
    } else {
        hashtable_init_ex(&modtable, MODTABLE_HASH_BITS, HASH_STRING_NOCASE,
                          true/*strdup*/, false/*!synch: using symbol_lock*/,
                          modtable_entry_free, NULL, NULL);

        /* FIXME i#601: We'd like to honor the mangling flags passed to each
         * search routine, but the demangling process used by SYMOPT_UNDNAME
         * loses information, so we can provide neither the fully mangled name
         * nor the parameter types for the symbol.  We can't change
         * SYMOPT_UNDNAME while we're running, either, or we get stuck with
         * whatever version of the symbols were loaded into memory when we load
         * the module.
         */
        SymSetOptions(SymGetOptions() | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);

        if (!SymInitialize(GetCurrentProcess(), NULL, FALSE)) {
            NOTIFY("SymInitialize error %d\n", GetLastError());
            return DRSYM_ERROR;
        }
    }

    drsym_unix_init();

    return DRSYM_SUCCESS;
}

DR_EXPORT
drsym_error_t
drsym_exit(void)
{
    drsym_error_t res = DRSYM_SUCCESS;
    if (!IS_SIDELINE) {
        hashtable_delete(&modtable);
        if (!SymCleanup(GetCurrentProcess())) {
            NOTIFY("SymCleanup error %d\n", GetLastError());
            res = DRSYM_ERROR;
        }
    }
    dr_recurlock_destroy(symbol_lock);

    return res;
}

/* Queries the available debug information for a module.
 * kind_p is optional.  Returns true on success.
 */
static bool
query_available(HANDLE proc, DWORD64 base, drsym_debug_kind_t *kind_p)
{
    drsym_debug_kind_t kind;
    IMAGEHLP_MODULE64 info; 
    memset(&info, 0, sizeof(info)); 
    info.SizeOfStruct = sizeof(info); 
    if (SymGetModuleInfo64(GetCurrentProcess(), base, &info)) {
        kind = 0;
        switch(info.SymType) {
        case SymNone: 
            NOTIFY("No symbols found\n");
            break; 
        case SymExport: 
            NOTIFY("Only export symbols found\n"); 
            break; 
        case SymPdb: 
            NOTIFY("Loaded pdb symbols from %s\n", info.LoadedPdbName);
            kind |= DRSYM_SYMBOLS | DRSYM_PDB;
            break;
        case SymDeferred: 
            NOTIFY("Symbol load deferred\n");
            break; 
        case SymCoff: 
        case SymCv: 
        case SymSym: 
        case SymVirtual: 
        case SymDia: 
            NOTIFY("Symbols in image file loaded\n"); 
            break; 
        default: 
            NOTIFY("Symbols in unknown format.\n");
            break; 
        }

        if (info.LineNumbers) {
            NOTIFY("  module has line number information.\n");
            kind |= DRSYM_LINE_NUMS;
        }

        /* could print out info.ImageName and info.LoadedImageName
         * and warn if info.PdbUnmatched or info.DbgUnmatched
         */
    } else {
        return false;
    }

    if (kind_p != NULL)
        *kind_p = kind;

    return true;
}

static DWORD64
load_module(HANDLE proc, const char *path)
{
    DWORD64 base;
    DWORD64 size;
    char *ext = strrchr(path, '.');

    /* We specify bases and try to pack the address space, except for
     * the .exe which is not relocatable.
     */
    if (!stri_eq(ext, ".exe")) {
        /* Any base will do, but we need the size */
        HANDLE f = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, 
                              NULL, OPEN_EXISTING, 0, NULL);
        if (f == INVALID_HANDLE_VALUE)
            return 0;
        base = next_load;
        size = GetFileSize(f, NULL);
        CloseHandle(f);
        if (size == INVALID_FILE_SIZE)
            return 0;
        next_load += ALIGN_FORWARD(size, 64*1024);
    } else {
        /* Can pass 0 to SymLoadModule64 */
        base = 0;
        size = 0;
    }

    base = SymLoadModule64(GetCurrentProcess(), NULL, (char *)path, NULL, base,
                           (DWORD)size/*should check trunc*/);
    if (base == 0) {
        /* FIXME PR 463897: for !single_target, we should handle load
         * failure by trying a different address, informed by some
         * memory queries.  For now we assume only one .exe and that
         * it's below our start load address and that we won't fail.
         */
        NOTIFY("SymLoadModule64 error %d\n", GetLastError());
        return 0;
    }
    if (verbose) {
        NOTIFY("loaded %s at 0x%I64x\n", path, base);
        query_available(GetCurrentProcess(), base, NULL);
    }
    return base;
}

static void
unload_module(HANDLE proc, DWORD64 base)
{
    if (!SymUnloadModule64(GetCurrentProcess(), base)) {
        NOTIFY("SymUnloadModule64 error %d\n", GetLastError());
    }
}

static mod_entry_t *
lookup_or_load(const char *modpath)
{
    mod_entry_t *mod = (mod_entry_t *) hashtable_lookup(&modtable, (void *)modpath);
    if (mod == NULL) {
        mod = dr_global_alloc(sizeof(*mod));
        memset(mod, 0, sizeof(*mod));
        /* First, see whether the module has pecoff symbols */
        mod->u.pecoff_data = drsym_unix_load(modpath);
        if (mod->u.pecoff_data == NULL) {
            /* If no pecoff, use dbghelp */
            mod->use_pecoff_symtable = false;
            mod->u.load_base = load_module(GetCurrentProcess(), modpath);
            if (mod->u.load_base == 0) {
                dr_global_free(mod, sizeof(*mod));
                return NULL;
            }
        } else {
            mod->use_pecoff_symtable = true;
        }
        hashtable_add(&modtable, (void *)modpath, (void *)mod);
    }
    return mod;
}

enum {
    SYMBOL_INFO_SIZE = (sizeof(SYMBOL_INFO) + MAX_SYM_NAME*sizeof(TCHAR) - 1
                        /*1 char already in Name[1] in struct*/)
};

/* Allocates a SYMBOL_INFO struct.  Initializes the SizeOfStruct and MaxNameLen
 * fields.
 */
static PSYMBOL_INFO
alloc_symbol_info(void)
{
    /* N.B.: we do not call dr_get_current_drcontext() and use dr_thread_alloc()
     * b/c that's not supported in standalone mode and we want standalone
     * tools to be able to use drsyms
     */
    PSYMBOL_INFO info = (PSYMBOL_INFO)dr_global_alloc(SYMBOL_INFO_SIZE);
    info->SizeOfStruct = sizeof(SYMBOL_INFO);
    info->MaxNameLen = MAX_SYM_NAME;
    return info;
}

static void
free_symbol_info(PSYMBOL_INFO info)
{
    dr_global_free(info, SYMBOL_INFO_SIZE);
}

static drsym_error_t
drsym_lookup_address_local(const char *modpath, size_t modoffs,
                           drsym_info_t *out INOUT, uint flags)
{
    mod_entry_t *mod;
    DWORD64 base;
    DWORD64 disp;
    IMAGEHLP_LINE64 line;
    DWORD line_disp;
    PSYMBOL_INFO info;

    if (modpath == NULL || out == NULL)
        return DRSYM_ERROR_INVALID_PARAMETER;
    /* If we add fields in the future we would dispatch on out->struct_size */
    if (out->struct_size != sizeof(*out))
        return DRSYM_ERROR_INVALID_SIZE;

    dr_recurlock_lock(symbol_lock);
    mod = lookup_or_load(modpath);
    if (mod == NULL) {
        dr_recurlock_unlock(symbol_lock);
        return DRSYM_ERROR_LOAD_FAILED;
    }
    if (mod->use_pecoff_symtable) {
        drsym_error_t symerr =
            drsym_unix_lookup_address(mod->u.pecoff_data, modoffs, out, flags);
        dr_recurlock_unlock(symbol_lock);
        return symerr;
    }

    base = mod->u.load_base;

    if (!query_available(GetCurrentProcess(), base, &out->debug_kind)) {
        out->debug_kind = 0;
    }

    info = alloc_symbol_info();
    if (SymFromAddr(GetCurrentProcess(), base + modoffs, &disp, info)) {
        out->start_offs = (size_t) (info->Address - base);
        out->end_offs = (size_t) ((info->Address + info->Size) - base);
        strncpy(out->name, info->Name, out->name_size);
        out->name[out->name_size - 1] = '\0';
        out->name_available_size = info->NameLen*sizeof(char);
        NOTIFY("Symbol 0x%I64x => %s+0x%x (0x%I64x-0x%I64x)\n", base+modoffs, out->name,
               disp, info->Address, info->Address + info->Size);
    } else {
        NOTIFY("SymFromAddr error %d\n", GetLastError());
        free_symbol_info(info);
        dr_recurlock_unlock(symbol_lock);
        return DRSYM_ERROR_SYMBOL_NOT_FOUND;
    }
    free_symbol_info(info);

    line.SizeOfStruct = sizeof(line);
    if (SymGetLineFromAddr64(GetCurrentProcess(), base + modoffs, &line_disp, &line)) {
        NOTIFY("%s:%u+0x%x\n", line.FileName, line.LineNumber, line_disp);
        /* We assume that line.FileName has a permanent lifetime */
        out->file = line.FileName;
        out->line = line.LineNumber;
        out->line_offs = line_disp;
    } else {
        NOTIFY("SymGetLineFromAddr64 error %d\n", GetLastError());
        dr_recurlock_unlock(symbol_lock);
        return DRSYM_ERROR_LINE_NOT_AVAILABLE;
    }

    dr_recurlock_unlock(symbol_lock);
    return DRSYM_SUCCESS;
}

static drsym_error_t
drsym_lookup_symbol_local(const char *modpath, const char *symbol,
                          size_t *modoffs OUT, uint flags)
{
    mod_entry_t *mod;
    drsym_error_t r;
    PSYMBOL_INFO info;

    if (modpath == NULL || symbol == NULL || modoffs == NULL)
        return DRSYM_ERROR_INVALID_PARAMETER;

    dr_recurlock_lock(symbol_lock);
    mod = lookup_or_load(modpath);
    if (mod == NULL) {
        dr_recurlock_unlock(symbol_lock);
        return DRSYM_ERROR_LOAD_FAILED;
    }
    if (mod->use_pecoff_symtable) {
        drsym_error_t symerr =
            drsym_unix_lookup_symbol(mod->u.pecoff_data, symbol, modoffs, flags);
        dr_recurlock_unlock(symbol_lock);
        return symerr;
    }

    /* the only thing identifying the target module is the symbol name,
     * which should be of "modname!symname" format
     */
    info = alloc_symbol_info();
    if (SymFromName(GetCurrentProcess(), (char *)symbol, info)) {
        NOTIFY("0x%I64x\n", info->Address);
        *modoffs = (size_t) (info->Address - mod->u.load_base);
        r = DRSYM_SUCCESS;
    } else {
        NOTIFY("SymFromName error %d %s\n", GetLastError(), symbol);
        r = DRSYM_ERROR_SYMBOL_NOT_FOUND;
    }
    free_symbol_info(info);
    dr_recurlock_unlock(symbol_lock);
    return r;
}

typedef struct _enum_info_t {
    drsym_enumerate_cb cb;
    void *data;
    DWORD64 base;
    bool found_match;
} enum_info_t;

static BOOL CALLBACK
enum_cb(PSYMBOL_INFO pSymInfo, ULONG SymbolSize, PVOID Context) 
{
    enum_info_t *info = (enum_info_t *) Context;
    info->found_match = true;
    return (BOOL) (*info->cb)(pSymInfo->Name, (size_t) (pSymInfo->Address - info->base),
                              info->data);
}

static drsym_error_t
drsym_enumerate_symbols_local(const char *modpath, const char *match,
                              drsym_enumerate_cb callback, void *data,
                              uint flags)
{
    mod_entry_t *mod;
    enum_info_t info;

    if (modpath == NULL || callback == NULL)
        return DRSYM_ERROR_INVALID_PARAMETER;

    dr_recurlock_lock(symbol_lock);
    mod = lookup_or_load(modpath);
    if (mod == NULL) {
        dr_recurlock_unlock(symbol_lock);
        return DRSYM_ERROR_LOAD_FAILED;
    }
    if (mod->use_pecoff_symtable) {
        drsym_error_t symerr =
            drsym_unix_enumerate_symbols(mod->u.pecoff_data, callback, data, flags);
        dr_recurlock_unlock(symbol_lock);
        return symerr;
    }

    info.cb = callback;
    info.data = data;
    info.base = mod->u.load_base;
    info.found_match = false;
    if (!SymEnumSymbols(GetCurrentProcess(), mod->u.load_base, match, enum_cb,
                        (PVOID) &info)) {
        NOTIFY("SymEnumSymbols error %d\n", GetLastError());
    }

    dr_recurlock_unlock(symbol_lock);
    if (!info.found_match)
        return DRSYM_ERROR_SYMBOL_NOT_FOUND;
    return DRSYM_SUCCESS;
}

/* SymSearch (w/ default flags) is much faster than SymEnumSymbols or even
 * SymFromName so we export it separately for Windows (Dr. Memory i#313).
 */
static drsym_error_t
drsym_search_symbols_local(const char *modpath, const char *match, bool full,
                           drsym_enumerate_cb callback, void *data)
{
    mod_entry_t *mod;
    drsym_error_t res = DRSYM_SUCCESS;
    /* dbghelp.dll 6.3+ is required for SymSearch, but the VS2005sp1
     * headers and lib have only 6.1, so we dynamically look it up
     */
    static func_SymSearch_t func;

    if (modpath == NULL || callback == NULL)
        return DRSYM_ERROR_INVALID_PARAMETER;

    dr_recurlock_lock(symbol_lock);
    mod = lookup_or_load(modpath);
    if (mod == NULL)
        res = DRSYM_ERROR_LOAD_FAILED;
    else if (mod->use_pecoff_symtable)
        /* pecoff doesn't support search, and the enumerate impl in
         * drsyms_unix.c doesn't take a pattern
         */
        res = DRSYM_ERROR_NOT_IMPLEMENTED;
    else {
        enum_info_t info;
        if (func == NULL) {
            /* if we fail to find it we'll pay the lookup cost every time,
             * but if we succeed we'll cache it
             */
            HMODULE hmod = GetModuleHandle("dbghelp.dll");
            if (hmod != NULL)
                func = (func_SymSearch_t) GetProcAddress(hmod, "SymSearch");
            if (func == NULL) {
                dr_recurlock_unlock(symbol_lock);
                /* fall back to slower enum */
                return drsym_enumerate_symbols_local(modpath, match, callback,
                                                     data, DRSYM_DEFAULT_FLAGS);
            }
        }
        info.cb = callback;
        info.data = data;
        info.base = mod->u.load_base;
        if (!(*func)(GetCurrentProcess(), mod->u.load_base, 0, 0, match, 0,
                     enum_cb, (PVOID) &info,
                     full ? SYMSEARCH_ALLITEMS : 0)) {
            NOTIFY("SymSearch error %d\n", GetLastError());
            res = DRSYM_ERROR_SYMBOL_NOT_FOUND;
        }
    }
    dr_recurlock_unlock(symbol_lock);
    return res;
}

static size_t
demangle_symbol(char *dst OUT, size_t dst_sz, const char *mangled, uint flags)
{
    DWORD undec_flags;
    size_t len;

    if (TEST(DRSYM_DEMANGLE_FULL, flags)) {
        /* FIXME: I'd like to suppress "class" from the types, but I can't find
         * an option to control it other than UNDNAME_NAME_ONLY, which
         * suppresses overloads, which we want.
         */
        undec_flags = (UNDNAME_COMPLETE |
                       UNDNAME_NO_ALLOCATION_LANGUAGE |
                       UNDNAME_NO_ALLOCATION_MODEL |
                       UNDNAME_NO_MEMBER_TYPE |
                       UNDNAME_NO_FUNCTION_RETURNS |
                       UNDNAME_NO_ACCESS_SPECIFIERS |
                       UNDNAME_NO_MS_KEYWORDS);
    } else {
        /* FIXME i#587: This still expands templates.
         */
        undec_flags = UNDNAME_NAME_ONLY;
    }

    len = (size_t)UnDecorateSymbolName(mangled, dst, (DWORD)dst_sz,
                                       undec_flags);

    /* The truncation behavior is not documented, but testing shows dbghelp
     * truncates and returns the number of characters written, not how many it
     * would take to hold the buffer.  It also returns 2 less than dst_sz if
     * truncating, one for the nul byte and it's not clear what the other is
     * for.
     */
    if (len != 0 && len + 2 < dst_sz) {
        return len;  /* Success. */
    } else if (len == 0) {
        /* The docs say the contents of dst are undetermined, so we cannot rely
         * on it being truncated.
         */
        strncpy(dst, mangled, dst_sz);
        dst[dst_sz-1] = '\0';
        NOTIFY("UnDecorateSymbolName error %d\n", GetLastError());
    } else if (len + 2 >= dst_sz) {
        NOTIFY("UnDecorateSymbolName overflowed\n");
        /* FIXME: This return value is made up and may not be large enough.
         * It will work eventually if the caller reallocates their buffer
         * and retries in a loop, or if they just want to detect truncation.
         */
        len = dst_sz * 2;
    }

    return len;
}

/***************************************************************************
 * Dbghelp type information decoding routines.
 */

static drsym_error_t decode_func_type(mempool_t *pool, DWORD64 base, ULONG type_idx,
                                    drsym_type_t **type_out OUT);
static drsym_error_t decode_ptr_type (mempool_t *pool, DWORD64 base, ULONG type_idx,
                                    drsym_type_t **type_out OUT);
static drsym_error_t decode_base_type(mempool_t *pool, DWORD64 base, ULONG type_idx,
                                    drsym_type_t **type_out OUT);
static drsym_error_t decode_typedef(mempool_t *pool, DWORD64 base, ULONG type_idx,
                                    drsym_type_t **type_out OUT);
static drsym_error_t decode_arg_type (mempool_t *pool, DWORD64 base, ULONG type_idx,
                                    drsym_type_t **type_out OUT);
static drsym_error_t decode_type   (mempool_t *pool, DWORD64 base, ULONG type_idx,
                                    drsym_type_t **type_out OUT);
static drsym_error_t make_unknown(mempool_t *pool, drsym_type_t **type_out OUT);

static bool
get_type_info(DWORD64 base, ULONG type_idx, IMAGEHLP_SYMBOL_TYPE_INFO property,
              void *arg)
{
    bool r = CAST_TO_bool(SymGetTypeInfo(GetCurrentProcess(), base, type_idx,
                                         property, arg));
    if (verbose && !r) {
        dr_fprintf(STDERR,
                   "drsyms: Error getting property %d of type index %d\n",
                   (int)property, (int)type_idx);
    }
    return r;
}

static drsym_error_t
decode_func_type(mempool_t *pool, DWORD64 base, ULONG type_idx,
               drsym_type_t **type_out OUT)
{
    drsym_func_type_t *func_type;
    DWORD arg_count;
    TI_FINDCHILDREN_PARAMS *children;
    uint i;
    drsym_error_t r;
    ULONG ret_type_idx;

    if (!get_type_info(base, type_idx, TI_GET_CHILDRENCOUNT, &arg_count))
        return DRSYM_ERROR;

    /* One element is included in struct, so use arg_count - 1. */
    children = POOL_ALLOC_SIZE(pool, TI_FINDCHILDREN_PARAMS,
                               (sizeof(*children) +
                                (arg_count-1) * sizeof(ULONG)));
    if (children == NULL)
        return DRSYM_ERROR_NOMEM;

    children->Count = arg_count;
    children->Start = 0;
    if (!get_type_info(base, type_idx, TI_FINDCHILDREN, children))
        return DRSYM_ERROR;

    func_type = POOL_ALLOC_SIZE(pool, drsym_func_type_t,
                              (sizeof(*func_type) +
                               (arg_count-1) * sizeof(func_type->arg_types[0])));
    if (func_type == NULL)
        return DRSYM_ERROR_NOMEM;

    func_type->type.kind = DRSYM_TYPE_FUNC;
    func_type->type.size = 0;  /* Not valid. */
    func_type->num_args = arg_count;
    if (!get_type_info(base, type_idx, TI_GET_TYPE, &ret_type_idx))
        return DRSYM_ERROR;
    r = decode_type(pool, base, ret_type_idx, &func_type->ret_type);
    if (r != DRSYM_SUCCESS)
        return r;
    for (i = 0; i < children->Count; i++) {
        r = decode_type(pool, base, children->ChildId[i], &func_type->arg_types[i]);
        if (r != DRSYM_SUCCESS)
            return r;
    }

    *type_out = &func_type->type;
    return DRSYM_SUCCESS;
}

static drsym_error_t
decode_ptr_type(mempool_t *pool, DWORD64 base, ULONG type_idx,
              drsym_type_t **type_out OUT)
{
    drsym_ptr_type_t *ptr_type;
    ULONG64 length;
    ULONG elt_type_idx;

    ptr_type = POOL_ALLOC(pool, drsym_ptr_type_t);
    if (ptr_type == NULL)
        return DRSYM_ERROR_NOMEM;
    ptr_type->type.kind = DRSYM_TYPE_PTR;
    if (!get_type_info(base, type_idx, TI_GET_LENGTH, &length))
        return DRSYM_ERROR;
    ptr_type->type.size = (size_t)length;
    if (!get_type_info(base, type_idx, TI_GET_TYPE, &elt_type_idx))
        return DRSYM_ERROR;
    *type_out = &ptr_type->type;
    /* Tail call reduces stack usage. */
    return decode_type(pool, base, elt_type_idx, &ptr_type->elt_type);
}

static drsym_error_t
decode_base_type(mempool_t *pool, DWORD64 base, ULONG type_idx,
               drsym_type_t **type_out OUT)
{
    DWORD base_type;  /* BasicType */
    bool is_signed;
    ULONG64 length;
    drsym_int_type_t *int_type;

    if (!get_type_info(base, type_idx, TI_GET_BASETYPE, &base_type))
        return DRSYM_ERROR;
    /* See if this base type is an int and if it's signed. */
    switch (base_type) {
    case btChar:
    case btWChar:
    case btUInt:
    case btBool:
    case btULong:
        is_signed = false;
    case btInt:
    case btLong:
        is_signed = true;
        break;
    default:
        return make_unknown(pool, type_out);
    }
    if (!get_type_info(base, type_idx, TI_GET_LENGTH, &length))
        return DRSYM_ERROR;
    int_type = POOL_ALLOC(pool, drsym_int_type_t);
    if (int_type == NULL)
        return DRSYM_ERROR_NOMEM;
    int_type->type.kind = DRSYM_TYPE_INT;
    int_type->type.size = (size_t)length;
    int_type->is_signed = is_signed;

    *type_out = &int_type->type;
    return DRSYM_SUCCESS;
}

static drsym_error_t
decode_typedef(mempool_t *pool, DWORD64 base, ULONG type_idx,
               drsym_type_t **type_out OUT)
{
    /* Go through typedefs. */
    ULONG base_type_idx;
    if (!get_type_info(base, type_idx, TI_GET_TYPE, &base_type_idx))
        return DRSYM_ERROR;
    return decode_type(pool, base, base_type_idx, type_out);
}

static drsym_error_t
decode_arg_type(mempool_t *pool, DWORD64 base, ULONG type_idx,
              drsym_type_t **type_out OUT)
{
    ULONG base_type_idx;
    if (!get_type_info(base, type_idx, TI_GET_TYPE, &base_type_idx))
        return DRSYM_ERROR;
    return decode_type(pool, base, base_type_idx, type_out);
}

static drsym_error_t
make_unknown(mempool_t *pool, drsym_type_t **type_out OUT)
{
    drsym_type_t *type = POOL_ALLOC(pool, drsym_type_t);
    if (type == NULL)
        return DRSYM_ERROR_NOMEM;
    type->kind = DRSYM_TYPE_OTHER;
    type->size = 0;
    *type_out = type;
    return DRSYM_SUCCESS;
}

/* Return an error code or success, store a pointer to the type created in
 * *type_out.
 */
static drsym_error_t
decode_type(mempool_t *pool, DWORD64 base, ULONG type_idx,
            drsym_type_t **type_out OUT)
{
    DWORD tag;  /* SymTagEnum */
    if (!get_type_info(base, type_idx, TI_GET_SYMTAG, &tag)) {
        return DRSYM_ERROR;
    }
    if (verbose) {
        switch (tag) {
        case SymTagFunctionType:    dr_fprintf(STDERR, "SymTagFunctionType\n"); break;
        case SymTagPointerType:     dr_fprintf(STDERR, "SymTagPointerType\n");  break;
        case SymTagBaseType:        dr_fprintf(STDERR, "SymTagBaseType\n");     break;
        case SymTagTypedef:         dr_fprintf(STDERR, "SymTagTypedef\n");      break;
        case SymTagFunctionArgType: dr_fprintf(STDERR, "SymTagFunctionArgType\n"); break;
        default:
            dr_fprintf(STDERR, "unknown: %d\n", tag);
        }
    }
    switch (tag) {
    case SymTagFunctionType:    return decode_func_type(pool, base, type_idx, type_out);
    case SymTagPointerType:     return decode_ptr_type(pool, base, type_idx, type_out);
    case SymTagBaseType:        return decode_base_type(pool, base, type_idx, type_out);
    case SymTagTypedef:         return decode_typedef(pool, base, type_idx, type_out);
    case SymTagFunctionArgType: return decode_arg_type(pool, base, type_idx, type_out);
    default:
        return make_unknown(pool, type_out);
    }
}

/***************************************************************************
 * Exported routines.
 */

DR_EXPORT
drsym_error_t
drsym_lookup_address(const char *modpath, size_t modoffs, drsym_info_t *out INOUT,
                     uint flags)
{
    if (IS_SIDELINE) {
        return DRSYM_ERROR_NOT_IMPLEMENTED;
    } else {
        return drsym_lookup_address_local(modpath, modoffs, out, flags);
    }
}

DR_EXPORT
drsym_error_t
drsym_lookup_symbol(const char *modpath, const char *symbol, size_t *modoffs OUT,
                    uint flags)
{
    if (IS_SIDELINE) {
        return DRSYM_ERROR_NOT_IMPLEMENTED;
    } else {
        return drsym_lookup_symbol_local(modpath, symbol, modoffs, flags);
    }
}

DR_EXPORT
drsym_error_t
drsym_enumerate_symbols(const char *modpath, drsym_enumerate_cb callback, void *data,
                        uint flags)
{
    if (IS_SIDELINE) {
        return DRSYM_ERROR_NOT_IMPLEMENTED;
    } else {
        return drsym_enumerate_symbols_local(modpath, NULL, callback, data, flags);
    }
}

DR_EXPORT
drsym_error_t
drsym_search_symbols(const char *modpath, const char *match, bool full,
                     drsym_enumerate_cb callback, void *data)
{
    if (IS_SIDELINE) {
        return DRSYM_ERROR_NOT_IMPLEMENTED;
    } else {
        return drsym_search_symbols_local(modpath, match, full, callback, data);
    }
}

DR_EXPORT
size_t
drsym_demangle_symbol(char *dst OUT, size_t dst_sz, const char *mangled,
                      uint flags)
{
    size_t r;
    dr_recurlock_lock(symbol_lock);
    /* Assume dbghelp is what we want unless it's Itanium "_Z" style */
    if (mangled[0] == '_' && mangled[1] == 'Z')
        r = drsym_unix_demangle_symbol(dst, dst_sz, mangled, flags);
    else
        r = demangle_symbol(dst, dst_sz, mangled, flags);
    dr_recurlock_unlock(symbol_lock);
    return r;
}

DR_EXPORT
drsym_error_t
drsym_get_func_type(const char *modpath, size_t modoffs, char *buf,
                    size_t buf_sz, drsym_func_type_t **func_type OUT)
{
    mod_entry_t *mod;
    ULONG type_index;
    mempool_t pool;
    drsym_error_t r;
    PSYMBOL_INFO info;

    if (modpath == NULL || buf == NULL || func_type == NULL)
        return DRSYM_ERROR_INVALID_PARAMETER;

    dr_recurlock_lock(symbol_lock);
    mod = lookup_or_load(modpath);
    if (mod == NULL) {
        dr_recurlock_unlock(symbol_lock);
        return DRSYM_ERROR_LOAD_FAILED;
    }
    if (mod->use_pecoff_symtable) {
        drsym_error_t symerr =
            drsym_unix_get_func_type(mod->u.pecoff_data, modoffs, buf, buf_sz,
                                       func_type);
        dr_recurlock_unlock(symbol_lock);
        return symerr;
    }

    /* XXX: For a perf boost, we could expose the concept of a
     * cursor/handle/index/whatever that refers to a given symbol and skip this
     * address lookup.  DWARF should have a similar construct we could expose.
     * However, that would break backwards compat.  We assume that the client is
     * not a debugger, and that they just want type info for a handful of
     * interesting symbols.  Therefore we can afford the overhead of the address
     * lookup.
     */
    info = alloc_symbol_info();
    if (SymFromAddr(GetCurrentProcess(), mod->u.load_base + modoffs, NULL, info)) {
        type_index = info->TypeIndex;
    } else {
        NOTIFY("SymFromAddr error %d\n", GetLastError());
        free_symbol_info(info);
        dr_recurlock_unlock(symbol_lock);
        return DRSYM_ERROR_SYMBOL_NOT_FOUND;
    }
    free_symbol_info(info);

    pool_init(&pool, buf, buf_sz);
    r = decode_type(&pool, mod->u.load_base, type_index, (drsym_type_t**)func_type);
    dr_recurlock_unlock(symbol_lock);

    if (r == DRSYM_SUCCESS && (*func_type)->type.kind != DRSYM_TYPE_FUNC)
        return DRSYM_ERROR;

    return r;
}

DR_EXPORT
drsym_error_t
drsym_get_module_debug_kind(const char *modpath, drsym_debug_kind_t *kind OUT)
{
    if (IS_SIDELINE) {
        return DRSYM_ERROR_NOT_IMPLEMENTED;
    } else {
        mod_entry_t *mod;
        drsym_error_t r;

        if (modpath == NULL || kind == NULL)
            return DRSYM_ERROR_INVALID_PARAMETER;

        dr_recurlock_lock(symbol_lock);
        mod = lookup_or_load(modpath);
        if (mod == NULL) {
            r = DRSYM_ERROR_LOAD_FAILED;
        } else if (mod->use_pecoff_symtable) {
            r = drsym_unix_get_module_debug_kind(mod->u.pecoff_data, kind);
        } else {
            if (query_available(GetCurrentProcess(), mod->u.load_base, kind)) {
                r = DRSYM_SUCCESS;
            } else {
                r = DRSYM_ERROR;
            }
        }
        dr_recurlock_unlock(symbol_lock);

        return r;
    }
}
