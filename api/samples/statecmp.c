/* **********************************************************
 * Copyright (c) 2021-Google, Inc.  All rights reserved.
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

/* Illustrates use of the drstatecmp extension by a sample client.
 * This sample client is not meant for comprehensive testing.
 */

#include "dr_api.h"
#include "drstatecmp.h"

static void
event_exit(void)
{
    drstatecmp_exit();
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_set_client_name("DynamoRIO Sample Client 'statecmp'",
                       "http://dynamorio.org/issues");
    /* Make it easy to tell, by looking at log file, which client executed. */
    dr_log(NULL, DR_LOG_ALL, 1, "Client 'statecmp' initializing\n");
    /* To enable state comparison checks by the drstatecmp library, a client simply needs
     * to initially invoke drstatecmp_init() and drstatecmp_exit() on exit.
     * The invocation of drstatecmp_init() registers callbacks that insert
     * machine state comparison checks in the code. Assertions will trigger if
     * there is any state mismatch indicating a instrumentation-induced clobbering.
     * Invoking drstatecmp_exit() unregisters the drstatecmp's callbacks and frees up the
     * allocated thread-local storage.
     */
    drstatecmp_init();
    dr_register_exit_event(event_exit);
}
