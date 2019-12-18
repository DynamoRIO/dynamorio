/* **********************************************************
 * Copyright (c) 2013-2017 Google, Inc.   All rights reserved.
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

/* kernel32.dll and kernelbase.dll library redirection routines */

#include "kernel32_redir.h" /* must be included first */
#include "../../globals.h"
#include "../../module_shared.h"
#include "drwinapi.h"

/* If you add any new priv invocation pointer here, update the list in
 * drwinapi_redirect_imports().
 */
static HMODULE(WINAPI *priv_kernel32_GetModuleHandleA)(const char *);
static HMODULE(WINAPI *priv_kernel32_GetModuleHandleW)(const wchar_t *);
static FARPROC(WINAPI *priv_kernel32_GetProcAddress)(HMODULE, const char *);
static HMODULE(WINAPI *priv_kernel32_LoadLibraryA)(const char *);
static HMODULE(WINAPI *priv_kernel32_LoadLibraryW)(const wchar_t *);

void
kernel32_redir_onload_lib(privmod_t *mod)
{
    priv_kernel32_GetModuleHandleA = (HMODULE(WINAPI *)(const char *))get_proc_address_ex(
        mod->base, "GetModuleHandleA", NULL);
    priv_kernel32_GetModuleHandleW =
        (HMODULE(WINAPI *)(const wchar_t *))get_proc_address_ex(mod->base,
                                                                "GetModuleHandleW", NULL);
    priv_kernel32_GetProcAddress = (FARPROC(WINAPI *)(
        HMODULE, const char *))get_proc_address_ex(mod->base, "GetProcAddress", NULL);
    priv_kernel32_LoadLibraryA = (HMODULE(WINAPI *)(const char *))get_proc_address_ex(
        mod->base, "LoadLibraryA", NULL);
    priv_kernel32_LoadLibraryW = (HMODULE(WINAPI *)(const wchar_t *))get_proc_address_ex(
        mod->base, "LoadLibraryW", NULL);
}

/* Eventually we should intercept at the Ldr level but that takes more work
 * so we initially just intercept here.  This is also needed to intercept
 * FlsAlloc located dynamically by msvcrt init.
 */
HMODULE WINAPI
redirect_GetModuleHandleA(const char *name)
{
    privmod_t *mod;
    app_pc res = NULL;
    ASSERT(priv_kernel32_GetModuleHandleA != NULL);
    acquire_recursive_lock(&privload_lock);
    mod = privload_lookup(name);
    if (mod != NULL) {
        res = mod->base;
        LOG(GLOBAL, LOG_LOADER, 2, "%s: %s => " PFX "\n", __FUNCTION__, name, res);
    }
    release_recursive_lock(&privload_lock);
    if (mod == NULL)
        return (*priv_kernel32_GetModuleHandleA)(name);
    else
        return (HMODULE)res;
}

HMODULE WINAPI
redirect_GetModuleHandleW(const wchar_t *name)
{
    privmod_t *mod;
    app_pc res = NULL;
    char buf[MAXIMUM_PATH];
    ASSERT(priv_kernel32_GetModuleHandleW != NULL);
    if (_snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%S", name) < 0)
        return (*priv_kernel32_GetModuleHandleW)(name);
    NULL_TERMINATE_BUFFER(buf);
    acquire_recursive_lock(&privload_lock);
    mod = privload_lookup(buf);
    if (mod != NULL) {
        res = mod->base;
        LOG(GLOBAL, LOG_LOADER, 2, "%s: %s => " PFX "\n", __FUNCTION__, buf, res);
    }
    release_recursive_lock(&privload_lock);
    if (mod == NULL)
        return (*priv_kernel32_GetModuleHandleW)(name);
    else
        return (HMODULE)res;
}

FARPROC WINAPI
redirect_GetProcAddress(HMODULE modbase, const char *name)
{
    app_pc res = NULL;
    ASSERT(priv_kernel32_GetProcAddress != NULL);
    LOG(GLOBAL, LOG_LOADER, 2, "%s: " PFX "%s\n", __FUNCTION__, modbase, name);
    if (!drwinapi_redirect_getprocaddr((app_pc)modbase, name, &res))
        return (*priv_kernel32_GetProcAddress)(modbase, name);
    else
        return (FARPROC)convert_data_to_function(res);
}

static HMODULE
helper_LoadLibrary(const char *name)
{
    app_pc res = NULL;
    char buf[MAXIMUM_PATH];
    if (double_strrchr(name, DIRSEP, ALT_DIRSEP) == NULL) {
        /* The LoadLibrary docs say that ".dll" will be appended if it's
         * not there and there's no trialing ".".
         */
        size_t len = strlen(name);
        if (len > 0 &&
            !((name[len - 1] == '.' ||
               (len > 3 && (name[len - 3] == 'd' || name[len - 1] == 'D') &&
                (name[len - 2] == 'l' || name[len - 1] == 'L') &&
                (name[len - 1] == 'l' || name[len - 1] == 'L'))))) {
            if (_snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%s.dll", name) < 0) {
                set_last_error(ERROR_DLL_NOT_FOUND);
                return NULL;
            }
            NULL_TERMINATE_BUFFER(buf);
            name = buf;
        }
    }
    res = locate_and_load_private_library(name, false /*!reachable*/);
    if (res == NULL) {
        /* XXX: if private loader can't handle some feature (delay-load dll,
         * bound imports, etc.), we could have the private kernel32 call the
         * shared ntdll which will load the lib and put it in the private PEB's
         * loader list.  there may be some loader data in ntdll itself which is
         * shared though so there's a transparency risk: so better to just fail
         * and if it's important add the feature to our loader.
         */
        /* XXX: should set more appropriate error code */
        set_last_error(ERROR_DLL_NOT_FOUND);
        return NULL;
    } else
        return (HMODULE)res;
}

HMODULE WINAPI
redirect_LoadLibraryA(const char *name)
{
#ifndef STANDALONE_UNIT_TEST
    ASSERT(priv_kernel32_LoadLibraryA != NULL);
#endif
    LOG(GLOBAL, LOG_LOADER, 2, "%s: %s\n", __FUNCTION__, name);
    return helper_LoadLibrary(name);
}

HMODULE WINAPI
redirect_LoadLibraryW(const wchar_t *name)
{
    char buf[MAXIMUM_PATH];
#ifndef STANDALONE_UNIT_TEST
    ASSERT(priv_kernel32_LoadLibraryW != NULL);
#endif
    if (_snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%S", name) < 0)
        return (*priv_kernel32_LoadLibraryW)(name);
    NULL_TERMINATE_BUFFER(buf);
    LOG(GLOBAL, LOG_LOADER, 2, "%s: %s\n", __FUNCTION__, buf);
    return helper_LoadLibrary(buf);
}

HMODULE WINAPI
redirect_LoadLibraryExA(const char *name, HANDLE reserved, DWORD flags)
{
    /* XXX i#1063: ignoring flags for now */
    return redirect_LoadLibraryA(name);
}

HMODULE WINAPI
redirect_LoadLibraryExW(const wchar_t *name, HANDLE reserved, DWORD flags)
{
    /* XXX i#1063: ignoring flags for now */
    return redirect_LoadLibraryW(name);
}

BOOL WINAPI
redirect_FreeLibrary(HMODULE hLibModule)
{
    return (BOOL)unload_private_library((app_pc)hLibModule);
}

DWORD WINAPI
redirect_GetModuleFileNameA(HMODULE modbase, char *buf, DWORD bufcnt)
{
    privmod_t *mod;
    DWORD cnt = 0;
    acquire_recursive_lock(&privload_lock);
    mod = privload_lookup_by_base((app_pc)modbase);
    if (mod != NULL) {
        cnt = (DWORD)strlen(mod->path);
        if (cnt >= bufcnt) {
            cnt = bufcnt;
            set_last_error(ERROR_INSUFFICIENT_BUFFER);
        }
        strncpy(buf, mod->path, bufcnt);
        buf[bufcnt - 1] = '\0';
        LOG(GLOBAL, LOG_LOADER, 2, "%s: " PFX " => %s\n", __FUNCTION__, mod, mod->path);
    }
    release_recursive_lock(&privload_lock);
    if (mod == NULL) {
        /* XXX: should set more appropriate error code */
        set_last_error(ERROR_DLL_NOT_FOUND);
        return 0;
    } else
        return cnt;
}

DWORD WINAPI
redirect_GetModuleFileNameW(HMODULE modbase, wchar_t *buf, DWORD bufcnt)
{
    privmod_t *mod;
    DWORD cnt = 0;
    acquire_recursive_lock(&privload_lock);
    mod = privload_lookup_by_base((app_pc)modbase);
    if (mod != NULL) {
        cnt = (DWORD)strlen(mod->path);
        if (cnt >= bufcnt) {
            cnt = bufcnt;
            set_last_error(ERROR_INSUFFICIENT_BUFFER);
        }
        _snwprintf(buf, bufcnt, L"%s", mod->path);
        buf[bufcnt - 1] = L'\0';
        LOG(GLOBAL, LOG_LOADER, 2, "%s: " PFX " => %s\n", __FUNCTION__, mod, mod->path);
    }
    release_recursive_lock(&privload_lock);
    if (mod == NULL) {
        /* XXX: should set more appropriate error code */
        set_last_error(ERROR_DLL_NOT_FOUND);
        return 0;
    } else
        return cnt;
}

/* FIXME i#1063: add the rest of the routines in kernel32_redir.h under
 * Libraries
 */

/***************************************************************************
 * TESTS
 */

#ifdef STANDALONE_UNIT_TEST
static void
test_loading(void)
{
    HMODULE h;
    BOOL ok;

    h = redirect_LoadLibraryA("kernel32.dll");
    EXPECT(h != NULL, true);
    ok = redirect_FreeLibrary(h);
    EXPECT(ok, TRUE);

    h = redirect_LoadLibraryA("kernel32");
    EXPECT(h != NULL, true);
    ok = redirect_FreeLibrary(h);
    EXPECT(ok, TRUE);

    h = redirect_LoadLibraryW(L"advapi32");
    EXPECT(h != NULL, true);
    ok = redirect_FreeLibrary(h);
    EXPECT(ok, TRUE);
}

void
unit_test_drwinapi_kernel32_lib(void)
{
    print_file(STDERR, "testing drwinapi kernel32 lib-related routines\n");

    test_loading();
}
#endif
