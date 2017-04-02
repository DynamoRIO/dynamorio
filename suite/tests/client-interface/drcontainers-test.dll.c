/* **********************************************************
 * Copyright (c) 2013-2016 Google, Inc.  All rights reserved.
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


/* Tests the drcontainers extension */

#include "dr_api.h"
#include "drvector.h"

#define CHECK(x, msg) do {               \
    if (!(x)) {                          \
        dr_fprintf(STDERR, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, msg); \
        dr_abort();                      \
    }                                    \
} while (0);

static void
test_vector(void)
{
    drvector_t vec;
    bool ok = drvector_init(&vec, 0, false/*!synch*/, NULL);
    CHECK(ok, "drvector_init failed");
    CHECK(vec.entries == 0, "should start empty");

    drvector_append(&vec, (void *)&vec);
    CHECK(vec.entries == 1, "should add 1");
    CHECK(drvector_get_entry(&vec, 0) == (void *)&vec, "entries not equal");
    CHECK(vec.array[0] == (void *)&vec, "entries not equal");

    drvector_set_entry(&vec, 4, (void *)&vec);
    CHECK(vec.entries == 5, "should add 1");
    CHECK(drvector_get_entry(&vec, 4) == (void *)&vec, "entries not equal");
    CHECK(vec.array[4] == (void *)&vec, "entries not equal");

    drvector_append(&vec, (void *)&vec);
    CHECK(vec.entries == 6, "should add beyond last-set index");
    CHECK(drvector_get_entry(&vec, 5) == (void *)&vec, "entries not equal");
    CHECK(vec.array[5] == (void *)&vec, "entries not equal");

    /* Test for i#1981 */
    drvector_set_entry(&vec, 0, NULL);
    CHECK(drvector_get_entry(&vec, 5) == (void *)&vec, "set messed up later entry");

    /* XXX: test other features like free_data_func */

    ok = drvector_delete(&vec);
    CHECK(ok, "drvector_delete failed");
}

DR_EXPORT void
dr_init(client_id_t id)
{
    test_vector();

    /* XXX: test other data structures */
}
