/* **********************************************************
 * Copyright (c) 2011-2017 Google, Inc.   All rights reserved.
 * Copyright (c) 2009-2010 Derek Bruening   All rights reserved.
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

/* rpcrt4 redirection routines */

#include "../../globals.h"
#include "drwinapi.h"
#include "drwinapi_private.h"
#include "rpcrt4_redir.h"

#ifndef WINDOWS
#    error Windows-only
#endif

/* We use a hashtale for faster lookups than a linear walk */
static strhash_table_t *rpcrt4_table;

static const redirect_import_t redirect_rpcrt4[] = {
    { "UuidCreate", (app_pc)redirect_UuidCreate },
};
#define REDIRECT_RPCRT4_NUM (sizeof(redirect_rpcrt4) / sizeof(redirect_rpcrt4[0]))

void
rpcrt4_redir_init(void)
{
    uint i;
    rpcrt4_table =
        strhash_hash_create(GLOBAL_DCONTEXT, hashtable_num_bits(REDIRECT_RPCRT4_NUM * 2),
                            80 /* load factor: not perf-critical, plus static */,
                            HASHTABLE_SHARED | HASHTABLE_PERSISTENT,
                            NULL _IF_DEBUG("rpcrt4 redirection table"));
    TABLE_RWLOCK(rpcrt4_table, write, lock);
    for (i = 0; i < REDIRECT_RPCRT4_NUM; i++) {
        strhash_hash_add(GLOBAL_DCONTEXT, rpcrt4_table, redirect_rpcrt4[i].name,
                         (void *)redirect_rpcrt4[i].func);
    }
    TABLE_RWLOCK(rpcrt4_table, write, unlock);
}

void
rpcrt4_redir_exit(void)
{
    strhash_hash_destroy(GLOBAL_DCONTEXT, rpcrt4_table);
}

void
rpcrt4_redir_onload(privmod_t *mod)
{
    /* nothing yet */
}

app_pc
rpcrt4_redir_lookup(const char *name)
{
    app_pc res;
    TABLE_RWLOCK(rpcrt4_table, read, lock);
    res = strhash_hash_lookup(GLOBAL_DCONTEXT, rpcrt4_table, name);
    TABLE_RWLOCK(rpcrt4_table, read, unlock);
    return res;
}

RPC_STATUS RPC_ENTRY
redirect_UuidCreate(__out UUID __RPC_FAR *Uuid)
{
    if (Uuid == NULL)
        return RPC_S_INVALID_ARG;
    /* This is based on RFC 4122.  We're using pseudo-random numbers, so
     * we follow Sec 4.4 of that RFC.
     */
    Uuid->Data1 = (ulong)get_random_offset(UINT_MAX);
    Uuid->Data2 = (ushort)get_random_offset(USHRT_MAX);
    Uuid->Data3 = (ushort)get_random_offset(USHRT_MAX);
    *((ulong *)&Uuid->Data4[0]) = (ulong)get_random_offset(UINT_MAX);
    *((ulong *)&Uuid->Data4[4]) = (ulong)get_random_offset(UINT_MAX);

    Uuid->Data4[0] &= 0xbf; /* clear bit 6 of clock_seq_hi_and_reserved */
    Uuid->Data4[0] |= 0x80; /* set bit 7 of clock_seq_hi_and_reserved */

    /* set bits 12-15 of time_hi_and_version to 4 to indicate "pseudo-random" */
    Uuid->Data3 &= 0x0fff;
    Uuid->Data3 |= 0x4000;

    return RPC_S_OK;
}

#ifdef STANDALONE_UNIT_TEST
void
unit_test_drwinapi_rpcrt4(void)
{
    /* Hard to test we're getting unique-looking ids.  For now we settle for
     * these simple tests.
     */
    UUID id = {
        0,
    };
    UUID id2 = {
        0,
    };
    RPC_STATUS res;

    print_file(STDERR, "testing drwinapi rpcrt4\n");

    res = redirect_UuidCreate(NULL);
    EXPECT(res == RPC_S_INVALID_ARG, true);
    res = redirect_UuidCreate(&id);
    EXPECT(res == RPC_S_OK, true);
    EXPECT((id.Data4[0] & 0xc0) == 0x80, true);
    res = redirect_UuidCreate(&id2);
    EXPECT(res == RPC_S_OK, true);
    EXPECT(memcmp(&id, &id2, sizeof(id)) != 0, true);
}
#endif
