/* **********************************************************
 * Copyright (c) 2011 Google, Inc.  All rights reserved.
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

#include <windows.h>
#include <dbghelp.h>
#include <stdio.h> /* _vsnprintf */

/* We use the Container Extension's hashtable */
#include "hashtable.h"

#include "drsyms.h"

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

/* All dbghelp routines are un-synchronized so we provide our own synch */
void *symbol_lock;

/* Hashtable for mapping module paths to addresses */
#define MODTABLE_HASH_BITS 8
static hashtable_t modtable;

/* For debugging */
static bool verbose = false;

#undef NOTIFY /* from DrMem utils.h */
#define NOTIFY(...) do { \
    if (verbose) { \
        dr_fprintf(STDERR, __VA_ARGS__); \
    } \
} while (0)

#define ALIGN_FORWARD(x, alignment) \
    ((((ptr_uint_t)x) + ((alignment)-1)) & (~((alignment)-1)))

/* Sideline server support */
static IF_WINDOWS_ELSE(const wchar_t *, int) shmid;

#define IS_SIDELINE (shmid != 0)

#define BUFFER_SIZE_BYTES(buf)      sizeof(buf)
#define BUFFER_SIZE_ELEMENTS(buf)   (BUFFER_SIZE_BYTES(buf) / sizeof(buf[0]))
#define BUFFER_LAST_ELEMENT(buf)    buf[BUFFER_SIZE_ELEMENTS(buf) - 1]
#define NULL_TERMINATE_BUFFER(buf)  BUFFER_LAST_ELEMENT(buf) = 0

/* We assume that the DWORD64 type used by dbghelp for module base addresses
 * is fine to be truncated to a 32-bit void* for 32-bit code
 */
static DWORD64 next_load = 0x11000000;

static void
unload_module(HANDLE proc, DWORD64 base);

static void
modtable_entry_free(void *p)
{
    unload_module(GetCurrentProcess(), (DWORD64)p);
}

DR_EXPORT
drsym_error_t
drsym_init(IF_WINDOWS_ELSE(const wchar_t *, int) shmid_in)
{
    shmid = shmid_in;

    symbol_lock = dr_mutex_create();

    if (IS_SIDELINE) {
        /* FIXME NYI: establish connection with sideline server via
         * shared memory specified by shmid
         */
    } else {
        hashtable_init_ex(&modtable, MODTABLE_HASH_BITS, HASH_STRING_NOCASE,
                          true/*strdup*/, false/*!synch: using symbol_lock*/,
                          modtable_entry_free, NULL, NULL);
        
        SymSetOptions(SYMOPT_LOAD_LINES | SymGetOptions());
        if (!SymInitialize(GetCurrentProcess(), NULL, FALSE)) {
            NOTIFY("SymInitialize error %d\n", GetLastError());
            return DRSYM_ERROR;
        }
    }
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
    dr_mutex_destroy(symbol_lock);
    return res;
}

static void
query_available(HANDLE proc, DWORD64 base)
{
    IMAGEHLP_MODULE64 info; 
    memset(&info, 0, sizeof(info)); 
    info.SizeOfStruct = sizeof(info); 
    if (SymGetModuleInfo64(GetCurrentProcess(), base, &info)) {
        switch(info.SymType) {
        case SymNone: 
            NOTIFY("No symbols found\n");
            break; 
        case SymExport: 
            NOTIFY("Only export symbols found\n"); 
            break; 
        case SymPdb: 
            NOTIFY("Loaded pdb symbols from %s\n", info.LoadedPdbName);
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
        
        /* could print out info.ImageName and info.LoadedImageName 
         * and whether info.LineNumbers
         * and warn if info.PdbUnmatched or info.DbgUnmatched
         */
    }
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
        query_available(GetCurrentProcess(), base);
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

static DWORD64
lookup_or_load(const char *modpath)
{
    DWORD64 base = (DWORD64) hashtable_lookup(&modtable, (void *)modpath);
    if (base == 0) {
        base = load_module(GetCurrentProcess(), modpath);
        if (base != 0) {
            /* See comment at top of file about truncation of base being ok */
            hashtable_add(&modtable, (void *)modpath, (void *)(ptr_uint_t)base);
        }
    }
    return base;
}

static drsym_error_t
drsym_lookup_address_local(const char *modpath, size_t modoffs, drsym_info_t *out INOUT)
{
    DWORD64 base;
    char buf[sizeof(SYMBOL_INFO) +
             MAX_SYM_NAME*sizeof(TCHAR) - 1/*1 char already in Name[1] in struct*/];
    PSYMBOL_INFO info = (PSYMBOL_INFO) buf;
    DWORD64 disp;
    IMAGEHLP_LINE64 line;
    DWORD line_disp;

    if (out == NULL)
        return DRSYM_ERROR_INVALID_PARAMETER;
    /* If we add fields in the future we would dispatch on out->struct_size */
    if (out->struct_size != sizeof(*out))
        return DRSYM_ERROR_INVALID_SIZE;
    
    dr_mutex_lock(symbol_lock);
    base = lookup_or_load(modpath);
    if (base == 0) {
        dr_mutex_unlock(symbol_lock);
        return DRSYM_ERROR_LOAD_FAILED;
    }

    info->SizeOfStruct = sizeof(SYMBOL_INFO);
    info->MaxNameLen = MAX_SYM_NAME;
    if (SymFromAddr(GetCurrentProcess(), base + modoffs, &disp, info)) {
        NOTIFY("Symbol 0x%I64x => %s+0x%x (0x%I64x-0x%I64x)\n", base+modoffs, info->Name,
               disp, info->Address, info->Address + info->Size);
        out->start_offs = (size_t) (info->Address - base);
        out->end_offs = (size_t) ((info->Address + info->Size) - base);
        strncpy(out->name, info->Name, out->name_size);
        out->name[out->name_size - 1] = '\0';
        /* Should we return an error when name gets truncated? */
        out->name_available_size = info->NameLen*sizeof(char);
    } else {
        NOTIFY("SymFromAddr error %d\n", GetLastError());
        dr_mutex_unlock(symbol_lock);
        return DRSYM_ERROR_SYMBOL_NOT_FOUND;
    }

    line.SizeOfStruct = sizeof(line);
    if (SymGetLineFromAddr64(GetCurrentProcess(), base + modoffs, &line_disp, &line)) {
        NOTIFY("%s:%u+0x%x\n", line.FileName, line.LineNumber, line_disp);
        /* We assume that line.FileName has a permanent lifetime */
        out->file = line.FileName;
        out->line = line.LineNumber;
        out->line_offs = line_disp;
    } else {
        NOTIFY("SymGetLineFromAddr64 error %d\n", GetLastError());
        dr_mutex_unlock(symbol_lock);
        return DRSYM_ERROR_LINE_NOT_AVAILABLE;
    }
    dr_mutex_unlock(symbol_lock);
    return DRSYM_SUCCESS;
}

static drsym_error_t
drsym_lookup_symbol_local(const char *modpath, const char *symbol, size_t *modoffs OUT)
{
    DWORD64 base;
    char buf[sizeof(SYMBOL_INFO) +
             MAX_SYM_NAME*sizeof(TCHAR) - 1/*1 char already in Name[1] in struct*/];
    PSYMBOL_INFO info = (PSYMBOL_INFO) buf;

    if (modoffs == NULL)
        return DRSYM_ERROR_INVALID_PARAMETER;
    
    dr_mutex_lock(symbol_lock);
    base = lookup_or_load(modpath);
    if (base == 0) {
        dr_mutex_unlock(symbol_lock);
        return DRSYM_ERROR_LOAD_FAILED;
    }

    /* the only thing identifying the target module is the symbol name,
     * which should be of "modname!symname" format
     */
    info->SizeOfStruct = sizeof(SYMBOL_INFO);
    info->MaxNameLen = MAX_SYM_NAME;
    if (SymFromName(GetCurrentProcess(), (char *)symbol, info)) {
        NOTIFY("0x%I64x\n", info->Address);
        *modoffs = (size_t) (info->Address - base);
    } else {
        NOTIFY("SymFromName error %d %s\n", GetLastError(), symbol);
        dr_mutex_unlock(symbol_lock);
        return DRSYM_ERROR_SYMBOL_NOT_FOUND;
    }
    dr_mutex_unlock(symbol_lock);
    return DRSYM_SUCCESS;
}

typedef struct _enum_info_t {
    drsym_enumerate_cb cb;
    void *data;
    DWORD64 base;
} enum_info_t;

static BOOL CALLBACK
enum_cb(PSYMBOL_INFO pSymInfo, ULONG SymbolSize, PVOID Context) 
{
    enum_info_t *info = (enum_info_t *) Context;
    return (BOOL) (*info->cb)(pSymInfo->Name, (size_t) (pSymInfo->Address - info->base),
                              info->data);
}

static drsym_error_t
drsym_enumerate_symbols_local(const char *modpath, const char *match,
                              drsym_enumerate_cb callback, void *data)
{
    DWORD64 base;
    drsym_error_t res = DRSYM_SUCCESS;
    dr_mutex_lock(symbol_lock);
    base = lookup_or_load(modpath);
    if (base == 0)
        res = DRSYM_ERROR_LOAD_FAILED;
    else {
        enum_info_t info;
        info.cb = callback;
        info.data = data;
        info.base = base;
        if (!SymEnumSymbols(GetCurrentProcess(), base, match, enum_cb, (PVOID) &info)) {
            NOTIFY("SymEnumSymbols error %d\n", GetLastError());
            res = DRSYM_ERROR_SYMBOL_NOT_FOUND;
        }
    }
    dr_mutex_unlock(symbol_lock);
    return res;
}

/* SymSearch (w/ default flags) is much faster than SymEnumSymbols or even
 * SymFromName so we export it separately for Windows (Dr. Memory i#313).
 */
static drsym_error_t
drsym_search_symbols_local(const char *modpath, const char *match, bool full,
                           drsym_enumerate_cb callback, void *data)
{
    DWORD64 base;
    drsym_error_t res = DRSYM_SUCCESS;
    /* dbghelp.dll 6.3+ is required for SymSearch, but the VS2005sp1
     * headers and lib have only 6.1, so we dynamically look it up
     */
    static func_SymSearch_t func;

    dr_mutex_lock(symbol_lock);
    if (func == NULL) {
        /* if we fail to find it we'll pay the lookup cost every time,
         * but if we succeed we'll cache it
         */
        HMODULE hmod = GetModuleHandle("dbghelp.dll");
        if (hmod == NULL) {
            dr_mutex_unlock(symbol_lock);
            /* fall back to slower enum */
            return drsym_enumerate_symbols_local(modpath, match, callback, data);
        }
        func = (func_SymSearch_t) GetProcAddress(hmod, "SymSearch");
        if (func == NULL) {
            dr_mutex_unlock(symbol_lock);
            /* fall back to slower enum */
            return drsym_enumerate_symbols_local(modpath, match, callback, data);
        }
    }
    base = lookup_or_load(modpath);
    if (base == 0)
        res = DRSYM_ERROR_LOAD_FAILED;
    else {
        enum_info_t info;
        info.cb = callback;
        info.data = data;
        info.base = base;
        if (!(*func)(GetCurrentProcess(), base, 0, 0, match, 0,
                     enum_cb, (PVOID) &info,
                     full ? SYMSEARCH_ALLITEMS : 0)) {
            NOTIFY("SymSearch error %d\n", GetLastError());
            res = DRSYM_ERROR_SYMBOL_NOT_FOUND;
        }
    }
    dr_mutex_unlock(symbol_lock);
    return res;
}

DR_EXPORT
drsym_error_t
drsym_lookup_address(const char *modpath, size_t modoffs, drsym_info_t *out INOUT)
{
    if (IS_SIDELINE) {
        return DRSYM_ERROR_NOT_IMPLEMENTED;
    } else {
        return drsym_lookup_address_local(modpath, modoffs, out);
    }
}

DR_EXPORT
drsym_error_t
drsym_lookup_symbol(const char *modpath, const char *symbol, size_t *modoffs OUT)
{
    if (IS_SIDELINE) {
        return DRSYM_ERROR_NOT_IMPLEMENTED;
    } else {
        return drsym_lookup_symbol_local(modpath, symbol, modoffs);
    }
}

DR_EXPORT
drsym_error_t
drsym_enumerate_symbols(const char *modpath, drsym_enumerate_cb callback, void *data)
{
    if (IS_SIDELINE) {
        return DRSYM_ERROR_NOT_IMPLEMENTED;
    } else {
        return drsym_enumerate_symbols_local(modpath, NULL, callback, data);
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


/***************************************************************************/

/* Convenience routines to work around i#261.  Perhaps cleaner in a
 * separate library but putting here for now since we're already using
 * kernel32 and I prefer to use shared libs for Extensions that import
 * from Windows libs: and more efficient to use fewer shared libs.
 */

DR_EXPORT
bool
drsym_using_console(void)
{
    /* We detect cmd window using what kernel32!WriteFile uses: a handle
     * having certain bits set.
     */
    return (((ptr_int_t)GetStdHandle(STD_ERROR_HANDLE) & 0x10000003) == 0x3);
}

DR_EXPORT
bool
drsym_write_to_console(const char *fmt, ...)
{
    bool res = true;
    /* mirroring DR's MAX_LOG_LEN */
# define MAX_MSG_LEN 768
    char msg[MAX_MSG_LEN];
    va_list ap;
    uint written;
    int len;
    va_start(ap, fmt);
    /* DR should provide vsnprintf: i#168 */
    len = _vsnprintf(msg, BUFFER_SIZE_ELEMENTS(msg), fmt, ap);
    /* Let user know if message was truncated */
    if (len < 0 || len == BUFFER_SIZE_ELEMENTS(msg))
        res = false;
    NULL_TERMINATE_BUFFER(msg);
    /* Make this routine work in all kinds of windows by going through
     * kernel32!WriteFile, which will call WriteConsole for us.
     */
    res = res &&
        WriteFile(GetStdHandle(STD_ERROR_HANDLE),
                  msg, (DWORD) strlen(msg), (LPDWORD) &written, NULL);
    va_end(ap);
    return res;
}

