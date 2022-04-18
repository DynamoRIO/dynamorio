/* **********************************************************
 * Copyright (c) 2015-2022 Google, Inc.  All rights reserved.
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
#include <string>
#include <stdexcept>
#ifdef LINUX
#    define _GNU_SOURCE 1
#    define __USE_GNU 1
#    include <link.h> /* struct dl_phdr_info */
#endif
#ifdef WINDOWS
#    pragma warning(disable : 4100) /* 'id' : unreferenced formal parameter */
#    pragma warning(disable : 4702) /* unreachable code */
#endif

#ifdef LINUX
static int
dl_iterate_cb(struct dl_phdr_info *info, size_t size, void *data)
{
#    ifdef VERBOSE
    dr_printf("dl_iterate_cb: addr=" PFX " hdrs=" PFX " num=%d name=%s\n",
              info->dlpi_addr, info->dlpi_phdr, info->dlpi_phnum, info->dlpi_name);
#    endif
    return 0; /* continue */
}
#endif

DR_EXPORT
void
dr_init(client_id_t id)
{
#ifdef LINUX
    int res = dl_iterate_phdr(dl_iterate_cb, NULL);
    dr_printf("dl_iterate_phdr returned %d\n", res);
#else
    /* make expect file cross-platform */
    dr_printf("dl_iterate_phdr returned 0\n");
#endif

    try {
        dr_printf("about to throw\n");
        throw std::runtime_error("test throw");
        dr_printf("should not get here\n");
    } catch (std::runtime_error &e) {
        dr_printf("caught runtime_error %s\n", e.what());
    }

    bool ok;
    /* test DR_TRY_EXCEPT */
    DR_TRY_EXCEPT(
        dr_get_current_drcontext(),
        {
            ok = false;
            *((int *)4) = 42;
        },
        { /* EXCEPT */
          ok = true;
        });
    if (!ok)
        dr_printf("DR_TRY_EXCEPT failure\n");

    dr_printf("all done\n");
}
