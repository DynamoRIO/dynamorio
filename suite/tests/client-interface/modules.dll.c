/* **********************************************************
 * Copyright (c) 2007-2008 VMware, Inc.  All rights reserved.
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

#include "dr_api.h"

#ifdef WINDOWS
# define IF_WINDOWS_ELSE(x, y) x
# define IF_WINDOWS(x) x
#else
# define IF_WINDOWS_ELSE(x, y) y
# define IF_WINDOWS(x) 
#endif

bool string_match(const char *str1, const char *str2)
{
    if (str1 == NULL || str2 == NULL)
        return false;

    while (*str1 == *str2) {
        if (*str1 == '\0')
            return true;
        str1++;
        str2++;
    }

    return false;
}

static
void module_load_event(void *dcontext, const module_data_t *data, bool loaded)
{
    /* It's easier to simply print all module loads and unloads, but
     * it appears that loading a module like advapi32.dll causes
     * different modules to load on different windows versions.  Even
     * worse, they seem to be loaded in a different order for
     * different runs.  For the sake of making this test robust, I'll
     * just look for the module in question.
     */
    /* Test i#138 */
    if (data->full_path == NULL || data->full_path[0] == '\0')
        dr_fprintf(STDERR, "ERROR: full_path empty for %s\n", dr_module_preferred_name(data));
#ifdef WINDOWS
    /* We do not expect \\server-style paths for this test */
    else if (data->full_path[0] == '\\' || data->full_path[1] != ':')
        dr_fprintf(STDERR, "ERROR: full_path is not in DOS format: %s\n", data->full_path);
#endif
    if (string_match(data->names.module_name,
                     IF_WINDOWS_ELSE("ADVAPI32.dll", "libz.so.1")))
        dr_fprintf(STDERR, "LOADED MODULE: %s\n", data->names.module_name);
}

static
void module_unload_event(void *dcontext, const module_data_t *data)
{
    if (string_match(data->names.module_name,
                     IF_WINDOWS_ELSE("ADVAPI32.dll", "libz.so.1")))
        dr_fprintf(STDERR, "UNLOADED MODULE: %s\n", data->names.module_name);
}

static void
test_aux_lib(client_id_t id)
{
    const char *auxname = IF_WINDOWS_ELSE("client.events.dll.dll",
                                          "libclient.events.dll.so");
    char buf[MAXIMUM_PATH];
    char path[MAXIMUM_PATH];
    dr_auxlib_handle_t lib;
    char *sep;
    if (dr_snprintf(path, sizeof(path)/sizeof(path[0]), "%s", 
                    dr_get_client_path(id)) < 0) {
        dr_fprintf(STDERR, "ERROR printing to buffer\n");
        return;
    }
    sep = path;
    while (*sep != '\0')
        sep++;
    while (sep > path && *sep != '/' IF_WINDOWS(&& *sep != '\\'))
        sep--;
    *sep = '\0';
    if (dr_snprintf(buf, sizeof(buf)/sizeof(buf[0]), "%s/%s", path, auxname) < 0) {
        dr_fprintf(STDERR, "ERROR printing to buffer\n");
        return;
    }
    /* test loading an auxiliary library: just use another client lib */
    lib = dr_load_aux_library(buf, NULL, NULL);
    if (lib != NULL) {
        dr_auxlib_routine_ptr_t func = dr_lookup_aux_library_routine(lib, "dr_init");
        if (func != NULL) {
            if (!dr_memory_is_in_client((byte *)func)) {
                dr_fprintf(STDERR, "ERROR: aux lib "PFX" not considered client\n",
                           func);
            }
        } else
            dr_fprintf(STDERR, "ERROR: unable to find dr_init\n");
    } else
        dr_fprintf(STDERR, "ERROR: unable to load %s\n", buf);
    if (!dr_unload_aux_library(lib))
        dr_fprintf(STDERR, "ERROR: unable to unload %s\n", buf);
}

DR_EXPORT
void dr_init(client_id_t id)
{
    dr_register_module_load_event(module_load_event);
    dr_register_module_unload_event(module_unload_event);    
    test_aux_lib(id);
}

/* TODO - add more module interface tests. */
