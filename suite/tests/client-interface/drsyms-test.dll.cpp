/* **********************************************************
 * Copyright (c) 2011 Google, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Tests the drsyms extension.  Relies on the drwrap extension. */

#include "dr_api.h"
#include "drsyms.h"
#include "drwrap.h"
#include "client_tools.h"

#include <string.h>

/* DR's build system usually disables warnings we're not interested in, but the
 * flags don't seem to make it to the compiler for this file, maybe because
 * it's C++.  Disable them with pragmas instead.
 */
#ifdef WINDOWS
# pragma warning( disable : 4100 )  /* Unreferenced formal parameter. */
#endif

#ifdef WINDOWS
# define EXE_SUFFIX ".exe"
#else
# define EXE_SUFFIX
#endif

static void event_exit(void);
static void lookup_exe_syms(void);
static void lookup_dll_syms(void *dc, const module_data_t *dll_data,
                            bool loaded);
static void check_enumerate_dll_syms(void *dc, const char *dll_path);
static bool enum_sym_cb(const char *name, size_t modoffs, void *data);
#ifdef LINUX
static void lookup_glibc_syms(void *dc, const module_data_t *dll_data);
#endif

extern "C" DR_EXPORT void
dr_init(client_id_t id)
{
    drsym_init(0);
    drwrap_init();
    dr_register_exit_event(event_exit);

    lookup_exe_syms();
    dr_register_module_load_event(lookup_dll_syms);
}

/* Count intercepted calls. */
static int call_count = 0;

static void
pre_func(void *wrapcxt, void **user_data)
{
    call_count++;
}

/* Assuming prologue has "push xbp, mov xsp -> xbp", this struct is at the base
 * of every frame.
 */
typedef struct _frame_base_t {
    struct _frame_base_t *parent;
    app_pc ret_addr;
} frame_base_t;

#define MAX_FUNC_LEN 1024
/* Take and symbolize a stack trace.  Assumes no frame pointer omission.
 */
static void
pre_stack_trace(void *wrapcxt, void **user_data)
{
    dr_mcontext_t *mc = drwrap_get_mcontext(wrapcxt);
    frame_base_t inner_frame;
    frame_base_t *frame;
    int depth;

    /* This should use safe_read and all that, but this is a test case. */
    dr_fprintf(STDERR, "stack trace:\n");
    frame = &inner_frame;
    frame->parent = (frame_base_t*)mc->xbp;
    frame->ret_addr = *(app_pc*)mc->xsp;
    depth = 0;
    while (frame != NULL) {
        drsym_error_t r;
        module_data_t *mod;
        drsym_info_t *sym_info;
        char sbuf[sizeof(*sym_info) + MAX_FUNC_LEN];
        size_t modoffs;
        const char *basename;

        sym_info = (drsym_info_t *)sbuf;
        sym_info->struct_size = sizeof(*sym_info);
        sym_info->name_size = MAX_FUNC_LEN;

        mod = dr_lookup_module(frame->ret_addr);
        modoffs = frame->ret_addr - mod->start;
        r = drsym_lookup_address(mod->full_path, modoffs, sym_info);
        dr_free_module_data(mod);
        ASSERT(r == DRSYM_SUCCESS);
        basename = (sym_info->file ?
                    strrchr(sym_info->file, IF_WINDOWS_ELSE('\\', '/')) :
                    "/<unknown>");
        ASSERT(basename != NULL);
        basename++;
        dr_fprintf(STDERR, "%s:%d!%s\n",
                   basename, (int)sym_info->line, sym_info->name);

        /* Stop after main. */
        if (strstr(sym_info->name, "main"))
            break;

        frame = frame->parent;
        depth++;
        if (depth > 20) {
            dr_fprintf(STDERR, "20 frames deep, stopping trace.\n");
            break;
        }
    }
}

static void
post_func(void *wrapcxt, void *user_data)
{
}

/* Use dr_get_proc_addr to get the exported address of a symbol.  Attempt to
 * look through any export table jumps so that we get the address for the
 * symbol that would be returned by looking at debug information.
 */
static app_pc
get_real_proc_addr(void *mod_handle, const char *symbol)
{
    instr_t instr;
    app_pc next_pc = NULL;
    app_pc export_addr = (app_pc)dr_get_proc_address(mod_handle, symbol);
    void *dc = dr_get_current_drcontext();

    instr_init(dc, &instr);
    if (export_addr != NULL)
        next_pc = decode(dc, export_addr, &instr);
    if (next_pc != NULL && instr_is_ubr(&instr)) {
        /* This is a jump to the real function entry point. */
        export_addr = opnd_get_pc(instr_get_target(&instr));
    }
    instr_reset(dc, &instr);

    return export_addr;
}

/* Lookup symbols in the exe and wrap them. */
static void
lookup_exe_syms(void)
{
    drsym_error_t r;
    module_data_t *exe_data;
    const char *exe_path;
    app_pc exe_base;
    app_pc exe_export_addr;
    size_t exe_export_offs;
    size_t exe_static_offs;
    bool ok;
    drsym_info_t unused_info;

    exe_data = dr_lookup_module_by_name("client.drsyms-test" EXE_SUFFIX);
    ASSERT(exe_data != NULL);
    exe_path = exe_data->full_path;
    exe_base = exe_data->start;

    exe_export_addr = get_real_proc_addr(exe_data->handle, "exe_export");
    exe_export_offs = 0;
    r = drsym_lookup_symbol(exe_path, "client.drsyms-test!exe_export",
                            &exe_export_offs);
    ASSERT(r == DRSYM_SUCCESS && exe_export_offs != 0);
    ASSERT(exe_export_addr == exe_base + exe_export_offs);
    ok = drwrap_wrap(exe_export_addr, pre_func, post_func);
    ASSERT(ok);

    /* exe_static is a static function in the exe we wouldn't be able to find
     * without drsyms and debug info.
     */
    r = drsym_lookup_symbol(exe_path, "client.drsyms-test!exe_static",
                            &exe_static_offs);
    ASSERT(r == DRSYM_SUCCESS);
    ok = drwrap_wrap(exe_base + exe_static_offs, pre_func, post_func);
    ASSERT(ok);

    /* Test symbol not found error handling. */
    r = drsym_lookup_symbol(exe_path, "nonexistant_sym", &exe_static_offs);
    ASSERT(r == DRSYM_ERROR_SYMBOL_NOT_FOUND);

    /* Test invalid parameter errors. */
    r = drsym_lookup_symbol(NULL, "malloc", &exe_static_offs);
    ASSERT(r == DRSYM_ERROR_INVALID_PARAMETER);
    r = drsym_lookup_symbol(exe_path, NULL, &exe_static_offs);
    ASSERT(r == DRSYM_ERROR_INVALID_PARAMETER);
    r = drsym_enumerate_symbols(exe_path, NULL, NULL);
    ASSERT(r == DRSYM_ERROR_INVALID_PARAMETER);
    r = drsym_lookup_address(NULL, 0xDEADBEEFUL, &unused_info);
    ASSERT(r == DRSYM_ERROR_INVALID_PARAMETER);

    /* FIXME: Lookup C++ symbols and do demangling. */

    /* FIXME: Test glob matching. */

    /* FIXME: Test looking up malloc in libc.  libc's .gnu_debuglink section
     * relies on searching paths other than the current directory.
     */

    dr_free_module_data(exe_data);
}

/* Lookup symbols in the appdll and wrap them. */
static void
lookup_dll_syms(void *dc, const module_data_t *dll_data, bool loaded)
{
    const char *dll_path;
    app_pc dll_base;
    app_pc dll_export_addr;
    size_t dll_export_offs;
    size_t dll_static_offs;
    size_t stack_trace_offs;
    drsym_error_t r;
    bool ok;

    dll_path = dll_data->full_path;
    dll_base = dll_data->start;

#ifdef LINUX
    if (strstr(dll_path, "/libc-")) {
        lookup_glibc_syms(dc, dll_data);
        return;
    }
#endif

    /* Avoid running on any module other than the appdll. */
    if (!strstr(dll_path, "appdll"))
        return;

    dll_export_addr = get_real_proc_addr(dll_data->handle, "dll_export");
    dll_export_offs = 0;
    r = drsym_lookup_symbol(dll_path, "client.drsyms-test.appdll!dll_export",
                            &dll_export_offs);
    ASSERT(r == DRSYM_SUCCESS && dll_export_offs != 0);
    ASSERT(dll_export_addr == dll_base + dll_export_offs);
    ok = drwrap_wrap(dll_export_addr, pre_func, post_func);
    ASSERT(ok);

    /* dll_static is a static function in the dll we wouldn't be able to find
     * without drsyms and debug info.
     */
    r = drsym_lookup_symbol(dll_path, "client.drsyms-test.appdll!dll_static",
                            &dll_static_offs);
    ASSERT(r == DRSYM_SUCCESS);
    ok = drwrap_wrap(dll_base + dll_static_offs, pre_func, post_func);
    ASSERT(ok);

    /* stack_trace is a static function in the DLL that we use to get PCs of all
     * the functions we've looked up so far.
     */
    r = drsym_lookup_symbol(dll_path, "client.drsyms-test.appdll!stack_trace",
                            &stack_trace_offs);
    ASSERT(r == DRSYM_SUCCESS);
    ok = drwrap_wrap(dll_base + stack_trace_offs, pre_stack_trace, post_func);
    ASSERT(ok);

    check_enumerate_dll_syms(dc, dll_path);
}

typedef struct _dll_syms_found_t {
    bool dll_export_found;
    bool dll_static_found;
    bool stack_trace_found;
} dll_syms_found_t;

/* Enumerate all symbols in the dll and verify that we at least find the ones
 * we expected to be there.
 */
static void
check_enumerate_dll_syms(void *dc, const char *dll_path)
{
    dll_syms_found_t syms_found;
    drsym_error_t r;
    memset(&syms_found, 0, sizeof(syms_found));
    r = drsym_enumerate_symbols(dll_path, enum_sym_cb, &syms_found);
    ASSERT(r == DRSYM_SUCCESS);
    ASSERT(syms_found.dll_export_found &&
           syms_found.dll_static_found &&
           syms_found.stack_trace_found);
}

static bool
enum_sym_cb(const char *name, size_t modoffs, void *data)
{
    dll_syms_found_t *syms_found = (dll_syms_found_t*)data;
    if (strstr(name, "dll_export"))
        syms_found->dll_export_found = true;
    if (strstr(name, "dll_static"))
        syms_found->dll_static_found = true;
    if (strstr(name, "stack_trace"))
        syms_found->stack_trace_found = true;
    return true;
}

#ifdef LINUX
/* Test if we can look up glibc symbols.  This only works if the user is using
 * glibc (and not some other libc) and is has debug info installed for it, so we
 * avoid making assertions if we can't find the symbols.  The purpose of this
 * test is really to see if we can follow the .gnu_debuglink section into
 * /usr/lib/debug/$mod_dir/$debuglink.
 */
static void
lookup_glibc_syms(void *dc, const module_data_t *dll_data)
{
    const char *libc_path;
    app_pc libc_base;
    size_t malloc_offs;
    size_t gi_malloc_offs;
    drsym_error_t r;

    /* i#479: DR loads a private copy of libc.  The result should be the same
     * both times, so avoid running twice.
     */
    static bool already_called = false;
    if (already_called) {
        return;
    }
    already_called = true;

    libc_path = dll_data->full_path;
    libc_base = dll_data->start;

    /* FIXME: When drsyms can read .dynsym we should always find malloc. */
    malloc_offs = 0;
    r = drsym_lookup_symbol(libc_path, "libc!malloc", &malloc_offs);
    if (r == DRSYM_SUCCESS)
        ASSERT(malloc_offs != 0);

    /* __GI___libc_malloc is glibc's internal reference to malloc.  They use
     * these internal symbols so that glibc calls to exported functions are
     * never pre-empted by other libraries.
     */
    gi_malloc_offs = 0;
    r = drsym_lookup_symbol(libc_path, "libc!__GI___libc_malloc",
                            &gi_malloc_offs);
    /* We can't compare the offsets because the exported offset and internal
     * offset are probably going to be different.
     */
    if (r == DRSYM_SUCCESS)
        ASSERT(gi_malloc_offs != 0);

    if (malloc_offs != 0 && gi_malloc_offs != 0) {
        dr_fprintf(STDERR, "found glibc malloc and __GI___libc_malloc.\n");
    } else {
        dr_fprintf(STDERR, "couldn't find glibc malloc or __GI___libc_malloc.\n");
    }
}
#endif /* LINUX */

static void
event_exit(void)
{
    drwrap_exit();
    drsym_exit();
    /* Check that all symbols we looked up got called. */
    ASSERT(call_count == 4);
    dr_fprintf(STDERR, "all done\n");
}
