/* **********************************************************
 * Copyright (c) Bryan Berns  All rights reserved.
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

#include "dr_api.h"
#include "client_tools.h"
#include <wchar.h>

static app_pc check_language;
static int checks;

static void
event_thread_init(void *dc)
{
    ULONG count;
    ASSERT(SetThreadPreferredUILanguages(MUI_LANGUAGE_NAME, L"en-US\0", &count));
    ASSERT(count == 1);
}

static void
check_private_language(void)
{
    ULONG count, size = 128;
    WCHAR languages[128];
    ASSERT(GetThreadPreferredUILanguages(MUI_LANGUAGE_NAME, &count, languages, &size));
    ASSERT(count >= 1 && wcscmp(languages, L"en-US") == 0);
    ASSERT(HeapValidate(GetProcessHeap(), 0, NULL));
    ++checks;
}

static dr_emit_flags_t
event_bb(void *dc, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    if (dr_fragment_app_pc(tag) == check_language)
        dr_insert_clean_call(dc, bb, instrlist_first(bb), check_private_language, false,
                             0);
    return DR_EMIT_DEFAULT;
}

static void
event_exit(void)
{
    ASSERT(checks == 3);
    dr_fprintf(STDERR, "private languages preserved\n");
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    module_data_t *app = dr_get_main_module();
    check_language = (app_pc)dr_get_proc_address(app->handle, "check_language");
    ASSERT(check_language != NULL);
    dr_free_module_data(app);
    dr_register_thread_init_event(event_thread_init);
    dr_register_bb_event(event_bb);
    dr_register_exit_event(event_exit);
}
