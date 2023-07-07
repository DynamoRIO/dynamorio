/* **********************************************************
 * Copyright (c) 2011-2023 Google, Inc.  All rights reserved.
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

#include <limits.h>
#include <string.h>

/* DR's build system usually disables warnings we're not interested in, but the
 * flags don't seem to make it to the compiler for this file, maybe because
 * it's C++.  Disable them with pragmas instead.
 */
#ifdef WINDOWS
#    pragma warning(disable : 4100) /* Unreferenced formal parameter. */
#endif

static void
event_exit(void);
static void
lookup_exe_syms(void);
static void
lookup_dll_syms(void *dc, const module_data_t *dll_data, bool loaded);
static void
check_enumerate_dll_syms(const char *dll_path);
#ifdef UNIX
static void
lookup_glibc_syms(void *dc, const module_data_t *dll_data);
#endif
static void
test_demangle(void);
#ifdef WINDOWS
static void
lookup_overloads(const char *exe_path);
static void
lookup_templates(const char *exe_path);
static void
lookup_type_by_name(const char *exe_path);
#endif

#ifdef WINDOWS
static dr_os_version_info_t os_version = {
    sizeof(os_version),
};
#endif

static bool found_tools_h, found_appdll;

extern "C" DR_EXPORT void
dr_init(client_id_t id)
{
    drsym_error_t r = drsym_init(0);
    ASSERT(r == DRSYM_SUCCESS);
    drwrap_init();
    dr_register_exit_event(event_exit);

    lookup_exe_syms();
    dr_register_module_load_event(lookup_dll_syms);
    test_demangle();

#ifdef WINDOWS
    if (!dr_get_os_version(&os_version))
        ASSERT(false);
#endif
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

#ifdef WINDOWS
#    define FULL_PDB_DEBUG_KIND (DRSYM_SYMBOLS | DRSYM_LINE_NUMS | DRSYM_PDB)
#    define FULL_PECOFF_DEBUG_KIND \
        (DRSYM_SYMBOLS | DRSYM_LINE_NUMS | DRSYM_PECOFF_SYMTAB | DRSYM_DWARF_LINE)
#else
#    define FULL_DEBUG_KIND \
        (DRSYM_SYMBOLS | DRSYM_LINE_NUMS | DRSYM_ELF_SYMTAB | DRSYM_DWARF_LINE)
#endif

static bool
debug_kind_is_full(drsym_debug_kind_t debug_kind)
{
    return
#ifdef WINDOWS
        TESTALL(FULL_PDB_DEBUG_KIND, debug_kind) ||
        TESTALL(FULL_PECOFF_DEBUG_KIND, debug_kind);
#else
        TESTALL(FULL_DEBUG_KIND, debug_kind);
#endif
}

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
    /* It's impossible to get frame pointers on Win x64, so we only print one
     * frame.
     */
#if (defined(WINDOWS) && defined(X64)) || defined(AARCHXX)
    frame->parent = NULL;
#else
    frame->parent = (frame_base_t *)mc->xbp;
#endif
#ifdef X86
    frame->ret_addr = *(app_pc *)mc->xsp;
#elif defined(ARM)
    /* clear the least significant bit if thumb mode */
    frame->ret_addr = dr_app_pc_as_load_target(DR_ISA_ARM_THUMB, (app_pc)mc->lr);
#elif defined(AARCH64)
    frame->ret_addr = (app_pc)mc->lr;
#endif
    depth = 0;
    while (frame != NULL) {
        drsym_error_t r;
        module_data_t *mod;
        drsym_info_t sym_info;
        char name[MAX_FUNC_LEN];
        char file[MAXIMUM_PATH];
        size_t modoffs;
        const char *basename;

        sym_info.struct_size = sizeof(sym_info);
        sym_info.name = name;
        sym_info.name_size = MAX_FUNC_LEN;
        sym_info.file = file;
        sym_info.file_size = MAXIMUM_PATH;

        mod = dr_lookup_module(frame->ret_addr);
        modoffs = frame->ret_addr - mod->start;
        /* gcc says the next line starts at the return address.  Back up one to
         * get the line that the call was on.
         */
        modoffs--;
        r = drsym_lookup_address(mod->full_path, modoffs, &sym_info, DRSYM_DEMANGLE);
        dr_free_module_data(mod);
        ASSERT(r == DRSYM_SUCCESS);
        if (!debug_kind_is_full(sym_info.debug_kind)) {
            dr_fprintf(STDERR, "unexpected debug_kind: %x\n", sym_info.debug_kind);
        }
        if (sym_info.file_available_size == 0)
            basename = "/<unknown>";
        else {
            basename = strrchr(sym_info.file, IF_WINDOWS_ELSE('\\', '/'));
#ifdef WINDOWS
            if (basename == NULL)
                basename = strrchr(sym_info.file, '/');
#endif
        }
        ASSERT(basename != NULL);
        basename++;
        dr_fprintf(STDERR, "%s:%d!%s\n", basename, (int)sym_info.line, sym_info.name);

        /* Stop after main. */
        if (strstr(sym_info.name, "main"))
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
get_real_proc_addr(module_handle_t mod_handle, const char *symbol)
{
    instr_t instr;
    app_pc next_pc = NULL;
    app_pc export_addr = (app_pc)dr_get_proc_address(mod_handle, symbol);
    void *dc = dr_get_current_drcontext();

#ifdef WINDOWS
    /* use a symbol forwarded by DR to ntdll that's not an intrinsic to test
     * duplicate link issues vs libcmt
     */
    if (isdigit(symbol[0]))
        next_pc = next_pc; /* avoid warning about empty statement */
#endif

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

static size_t
lookup_and_wrap(const char *modpath, app_pc modbase, const char *modname,
                const char *symbol, uint flags)
{
    drsym_error_t r;
    size_t modoffs = 0;
    bool ok;
    char lookup_str[256];

    dr_snprintf(lookup_str, sizeof(lookup_str), "%s!%s", modname, symbol);
    r = drsym_lookup_symbol(modpath, lookup_str, &modoffs, flags);
    if (r != DRSYM_SUCCESS || modoffs == 0) {
        dr_fprintf(STDERR, "Failed to lookup %s => %d\n", lookup_str, r);
    } else {
        ok = drwrap_wrap(modbase + modoffs, pre_func, post_func);
        ASSERT(ok);
    }
    return modoffs;
}

/* Lookup symbols in the exe and wrap them. */
static void
lookup_exe_syms(void)
{
    module_data_t *exe_data;
    const char *exe_path;
    app_pc exe_base;
    app_pc exe_export_addr;
    size_t exe_export_offs;
    size_t exe_public_offs;
    drsym_info_t unused_info;
    drsym_error_t r;
    drsym_debug_kind_t debug_kind;
    const char *appname = dr_get_application_name();
    char appbase[MAXIMUM_PATH];
    size_t len;

    len = dr_snprintf(appbase, BUFFER_SIZE_ELEMENTS(appbase), "%s", appname);
    ASSERT(len > 0);
#ifdef WINDOWS
    /* blindly assuming ends in .exe */
    appbase[strlen(appname) - 4 /*subtract .exe*/] = '\0';
#endif

    exe_data = dr_lookup_module_by_name(appname);
    ASSERT(exe_data != NULL);
    exe_path = exe_data->full_path;
    exe_base = exe_data->start;

    /* We expect to have full debug info for this module. */
    r = drsym_get_module_debug_kind(exe_path, &debug_kind);
    ASSERT(r == DRSYM_SUCCESS);
    if (!debug_kind_is_full(debug_kind)) {
        dr_fprintf(STDERR, "unexpected debug_kind: %x\n", debug_kind);
    }

    exe_export_addr = get_real_proc_addr(exe_data->handle, "exe_export");
    exe_export_offs =
        lookup_and_wrap(exe_path, exe_base, appbase, "exe_export", DRSYM_DEFAULT_FLAGS);
    ASSERT(exe_export_addr == exe_base + exe_export_offs);

    /* exe_public is a function in the exe we wouldn't be able to find without
     * drsyms and debug info.
     */
    (void)lookup_and_wrap(exe_path, exe_base, appbase, "exe_public", DRSYM_DEFAULT_FLAGS);

    /* Test symbol not found error handling. */
    r = drsym_lookup_symbol(exe_path, "nonexistent_sym", &exe_public_offs,
                            DRSYM_DEFAULT_FLAGS);
    ASSERT(r == DRSYM_ERROR_SYMBOL_NOT_FOUND);

    /* Test invalid parameter errors. */
    r = drsym_lookup_symbol(NULL, "malloc", &exe_public_offs, DRSYM_DEFAULT_FLAGS);
    ASSERT(r == DRSYM_ERROR_INVALID_PARAMETER);
    r = drsym_lookup_symbol(exe_path, NULL, &exe_public_offs, DRSYM_DEFAULT_FLAGS);
    ASSERT(r == DRSYM_ERROR_INVALID_PARAMETER);
    r = drsym_enumerate_symbols(exe_path, NULL, NULL, DRSYM_DEFAULT_FLAGS);
    ASSERT(r == DRSYM_ERROR_INVALID_PARAMETER);
    r = drsym_lookup_address(NULL, 0xDEADBEEFUL, &unused_info, DRSYM_DEFAULT_FLAGS);
    ASSERT(r == DRSYM_ERROR_INVALID_PARAMETER);

#ifdef WINDOWS
    if (TEST(DRSYM_PDB, debug_kind)) { /* else NYI */
        lookup_overloads(exe_path);
        lookup_templates(exe_path);
        /* Test drsym_get_type_by_name function */
        lookup_type_by_name(exe_path);
    }
#endif

    dr_free_module_data(exe_data);
}

#ifdef WINDOWS
#    define NUM_OVERLOADED_CLASS 3
typedef struct _overloaded_params_t {
    const char *exe_path;
    bool overloaded_char;
    bool overloaded_wchar;
    bool overloaded_int;
    bool overloaded_void_ptr;
    bool overloaded_void;
    uint overloaded_class;
} overloaded_params_t;

static bool
overloaded_cb(const char *name, size_t modoffs, void *data)
{
    overloaded_params_t *p = (overloaded_params_t *)data;
    drsym_func_type_t *func_type;
    static char buf[4096];
    drsym_error_t r;

    if (strcmp(name, "overloaded") != 0)
        return true;
    r = drsym_get_func_type(p->exe_path, modoffs, buf, sizeof(buf), &func_type);
    if (r != DRSYM_SUCCESS) {
        dr_fprintf(STDERR, "drsym_get_func_type failed: %d\n", (int)r);
        return true;
    }
    if (func_type->num_args == 1 && func_type->arg_types[0]->kind == DRSYM_TYPE_PTR) {
        drsym_ptr_type_t *arg_type = (drsym_ptr_type_t *)func_type->arg_types[0];
        size_t arg_int_size = arg_type->elt_type->size;
        if (arg_type->elt_type->kind == DRSYM_TYPE_INT) {
            switch (arg_int_size) {
            case 1: p->overloaded_char = true; break;
            case 2: p->overloaded_wchar = true; break;
            case 4: p->overloaded_int = true; break;
            default: break;
            }
        } else if (arg_type->elt_type->kind == DRSYM_TYPE_VOID) {
            p->overloaded_void_ptr = true;
        } else if (arg_type->elt_type->kind == DRSYM_TYPE_COMPOUND) {
            drsym_compound_type_t *ctype = (drsym_compound_type_t *)arg_type->elt_type;
            int i;
            ASSERT(ctype->field_types == NULL); /* drsym_get_func_type does not expand */
            p->overloaded_class++;
            dr_fprintf(STDERR, "compound arg %s has %d field(s), size %d\n", ctype->name,
                       ctype->num_fields, ctype->type.size);
            r = drsym_expand_type(p->exe_path, ctype->type.id, UINT_MAX, buf, sizeof(buf),
                                  (drsym_type_t **)&ctype);
            if (r != DRSYM_SUCCESS) {
                dr_fprintf(STDERR, "drsym_expand_type failed: %d\n", (int)r);
            } else {
                ASSERT(ctype->type.kind == DRSYM_TYPE_COMPOUND);
                for (i = 0; i < ctype->num_fields; i++) {
                    dr_fprintf(STDERR, "  class field %d is type %d and size %d\n", i,
                               ctype->field_types[i]->kind, ctype->field_types[i]->size);
                    if (ctype->field_types[i]->kind == DRSYM_TYPE_FUNC) {
                        drsym_func_type_t *ftype =
                            (drsym_func_type_t *)ctype->field_types[i];
                        dr_fprintf(STDERR, "    func has %d args\n", ftype->num_args);
                        for (i = 0; i < ftype->num_args; i++) {
                            dr_fprintf(STDERR, "      arg %d is type %d and size %d\n", i,
                                       ftype->arg_types[i]->kind,
                                       ftype->arg_types[i]->size);
                        }
                    }
                }
            }
        } else {
            dr_fprintf(STDERR, "overloaded() arg has unexpected type!\n");
        }
    } else if (func_type->num_args == 0) {
        /* no arg so not really an overload, but we need to test no-arg func */
        p->overloaded_void = true;
    } else {
        dr_fprintf(STDERR, "overloaded() has unexpected args\n");
    }

    return true;
}

static void
lookup_overloads(const char *exe_path)
{
    overloaded_params_t p;
    drsym_error_t r;

    p.exe_path = exe_path;
    p.overloaded_char = false;
    p.overloaded_wchar = false;
    p.overloaded_int = false;
    p.overloaded_void = false;
    p.overloaded_void_ptr = false;
    p.overloaded_class = 0;
    r = drsym_enumerate_symbols(exe_path, overloaded_cb, &p, DRSYM_DEFAULT_FLAGS);
    ASSERT(r == DRSYM_SUCCESS);
    if (!p.overloaded_char)
        dr_fprintf(STDERR, "overloaded_char missing!\n");
    if (!p.overloaded_wchar)
        dr_fprintf(STDERR, "overloaded_wchar missing!\n");
    if (!p.overloaded_int)
        dr_fprintf(STDERR, "overloaded_int missing!\n");
    if (!p.overloaded_void)
        dr_fprintf(STDERR, "overloaded_void missing!\n");
    if (!p.overloaded_void_ptr)
        dr_fprintf(STDERR, "overloaded_void_ptr missing!\n");
    if (p.overloaded_class != NUM_OVERLOADED_CLASS)
        dr_fprintf(STDERR, "overloaded_class missing!\n");
    if (p.overloaded_char && p.overloaded_wchar && p.overloaded_int &&
        p.overloaded_void && p.overloaded_void_ptr &&
        p.overloaded_class == NUM_OVERLOADED_CLASS) {
        dr_fprintf(STDERR, "found all overloads\n");
    }
}

static bool
search_templates_cb(const char *name, size_t modoffs, void *data)
{
    /* See below about i#1376 and unnamed-tag */
    if (strstr(name, "::templated_func") != NULL)
        dr_fprintf(STDERR, "found %s\n", name);
    return true;
}

static bool
search_ex_templates_cb(drsym_info_t *out, drsym_error_t status, void *data)
{
    /* i#1376: VS2013 PDB seems to not have qualified unnamed-tag entries so
     * in the interests of cross-platform non-flaky tests we don't
     * print them out anymore.  We're talking about this:
     *   name_outer::name_middle::name_inner::sample_class<char>::nested_class<int>::
     *   <unnamed-tag>
     */
    if (strstr(out->name, "::templated_func") != NULL)
        dr_fprintf(STDERR, "found %s\n", out->name);
    return true;
}

static void
lookup_templates(const char *exe_path)
{
    /* These should collapse the templates */
    drsym_error_t r =
        drsym_search_symbols(exe_path, "*!*nested*", true, search_templates_cb, NULL);
    ASSERT(r == DRSYM_SUCCESS);
    r = drsym_search_symbols_ex(exe_path, "*!*nested*",
                                DRSYM_FULL_SEARCH | DRSYM_DEMANGLE,
                                search_ex_templates_cb, sizeof(drsym_info_t), NULL);
    ASSERT(r == DRSYM_SUCCESS);
    r = drsym_enumerate_symbols(exe_path, search_templates_cb, NULL, DRSYM_DEFAULT_FLAGS);
    ASSERT(r == DRSYM_SUCCESS);
    r = drsym_enumerate_symbols_ex(exe_path, search_ex_templates_cb, sizeof(drsym_info_t),
                                   NULL, DRSYM_DEFAULT_FLAGS);
    ASSERT(r == DRSYM_SUCCESS);
    /* These should expand the templates */
    r = drsym_search_symbols_ex(exe_path, "*!*nested*",
                                DRSYM_FULL_SEARCH | DRSYM_DEMANGLE |
                                    DRSYM_DEMANGLE_PDB_TEMPLATES,
                                search_ex_templates_cb, sizeof(drsym_info_t), NULL);
    ASSERT(r == DRSYM_SUCCESS);
    r = drsym_enumerate_symbols(exe_path, search_templates_cb, NULL,
                                DRSYM_DEMANGLE | DRSYM_DEMANGLE_PDB_TEMPLATES);
    ASSERT(r == DRSYM_SUCCESS);
    r = drsym_enumerate_symbols_ex(exe_path, search_ex_templates_cb, sizeof(drsym_info_t),
                                   NULL, DRSYM_DEMANGLE | DRSYM_DEMANGLE_PDB_TEMPLATES);
    ASSERT(r == DRSYM_SUCCESS);
}

/* This routine assumes it's called only at init time. */
static void
lookup_type_by_name(const char *exe_path)
{
    drsym_type_t *type;
    drsym_error_t r;
    static char buf[4096];
    /* It should successfully return valid type info */
    r = drsym_get_type_by_name(exe_path, "`anonymous-namespace'::HasFields", buf,
                               BUFFER_SIZE_BYTES(buf), &type);
    ASSERT(r == DRSYM_SUCCESS);
    dr_fprintf(STDERR, "drsym_get_type_by_name successfully found HasFields type\n");
}
#endif /* WINDOWS */

static bool
enum_line_cb(drsym_line_info_t *info, void *data)
{
    const module_data_t *dll_data = (const module_data_t *)data;
    ASSERT(info->line_addr <= (size_t)(dll_data->end - dll_data->start));
    if (info->file != NULL) {
        if (!found_appdll && strstr(info->file, "drsyms-test.appdll.cpp") != NULL) {
            found_appdll = true;
        }
        if (!found_tools_h && strstr(info->file, "tools.h") != NULL) {
            found_tools_h = true;
        }
    }
    return true;
}

static void
test_line_iteration(const module_data_t *dll_data)
{
    drsym_error_t res =
        drsym_enumerate_lines(dll_data->full_path, enum_line_cb, (void *)dll_data);
    ASSERT(res == DRSYM_SUCCESS);
    /* We print outside of the loop to ensure a fixed order */
    if (found_appdll)
        dr_fprintf(STDERR, "found drsyms-test.appdll.cpp\n");
    if (found_tools_h)
        dr_fprintf(STDERR, "found tools.h\n");
}

/* Lookup symbols in the appdll and wrap them. */
static void
lookup_dll_syms(void *dc, const module_data_t *dll_data, bool loaded)
{
    const char *dll_path;
    app_pc dll_base;
    app_pc dll_export_addr;
    size_t dll_export_offs;
    size_t stack_trace_offs;
    drsym_error_t r;
    bool ok;
    drsym_debug_kind_t debug_kind;
    const char *dll_name = dr_module_preferred_name(dll_data);
    char base_name[MAXIMUM_PATH];
    size_t len;

    dll_path = dll_data->full_path;
    dll_base = dll_data->start;

#ifdef UNIX
    if (strstr(dll_path, "/libc-")) {
        lookup_glibc_syms(dc, dll_data);
        return;
    }
#endif

    /* Avoid running on any module other than the appdll. */
    if (!strstr(dll_path, "appdll"))
        return;

    len = dr_snprintf(base_name, BUFFER_SIZE_ELEMENTS(base_name), "%s", dll_name);
    ASSERT(len > 0);
#ifdef WINDOWS
    /* blindly assuming ends in .dll */
    base_name[strlen(dll_name) - 4 /*subtract .dll*/] = '\0';
#else
    /* blindly assuming ends in .so */
    base_name[strlen(dll_name) - 3 /*subtract .so*/] = '\0';
#endif

    /* We expect to have full debug info for this module. */
    r = drsym_get_module_debug_kind(dll_path, &debug_kind);
    ASSERT(r == DRSYM_SUCCESS);
    if (!debug_kind_is_full(debug_kind)) {
        dr_fprintf(STDERR, "unexpected debug_kind: %x\n", debug_kind);
    }

    dll_export_addr = get_real_proc_addr(dll_data->handle, "dll_export");
    dll_export_offs =
        lookup_and_wrap(dll_path, dll_base, base_name, "dll_export", DRSYM_DEFAULT_FLAGS);
    ASSERT(dll_export_addr == dll_base + dll_export_offs);

    /* dll_public is a function in the dll we wouldn't be able to find without
     * drsyms and debug info.
     */
    (void)lookup_and_wrap(dll_path, dll_base, base_name, "dll_public",
                          DRSYM_DEFAULT_FLAGS);

    /* stack_trace is a static function in the DLL that we use to get PCs of all
     * the functions we've looked up so far.
     */
    dr_snprintf(base_name + strlen(base_name),
                BUFFER_SIZE_ELEMENTS(base_name) - strlen(base_name), "!stack_trace");
    r = drsym_lookup_symbol(dll_path, base_name, &stack_trace_offs, DRSYM_DEFAULT_FLAGS);
    ASSERT(r == DRSYM_SUCCESS);
    ok = drwrap_wrap(dll_base + stack_trace_offs, pre_stack_trace, post_func);
    ASSERT(ok);

    check_enumerate_dll_syms(dll_path);

    test_line_iteration(dll_data);

    drsym_free_resources(dll_path);
}

static const char *dll_syms[] = { "dll_export", "dll_static", "dll_public", "stack_trace",
                                  NULL };

/* FIXME: We don't support getting mangled or fully demangled symbols on
 * Windows PDB.
 */
static const char *dll_syms_mangled_pdb[] = { "dll_export", "dll_static", "dll_public",
                                              "stack_trace", NULL

};

static const char *dll_syms_mangled[] = { "dll_export", "_ZL10dll_statici",
                                          "_Z10dll_publici", "_Z11stack_tracev", NULL };

static const char *dll_syms_short_pdb[] = { "dll_export", "dll_static", "dll_public",
                                            "stack_trace", NULL };

static const char *dll_syms_short[] = { "dll_export", "dll_static", "dll_public",
                                        "stack_trace", NULL };

static const char *dll_syms_full_pdb[] = { "dll_export", "dll_static", "dll_public",
                                           "stack_trace", NULL };

static const char *dll_syms_full[] = { "dll_export", "dll_static(int)", "dll_public(int)",
                                       "stack_trace(void)", NULL };

typedef struct {
    bool syms_found[BUFFER_SIZE_ELEMENTS(dll_syms) - 1];
    const char **syms_expected;
    const char *dll_path;
    uint flags_expected;
    /* used to handle type id mismatches (i#1376, i#1638) */
    char prev_name[MAXIMUM_PATH];
    uint prev_type;
    bool prev_mismatch;
} dll_syms_found_t;

/* If this was a symbol we expected that we haven't found yet, mark it found,
 * and check the mangling to see if it matches our expected mangling.
 */
static bool
enum_sym_cb(const char *name, size_t modoffs, void *data)
{
    dll_syms_found_t *syms_found = (dll_syms_found_t *)data;
    uint i;
    for (i = 0; i < BUFFER_SIZE_ELEMENTS(syms_found->syms_found); i++) {
        if (syms_found->syms_found[i])
            continue;
        if (strstr(name, dll_syms[i]) != NULL) {
            syms_found->syms_found[i] = true;
            const char *expected = syms_found->syms_expected[i];
            char alternative[MAX_FUNC_LEN] = "\xff"; /* invalid string */
            /* If the expected mangling is _ZL*, try _Z* too.  Different gccs
             * from Cyginw, MinGW, and Linux all do different things.
             */
            if (strncmp(expected, "_ZL", 3) == 0) {
                dr_snprintf(alternative, BUFFER_SIZE_ELEMENTS(alternative), "%.2s%s",
                            expected, expected + 3);
                NULL_TERMINATE_BUFFER(alternative);
            }
            /* I'm seeing an inserted underscore too: */
            if (expected[0] != '_') {
                dr_snprintf(alternative, BUFFER_SIZE_ELEMENTS(alternative), "_%s",
                            expected);
                NULL_TERMINATE_BUFFER(alternative);
            }
            if (strcmp(name, expected) != 0 && strcmp(name, alternative) != 0) {
                dr_fprintf(STDERR,
                           "symbol had wrong mangling:\n"
                           " expected: %s\n actual: %s\n",
                           syms_found->syms_expected[i], name);
            }
        }
    }
    return true;
}

static bool
enum_sym_ex_cb(drsym_info_t *out, drsym_error_t status, void *data)
{
    dll_syms_found_t *syms_found = (dll_syms_found_t *)data;
    uint i;

    ASSERT(status == DRSYM_ERROR_LINE_NOT_AVAILABLE);
    /* Some dbghelps have the available size as larger sometimes, strangely. */
    ASSERT(strlen(out->name) <= out->name_available_size);
    ASSERT((out->file == NULL && out->file_available_size == 0) ||
           (strlen(out->file) == out->file_available_size));

    for (i = 0; i < BUFFER_SIZE_ELEMENTS(syms_found->syms_found); i++) {
        if (syms_found->syms_found[i])
            continue;
        if (strstr(out->name, dll_syms[i]) != NULL)
            syms_found->syms_found[i] = true;
    }

    ASSERT(out->flags ==
           syms_found->flags_expected IF_WINDOWS(
               || syms_found->flags_expected == DRSYM_LEAVE_MANGLED ||
               (out->flags == (syms_found->flags_expected & ~DRSYM_DEMANGLE_FULL))));

    if (TEST(DRSYM_PDB, out->debug_kind)) { /* else types NYI */
        static char buf[4096];
        drsym_type_t *type;
        drsym_error_t r = drsym_get_type(syms_found->dll_path, out->start_offs, 1, buf,
                                         sizeof(buf), &type);
        if (r == DRSYM_SUCCESS) {
            /* XXX: I'm seeing error 126 (ERROR_MOD_NOT_FOUND) from
             * SymFromAddr for some symbols that the enum finds: strange.
             * On another machine I saw mismatches in type id:
             *   error for __initiallocinfo: 481 != 483, kind = 5
             * Grrr!  Do we really have to go and compare all the properties
             * of the type to ensure it's the same?!?
             *
             * Plus, with VS2008 dbghelp we get a lot of even worse mismatches
             * here (part of i#1196).  We only use it on pre-Vista so we relax this
             * check there.
             */
            /* i#1638: we delay reporting a mismatch to extend i#1376 to
             * duplicate names in the other order: i.e., the 1st has a mismatched
             * type, but the 2nd's type matches:
             *   comparing id=497 vs 89 _wctype
             *   comparing id=497 vs 497 _wctype
             * We check for mismatch on the last entry at the caller site.
             */
            ASSERT(!syms_found->prev_mismatch ||
                   strcmp(out->name, syms_found->prev_name) == 0);
            if (!(IF_WINDOWS(os_version.version < DR_WINDOWS_VERSION_VISTA ||)
                          type->id == out->type_id ||
                  /* Unknown type has id cleared to 0 */
                  (type->kind == DRSYM_TYPE_OTHER && type->id == 0) ||
                  /* Some __ types seem to have varying id's */
                  strstr(out->name, "__") == out->name ||
                  /* i#1376: if we use a recent dbghelp.dll,
                   * we see weird duplicate names w/ different ids
                   */
                  strcmp(out->name, syms_found->prev_name) == 0)) {
                /* XXX i#4056: Given all the inconsistencies in recent dbghelp,
                 * I'm giving up on ensuring the types match and just never
                 * setting prev_mismatch to false!
                 */
                syms_found->prev_mismatch = false;
            } else
                syms_found->prev_mismatch = false;
            syms_found->prev_type = type->id;
        }
    }
    dr_snprintf(syms_found->prev_name, BUFFER_SIZE_ELEMENTS(syms_found->prev_name), "%s",
                out->name);
    NULL_TERMINATE_BUFFER(syms_found->prev_name);
    return true;
}

static void
enum_syms_with_flags(const char *dll_path, const char **syms_expected, uint flags)
{
    dll_syms_found_t syms_found;
    drsym_error_t r;
    uint i;
    drsym_debug_kind_t debug_kind;
    r = drsym_get_module_debug_kind(dll_path, &debug_kind);
    ASSERT(r == DRSYM_SUCCESS);

    memset(&syms_found, 0, sizeof(syms_found));
    syms_found.syms_expected = syms_expected;
    r = drsym_enumerate_symbols(dll_path, enum_sym_cb, &syms_found, flags);
    ASSERT(r == DRSYM_SUCCESS);
    for (i = 0; i < BUFFER_SIZE_ELEMENTS(syms_found.syms_found); i++) {
        if (!syms_found.syms_found[i])
            dr_fprintf(STDERR, "failed to find symbol for %s!\n", dll_syms[i]);
    }

    /* Test the _ex version */
    memset(&syms_found, 0, sizeof(syms_found));
    syms_found.dll_path = dll_path;
    syms_found.flags_expected = flags;
    r = drsym_enumerate_symbols_ex(dll_path, enum_sym_ex_cb, sizeof(drsym_info_t),
                                   &syms_found, flags);
    ASSERT(r == DRSYM_SUCCESS && !syms_found.prev_mismatch);
    for (i = 0; i < BUFFER_SIZE_ELEMENTS(syms_found.syms_found); i++) {
        if (!syms_found.syms_found[i])
            dr_fprintf(STDERR, "_ex failed to find symbol for %s!\n", dll_syms[i]);
    }

#ifdef WINDOWS
    if (TEST(DRSYM_PDB, debug_kind)) {
        /* drsym_search_symbols should find the same symbols with the short
         * mangling, regardless of the flags used by the previous enumerations.
         */
        memset(&syms_found, 0, sizeof(syms_found));
        syms_found.syms_expected = dll_syms_short_pdb;
        r = drsym_search_symbols(dll_path, "*!*dll_*", false, enum_sym_cb, &syms_found);
        ASSERT(r == DRSYM_SUCCESS);
        r = drsym_search_symbols(dll_path, "*!*stack_trace*", false, enum_sym_cb,
                                 &syms_found);
        ASSERT(r == DRSYM_SUCCESS);
        for (i = 0; i < BUFFER_SIZE_ELEMENTS(syms_found.syms_found); i++) {
            if (!syms_found.syms_found[i])
                dr_fprintf(STDERR, "search failed to find %s!\n", dll_syms[i]);
        }

        /* Test the _ex version */
        memset(&syms_found, 0, sizeof(syms_found));
        syms_found.dll_path = dll_path;
        r = drsym_search_symbols_ex(dll_path, "*!*dll_*", DRSYM_DEMANGLE, enum_sym_ex_cb,
                                    sizeof(drsym_info_t), &syms_found);
        ASSERT(r == DRSYM_SUCCESS && !syms_found.prev_mismatch);
        r = drsym_search_symbols_ex(dll_path, "*!*stack_trace*", DRSYM_DEMANGLE,
                                    enum_sym_ex_cb, sizeof(drsym_info_t), &syms_found);
        ASSERT(r == DRSYM_SUCCESS && !syms_found.prev_mismatch);
        for (i = 0; i < BUFFER_SIZE_ELEMENTS(syms_found.syms_found); i++) {
            if (!syms_found.syms_found[i])
                dr_fprintf(STDERR, "search _ex failed to find %s!\n", dll_syms[i]);
        }
    }
#endif
}

/* Enumerate all symbols in the dll and verify that we at least find the ones
 * we expected to be there, and that DRSYM_LEAVE_MANGLED was respected.
 */
static void
check_enumerate_dll_syms(const char *dll_path)
{
    drsym_debug_kind_t debug_kind;
    drsym_error_t r = drsym_get_module_debug_kind(dll_path, &debug_kind);
    ASSERT(r == DRSYM_SUCCESS);

    dr_fprintf(STDERR, "enumerating with DRSYM_LEAVE_MANGLED\n");
    enum_syms_with_flags(
        dll_path, TEST(DRSYM_PDB, debug_kind) ? dll_syms_mangled_pdb : dll_syms_mangled,
        DRSYM_LEAVE_MANGLED);
    dr_fprintf(STDERR, "enumerating with DRSYM_DEMANGLE\n");
    enum_syms_with_flags(
        dll_path, TEST(DRSYM_PDB, debug_kind) ? dll_syms_short_pdb : dll_syms_short,
        DRSYM_DEMANGLE);
    dr_fprintf(STDERR, "enumerating with DRSYM_DEMANGLE_FULL\n");
    enum_syms_with_flags(dll_path,
                         TEST(DRSYM_PDB, debug_kind) ? dll_syms_full_pdb : dll_syms_full,
                         (DRSYM_DEMANGLE | DRSYM_DEMANGLE_FULL));
}

#ifdef UNIX
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

    /* FIXME: When drsyms can read .dynsym we should always find malloc. */
    malloc_offs = 0;
    r = drsym_lookup_symbol(libc_path, "libc!malloc", &malloc_offs, DRSYM_DEFAULT_FLAGS);
    if (r == DRSYM_SUCCESS)
        ASSERT(malloc_offs != 0);

    /* __GI___libc_malloc is glibc's internal reference to malloc.  They use
     * these internal symbols so that glibc calls to exported functions are
     * never pre-empted by other libraries.
     */
    gi_malloc_offs = 0;
    r = drsym_lookup_symbol(libc_path, "libc!__GI___libc_malloc", &gi_malloc_offs,
                            DRSYM_DEFAULT_FLAGS);
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
#endif /* UNIX */

typedef struct {
    const char *mangled;
    const char *dem_full;
    const char *demangled;
} cpp_name_t;

/* Table of mangled and unmangled symbols taken as a random sample from a
 * 32-bit Linux Chromium binary.
 */
static cpp_name_t symbols_unix[] = {
    { "_ZN4baseL9kDeadTaskE", "base::kDeadTask", "base::kDeadTask" },
    { "xmlRelaxNGParseImportRefs", "xmlRelaxNGParseImportRefs",
      "xmlRelaxNGParseImportRefs" },
    { "_ZL16piOverFourDouble", "piOverFourDouble", "piOverFourDouble" },
    { "_ZL8kint8min", "kint8min", "kint8min" },
    { "_ZZN7WebCore19SVGAnimatedProperty20LookupOrCreateHelperINS_"
      "32SVGAnimatedStaticPropertyTearOffIbEEbLb1EE21lookupOrCreateWrapperEPNS_"
      "10SVGElementEPKNS_15SVGPropertyInfoERbE19__PRETTY_FUNCTION__",
      "WebCore::SVGAnimatedProperty::LookupOrCreateHelper<WebCore::"
      "SVGAnimatedStaticPropertyTearOff<bool>, bool, "
      "true>::lookupOrCreateWrapper(WebCore::SVGElement*, WebCore::SVGPropertyInfo "
      "const*, bool&)::__PRETTY_FUNCTION__",
      "WebCore::SVGAnimatedProperty::LookupOrCreateHelper<>::lookupOrCreateWrapper::__"
      "PRETTY_FUNCTION__" },
    { "_ZL26GrNextArrayAllocationCounti", "GrNextArrayAllocationCount(int)",
      "GrNextArrayAllocationCount" },
    { "_ZN18safe_browsing_util25GeneratePhishingReportUrlERKSsS1_b",
      "safe_browsing_util::GeneratePhishingReportUrl(std::string const&, std::string, "
      "bool)",
      "safe_browsing_util::GeneratePhishingReportUrl" },
    { "_ZN9__gnu_cxx8hash_mapIjPN10disk_cache9EntryImplENS_4hashIjEESt8equal_toIjESaIS3_"
      "EE4findERKj",
      "__gnu_cxx::hash_map<unsigned int, disk_cache::EntryImpl*, "
      "__gnu_cxx::hash<unsigned int>, std::equal_to<unsigned int>, "
      "std::allocator<disk_cache::EntryImpl*> >::find(unsigned int const&)",
      "__gnu_cxx::hash_map<>::find" },
    { "_ZN18shortcuts_provider8ShortcutC2ERKSbItN4base20string16_char_"
      "traitsESaItEERK4GURLS6_"
      "RKSt6vectorIN17AutocompleteMatch21ACMatchClassificationESaISC_EES6_SG_",
      "shortcuts_provider::Shortcut::Shortcut(std::basic_string<unsigned short, "
      "base::string16_char_traits, std::allocator<unsigned short> > const&, GURL const&, "
      "std::basic_string<unsigned short, base::string16_char_traits, "
      "std::allocator<unsigned short> > const, "
      "std::vector<AutocompleteMatch::ACMatchClassification, "
      "std::allocator<AutocompleteMatch> > const&, std::basic_string<unsigned short, "
      "base::string16_char_traits, std::allocator<unsigned short> > const, "
      "std::vector<AutocompleteMatch::ACMatchClassification, "
      "std::allocator<AutocompleteMatch> > const)",
      "shortcuts_provider::Shortcut::Shortcut" },
    /* XXX libelftc fails on this pre-r3531, but r3531 has worse bugs so we
     * live with the failure here.  Xref i#4087.
     */
    { "_ZN10linked_ptrIN12CrxInstaller14WhitelistEntryEE4copyIS1_EEvPKS_IT_E",
      "void linked_ptr<CrxInstaller::WhitelistEntry>::copy<CrxInstaller::"
      "WhitelistEntry>(linked_ptr const*<CrxInstaller::WhitelistEntry>)",
      "linked_ptr<>::copy<>" },
    { "_ZN27ScopedRunnableMethodFactoryIN6webkit5ppapi18PPB_Scrollbar_ImplEED1Ev",
      "ScopedRunnableMethodFactory<webkit::ppapi::PPB_Scrollbar_Impl>::~"
      "ScopedRunnableMethodFactory(void)",
      "ScopedRunnableMethodFactory<>::~ScopedRunnableMethodFactory" },
    { "_ZN2v88internal9HashTableINS0_21StringDictionaryShapeEPNS0_"
      "6StringEE9FindEntryEPNS0_7IsolateES4_",
      "v8::internal::HashTable<v8::internal::StringDictionaryShape, "
      "v8::internal::String*>::FindEntry(v8::internal::Isolate*, "
      "v8::internal::HashTable<v8::internal::StringDictionaryShape, "
      "v8::internal::String*>)",
      "v8::internal::HashTable<>::FindEntry" },
    { "_ZNK7WebCore8Settings19localStorageEnabledEv",
      "WebCore::Settings::localStorageEnabled(void) const",
      "WebCore::Settings::localStorageEnabled" },
    { "_ZNSt4listIPN5media12VideoCapture12EventHandlerESaIS3_EE14_M_create_nodeERKS3_",
      "std::list<media::VideoCapture::EventHandler*, "
      "std::allocator<media::VideoCapture::EventHandler*> "
      ">::_M_create_node(media::VideoCapture::EventHandler* const&)",
      "std::list<>::_M_create_node" },
    { "_ZNK9__gnu_cxx13new_allocatorISt13_Rb_tree_"
      "nodeISt4pairIKiP20RenderWidgetHostViewEEE8max_sizeEv",
      "__gnu_cxx::new_allocator<std::_Rb_tree_node<std::pair<int const, "
      "RenderWidgetHostView*> >>::max_size(void) const",
      "__gnu_cxx::new_allocator<>::max_size" },
};

#ifdef WINDOWS
static cpp_name_t symbols_pdb[] = {
    { "?synchronizeRequiredExtensions@SVGSVGElement@WebCore@@EAEXXZ",
      "WebCore::SVGSVGElement::synchronizeRequiredExtensions(void)",
      "WebCore::SVGSVGElement::synchronizeRequiredExtensions" },
    { "??$?0$04@WebString@WebKit@@QAE@AAY04$$CBD@Z",
      "WebKit::WebString::WebString<5>(char const (&)[5])",
      "WebKit::WebString::WebString<>" },
    { "?createParser@PluginDocument@WebCore@@EAE?AV?$PassRefPtr@VDocumentParser@WebCore@@"
      "@WTF@@XZ",
      "WebCore::PluginDocument::createParser(void)",
      "WebCore::PluginDocument::createParser" },
    { "?_Compat@?$_Vector_const_iterator@V?$_Iterator@$00@?$list@U?$pair@$$"
      "CBHPAVWebIDBCursor@WebKit@@@std@@V?$allocator@U?$pair@$$CBHPAVWebIDBCursor@WebKit@"
      "@@std@@@2@@std@@V?$allocator@V?$_Iterator@$00@?$list@U?$pair@$$CBHPAVWebIDBCursor@"
      "WebKit@@@std@@V?$allocator@U?$pair@$$CBHPAVWebIDBCursor@WebKit@@@std@@@2@@std@@@3@"
      "@std@@QBEXABV12@@Z",
      "std::_Vector_const_iterator<class std::list<struct std::pair<int const ,class "
      "WebKit::WebIDBCursor *>,class std::allocator<struct std::pair<int const ,class "
      "WebKit::WebIDBCursor *> > >::_Iterator<1>,class std::allocator<class "
      "std::list<struct std::pair<int const ,class WebKit::WebIDBCursor *>,class "
      "std::allocator<struct std::pair<int const ,class WebKit::WebIDBCursor *> > "
      ">::_Iterator<1> > >::_Compat(class std::_Vector_const_iterator<class "
      "std::list<struct std::pair<int const ,class WebKit::WebIDBCursor *>,class "
      "std::allocator<struct std::pair<int const ,class WebKit::WebIDBCursor *> > "
      ">::_Iterator<1>,class std::allocator<class std::list<struct std::pair<int const "
      ",class WebKit::WebIDBCursor *>,class std::allocator<struct std::pair<int const "
      ",class WebKit::WebIDBCursor *> > >::_Iterator<1> > > const &)const ",
      "std::_Vector_const_iterator<>::_Compat" },
    { "??$MatchAndExplain@VNotificationDetails@@@?$PropertyMatcher@V?$Details@$$"
      "CBVAutofillCreditCardChange@@@@PBVAutofillCreditCardChange@@@internal@testing@@"
      "QBE_NABVNotificationDetails@@PAVMatchResultListener@2@@Z",
      "testing::internal::PropertyMatcher<class Details<class AutofillCreditCardChange "
      "const >,class AutofillCreditCardChange const *>::MatchAndExplain<class "
      "NotificationDetails>(class NotificationDetails const &,class "
      "testing::MatchResultListener *)const ",
      "testing::internal::PropertyMatcher<>::MatchAndExplain<>" },
    { "?MD5Sum@base@@YAXPBXIPAUMD5Digest@1@@Z",
      "base::MD5Sum(void const *,unsigned int,struct base::MD5Digest *)",
      "base::MD5Sum" },
    { "?create@EntryCallbacks@WebCore@@SA?AV?$PassOwnPtr@VEntryCallbacks@WebCore@@@WTF@@"
      "V?$PassRefPtr@VEntryCallback@WebCore@@@4@V?$PassRefPtr@VErrorCallback@WebCore@@@4@"
      "V?$PassRefPtr@VDOMFileSystemBase@WebCore@@@4@ABVString@4@_N@Z",
      "WebCore::EntryCallbacks::create(class WTF::PassRefPtr<class "
      "WebCore::EntryCallback>,class WTF::PassRefPtr<class WebCore::ErrorCallback>,class "
      "WTF::PassRefPtr<class WebCore::DOMFileSystemBase>,class WTF::String const &,bool)",
      "WebCore::EntryCallbacks::create" },
    { "?ReadReplyParam@ClipboardHostMsg_ReadAsciiText@@SA_NPBVMessage@IPC@@PAU?$Tuple1@V?"
      "$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@@@Z",
      "ClipboardHostMsg_ReadAsciiText::ReadReplyParam(class IPC::Message const *,struct "
      "Tuple1<class std::basic_string<char,struct std::char_traits<char>,class "
      "std::allocator<char> > > *)",
      "ClipboardHostMsg_ReadAsciiText::ReadReplyParam" },
    { "?begin@?$HashMap@PAVValue@v8@@PAVGlobalHandleInfo@WebCore@@U?$PtrHash@PAVValue@v8@"
      "@@WTF@@U?$HashTraits@PAVValue@v8@@@6@U?$HashTraits@PAVGlobalHandleInfo@WebCore@@@"
      "6@@WTF@@QAE?AU?$HashTableIteratorAdapter@V?$HashTable@PAVValue@v8@@U?$pair@"
      "PAVValue@v8@@PAVGlobalHandleInfo@WebCore@@@std@@U?$PairFirstExtractor@U?$pair@"
      "PAVValue@v8@@PAVGlobalHandleInfo@WebCore@@@std@@@WTF@@U?$PtrHash@PAVValue@v8@@@6@"
      "U?$PairHashTraits@U?$HashTraits@PAVValue@v8@@@WTF@@U?$HashTraits@"
      "PAVGlobalHandleInfo@WebCore@@@2@@6@U?$HashTraits@PAVValue@v8@@@6@@WTF@@U?$pair@"
      "PAVValue@v8@@PAVGlobalHandleInfo@WebCore@@@std@@@2@XZ",
      "WTF::HashMap<class v8::Value *,class WebCore::GlobalHandleInfo *,struct "
      "WTF::PtrHash<class v8::Value *>,struct WTF::HashTraits<class v8::Value *>,struct "
      "WTF::HashTraits<class WebCore::GlobalHandleInfo *> >::begin(void)",
      "WTF::HashMap<>::begin" },
    { "??D?$_Deque_iterator@V?$linked_ptr@V?$CallbackRunner@U?$Tuple1@H@@@@@@V?$"
      "allocator@V?$linked_ptr@V?$CallbackRunner@U?$Tuple1@H@@@@@@@std@@$00@std@@QBEAAV?$"
      "linked_ptr@V?$CallbackRunner@U?$Tuple1@H@@@@@@XZ",
      "std::_Deque_iterator<class linked_ptr<class CallbackRunner<struct Tuple1<int> > "
      ">,class std::allocator<class linked_ptr<class CallbackRunner<struct Tuple1<int> > "
      "> >,1>::operator*(void)const ",
      "std::_Deque_iterator<>::operator*" },
    { "??$PerformAction@$$A6AXABVFilePath@@0PBVDictionaryValue@base@@PBVExtension@@@Z@?$"
      "ActionResultHolder@X@internal@testing@@SAPAV012@ABV?$Action@$$A6AXABVFilePath@@"
      "0PBVDictionaryValue@base@@PBVExtension@@@Z@2@ABV?$tuple@ABVFilePath@@ABV1@"
      "PBVDictionaryValue@base@@PBVExtension@@XXXXXX@tr1@std@@@Z",
      "testing::internal::ActionResultHolder<void>::PerformAction<void (class FilePath "
      "const &,class FilePath const &,class base::DictionaryValue const *,class "
      "Extension const *)>(class testing::Action<void (class FilePath const &,class "
      "FilePath const &,class base::DictionaryValue const *,class Extension const *)> "
      "const &,class std::tr1::tuple<class FilePath const &,class FilePath const &,class "
      "base::DictionaryValue const *,class Extension const "
      "*,void,void,void,void,void,void> const &)",
      "testing::internal::ActionResultHolder<>::PerformAction<>" },
    { "?ClassifyInputEvent@ppapi@webkit@@YA?AW4PP_InputEvent_Class@@W4Type@WebInputEvent@"
      "WebKit@@@Z",
      "webkit::ppapi::ClassifyInputEvent(enum WebKit::WebInputEvent::Type)",
      "webkit::ppapi::ClassifyInputEvent" },
    /* Test removal of template parameters.  I don't have the mangled forms of
     * these b/c I'm drawing them from Chromium private symbols, which are never
     * decorated.
     */
    { "std::operator<<<std::char_traits<char> >",
      "std::operator<<<std::char_traits<char> >", "std::operator<<<>" },
    { "std::operator<<std::char_traits<char> >",
      "std::operator<<std::char_traits<char> >", "std::operator<<>" },
    { "std::operator<=<std::char_traits<char> >",
      "std::operator<=<std::char_traits<char> >", "std::operator<=<>" },
    { "std::operator<<=<std::char_traits<char> >",
      "std::operator<<=<std::char_traits<char> >", "std::operator<<=<>" },
    { "myclass<foo<bar<baz> > >::std::operator-><std::char_traits<char> >",
      "myclass<foo<bar<baz> > >::std::operator-><std::char_traits<char> >",
      "myclass<>::std::operator-><>" },
    { "std::operator-><std::char_traits<char, truncated",
      "std::operator-><std::char_traits<char, truncated",
      /* Truncated => we just close <> */
      "std::operator-><>" },
    { "std::operator<<<<<std::char_traits<char, truncated",
      "std::operator<<<<<std::char_traits<char, truncated", "<failure to unmangle>" },
    /* Test non-template <> */
    { "<CrtImplementationDetails>::NativeDll::ProcessVerifier",
      "<CrtImplementationDetails>::NativeDll::ProcessVerifier",
      "<CrtImplementationDetails>::NativeDll::ProcessVerifier" },
    { "foo::<unamed-tag>::<not a template>::template<foo::<bar>>",
      "foo::<unamed-tag>::<not a template>::template<foo::<bar>>",
      "foo::<unamed-tag>::<not a template>::template<>" },
    /* Test malformed */
    { "bogus<::std::operator-><std::char_traits<char> >",
      "bogus<::std::operator-><std::char_traits<char> >",
      /* Is this what we want?  Should we add a more sophisticated parser to
       * detect this as malformed?
       */
      "bogus<><>" },
};
#endif

static void
test_demangle_symbols(cpp_name_t *symbols, size_t symbols_sz)
{
    char sym_buf[2048];
    unsigned i;
    size_t len;

    for (i = 0; i < symbols_sz; i++) {
        cpp_name_t *sym = &symbols[i];

        /* Full demangling. */
        len = drsym_demangle_symbol(sym_buf, sizeof(sym_buf), sym->mangled,
                                    DRSYM_DEMANGLE_FULL);
        if (len == 0 || len >= sizeof(sym_buf)) {
            dr_fprintf(STDERR, "Failed to unmangle %s\n", sym->mangled);
        } else if (strcmp(sym_buf, sym->dem_full) != 0) {
            dr_fprintf(STDERR,
                       "Unexpected unmangling:\n"
                       " actual: %s\n expected: %s\n",
                       sym_buf, sym->dem_full);
        }

        /* Short demangling (no templates or overloads). */
        len =
            drsym_demangle_symbol(sym_buf, sizeof(sym_buf), sym->mangled, DRSYM_DEMANGLE);
        if (len == 0 || len >= sizeof(sym_buf)) {
            dr_fprintf(STDERR, "Failed to unmangle %s\n", sym->mangled);
        } else if (strcmp(sym_buf, sym->demangled) != 0) {
            dr_fprintf(STDERR,
                       "Unexpected unmangling:\n"
                       " actual: %s\n expected: %s\n",
                       sym_buf, sym->demangled);
        }
    }

    /* Test overflow. */
    len = drsym_demangle_symbol(sym_buf, 6, symbols[0].mangled, DRSYM_DEMANGLE_FULL);
    if (len == 0) {
        dr_fprintf(STDERR, "got error instead of overflow\n");
    } else if (len <= 6) {
        dr_fprintf(STDERR, "unexpected demangling success\n");
    } else {
        size_t old_len = 6;
        dr_fprintf(STDERR, "got correct overflow\n");
        /* Resize the buffer in a loop until it demangles correctly. */
        while (len > old_len && len < sizeof(sym_buf)) {
            old_len = len;
            len = drsym_demangle_symbol(sym_buf, old_len, symbols[0].mangled,
                                        DRSYM_DEMANGLE_FULL);
        }
        if (strcmp(sym_buf, symbols[0].dem_full) != 0) {
            dr_fprintf(STDERR, "retrying with demangle return value failed.\n");
        }
    }
}

static void
test_demangle(void)
{
    test_demangle_symbols(symbols_unix, BUFFER_SIZE_ELEMENTS(symbols_unix));
#ifdef WINDOWS
    test_demangle_symbols(symbols_pdb, BUFFER_SIZE_ELEMENTS(symbols_pdb));
#endif
    dr_fprintf(STDERR, "finished unmangling.\n");
}

static void
event_exit(void)
{
    drwrap_exit();
    drsym_exit();
    /* Check that all symbols we looked up got called. */
    ASSERT(call_count == 4);
    /* We don't print "all done" to avoid differences in printing working
     * or not in a cygwin program (i#1478).
     */
}
