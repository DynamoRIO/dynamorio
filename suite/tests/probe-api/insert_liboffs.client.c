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

/* This client registers a probe with library offset. */

/* To prevent cl from complaining about calling standard c library routine. */
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_DEPRECATE

#include "dr_api.h"
#include "dr_probe.h"
#include "dr_tools.h"

#define NUM_PROBES 1
dr_probe_desc_t probes[NUM_PROBES];

/*----------------------------------------------------------------------------*/
#include "dr_defines.h"

/* This probe increments the argument to insert_liboffs.c:doubler() */
__declspec(dllexport) void doubler_probe(dr_mcontext_t *cxt)
{
    volatile reg_t *arg_p = (reg_t *)(cxt->xsp + sizeof(reg_t));
    *arg_p = (*arg_p) + 1;
}
/*----------------------------------------------------------------------------*/
/* TODO: Figure out a way to share this code across all probe-api tests.  Can't
 * put it inside tools.c as it links against external libraries.  Ok for now as
 * this is the only one test.
 */
extern unsigned long
strtoul(const char *nptr, char **endptr, int base);
/* Library offset has to be computed before the probe library is loaded
 * into memory.  Reading it from the map file is one of the easiest ways to
 * do it.
 */
unsigned int
get_symbol_offset_from_map(const char *map_file, const char *symbol)
{
    const char *pref_addr_str = "Preferred load address is ";
    unsigned int pref_base, sym_addr, offset = 0xdeadc0de;
    ssize_t file_sz;
    file_t fd = INVALID_FILE;
    char *buf, *temp;

    fd = dr_open_file(map_file, DR_FILE_READ);
    if (fd == INVALID_FILE)
        goto _get_module_offset_exit;

    /* This seems to be the easiest way to get the size of the file. */
    if (!dr_file_seek(fd, 0, DR_SEEK_END))
        goto _get_module_offset_exit;
    file_sz = (ssize_t)dr_file_tell(fd);
    if (file_sz <= 0)
        goto _get_module_offset_exit;
    if (!dr_file_seek(fd, 0, DR_SEEK_SET))
        goto _get_module_offset_exit;

    /* Read the whole file. */
    buf = dr_global_alloc(file_sz + 1);
    if (buf == NULL)
        goto _get_module_offset_exit;
    dr_read_file(fd, buf, file_sz);
    buf[file_sz] = '\0';

    /* Locate preferred base & symbol address. */
    temp = strstr(buf, pref_addr_str);
    if (temp != NULL) {
        pref_base = strtoul(temp + strlen(pref_addr_str), NULL, 16);
        temp = strstr(buf, symbol);
        if (temp != NULL)
            sym_addr = strtoul(temp + strlen(symbol), NULL, 16);
        offset = sym_addr - pref_base;
    }

    dr_global_free(buf, file_sz + 1);

_get_module_offset_exit:
    if (fd != INVALID_FILE)
        dr_close_file(fd);
    return offset;
}

/* This routine builds the full map file path from the name of the client.
 * Assumes that map files for the test_{dll,exe}, probe and client dlls are all
 * in the same directory as the client, which is true for tests.
 */
unsigned int
get_symbol_offset(const char *map_file, const char *symbol)
{
    unsigned long offset = 0xdeadc0de, client_path_len;
    char *client_path = NULL, *map_path, *ptr;

    client_path = (char *)dr_get_client_path();
    if (client_path != NULL) {
        client_path_len = strlen(client_path);
        map_path = dr_global_alloc(client_path_len * 2); /* be safe */
        strcpy(map_path, client_path);
        ptr = map_path + client_path_len;
        while (*--ptr != '\\')
            ; /* get the dirname */
        strcpy(++ptr, map_file);
    }

    offset = get_symbol_offset_from_map(map_path, symbol);

    if (map_path != NULL)
        dr_global_free(map_path, client_path_len * 2);

    return offset;
}
/*----------------------------------------------------------------------------*/
static void
probe_def_init(void)
{
    probes[0].name = "insert_liboffs.exe probe";

    probes[0].insert_loc.type = DR_PROBE_ADDR_LIB_OFFS;
    probes[0].insert_loc.lib_offs.library = "insert_liboffs.exe";
    probes[0].insert_loc.lib_offs.offset =
        get_symbol_offset("insert_liboffs.map", "_doubler");

    probes[0].callback_func.type = DR_PROBE_ADDR_LIB_OFFS;
    probes[0].callback_func.lib_offs.library = "../probe-api/insert_liboffs.client.dll";
    /* probes[0].callback_func.lib_offs.library = "insert_liboffs.probe.dll"; */

    probes[0].callback_func.lib_offs.offset =
        get_symbol_offset("insert_liboffs.client.map", "_doubler_probe");
}

DR_EXPORT
void
dr_init()
{
    dr_probe_status_t stat;

    probe_def_init();
    dr_register_probes(&probes[0], NUM_PROBES);
    dr_get_probe_status(probes[0].id, &stat);
}
