/* **********************************************************
 * Copyright (c) 2012 Google, Inc.  All rights reserved.
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

/* Test AArch64 ISA features from a client. Reads 'Features' string from
 * /proc/cpuinfo then checks to see if proc_has_feature() correctly identifies
 * the FEATURE_<x> as supported.
 * TODO: Add similar tests for X86.
 */

#include "dr_api.h"
#include "client_tools.h"

#include <string.h>

#define MAX_FEATURES_LEN 2048

static int
read_hw_features(char *feat_str)
{
    FILE *cmd;
    size_t n;
    char feat_str_buff[MAX_FEATURES_LEN];

    cmd = popen("grep -i '^Features' /proc/cpuinfo | head -1", "r");
    if (cmd == NULL)
        return -1;

    if ((n = fread(feat_str_buff, 1, sizeof(feat_str_buff) - 1, cmd)) <= 0)
        return -1;

    feat_str_buff[n] = '\0';
    if (sscanf(feat_str_buff, "%[^\n]%*c", feat_str) != 1)
        return -1;

    pclose(cmd);

    return 1;
}

/*
 * FEATURE_PAUTH is identified by six different nibbles across two registers. We initially
 * check the API nibble but on Neoverse V1 hardware this is 0. Testing that the
 * FEATURE_PAUTH is recognized on a Neoverse V1 machine verifies that the other nibbles
 * are being checked correctly.
 */
static void
check_for_pauth()
{
    uint64 id_aa64isar1_el1 = 0, id_aa64isar2_el1 = 0;

    asm("mrs %0, ID_AA64ISAR1_EL1" : "=r"(id_aa64isar1_el1));

    asm(".inst 0xd5380640\n" /* mrs x0, ID_AA64ISAR2_EL1 */
        "mov %0, x0"
        : "=r"(id_aa64isar2_el1)
        :
        : "x0");

    ushort gpi = (id_aa64isar1_el1 >> 28) &
        0xF; /* IMPLEMENTATION DEFINED algorithm for generic code authentication */
    ushort gpa = (id_aa64isar1_el1 >> 24) &
        0xF; /* QARMA5 algorithm for generic code authentication */
    ushort api = (id_aa64isar1_el1 >> 8) &
        0xF; /* IMPLEMENTATION DEFINED algorithm for address authentication */
    ushort apa =
        (id_aa64isar1_el1 >> 4) & 0xF; /* QARMA5 algorithm for address authentication */
    ushort apa3 =
        (id_aa64isar2_el1 >> 12) & 0xF; /* QARMA3 algorithm for address authentication */
    ushort gpa3 = (id_aa64isar2_el1 >> 8) &
        0xF; /* QARMA3 algorithm for generic code authentication */

    /* If one of these conditions is met then FEATURE_PAUTH is implemented */
    if (apa >= 1 || api >= 1 || gpa == 1 || gpa == 1 || gpa3 == 1 || apa3 >= 1) {
        ASSERT(proc_has_feature(FEATURE_PAUTH));
    }
}

/* These features are supported by almost all base v8.0 h/w, i.e. at least one
 * of them will appear in /proc/cpuinfo's 'Features' string.
 */
static const char *test_features[] = { "aes",   "pmull",  "sha1",    "sha2", "crc32",
                                       "sve",   "sha512", "atomics", "bf16", "jscvt",
                                       "lrcpc", "sm3",    "sm4",     "i8mm", "rng",
                                       "fphp",  "" };

DR_EXPORT void
dr_init(client_id_t client_id)
{
    char feat_str[MAX_FEATURES_LEN];
    char *feat;

    check_for_pauth();

    if (read_hw_features(feat_str) == -1)
        dr_fprintf(STDERR, "Error retrieving 'Features' string from /proc/cpuinfo\n");

    feat = strtok(feat_str, " ");
    while (feat != NULL) {
        int i = 0;
        while (strcmp(test_features[i++], "") != 0) {
            if (strcmp(feat, "aes") == 0)
                ASSERT(proc_has_feature(FEATURE_AESX));
            else if (strcmp(feat, "pmull") == 0)
                ASSERT(proc_has_feature(FEATURE_PMULL));
            else if (strcmp(feat, "sha1") == 0)
                ASSERT(proc_has_feature(FEATURE_SHA1));
            else if (strcmp(feat, "sha2") == 0)
                ASSERT(proc_has_feature(FEATURE_SHA256));
            else if (strcmp(feat, "crc32") == 0)
                ASSERT(proc_has_feature(FEATURE_CRC32));
            else if (strcmp(feat, "sve") == 0)
                ASSERT(proc_has_feature(FEATURE_SVE));
            else if (strcmp(feat, "sha512") == 0)
                ASSERT(proc_has_feature(FEATURE_SHA512));
            else if (strcmp(feat, "atomics") == 0)
                ASSERT(proc_has_feature(FEATURE_LSE));
            else if (strcmp(feat, "bf16") == 0)
                ASSERT(proc_has_feature(FEATURE_BF16));
            else if (strcmp(feat, "jscvt") == 0)
                ASSERT(proc_has_feature(FEATURE_JSCVT));
            else if (strcmp(feat, "jscvt") == 0)
                ASSERT(proc_has_feature(FEATURE_LRCPC));
            else if (strcmp(feat, "sm3") == 0)
                ASSERT(proc_has_feature(FEATURE_SM3));
            else if (strcmp(feat, "sm4") == 0)
                ASSERT(proc_has_feature(FEATURE_SM4));
            else if (strcmp(feat, "i8mm") == 0)
                ASSERT(proc_has_feature(FEATURE_I8MM));
            else if (strcmp(feat, "rng") == 0)
                ASSERT(proc_has_feature(FEATURE_RNG));
            else if (strcmp(feat, "fphp") == 0)
                ASSERT(proc_has_feature(FEATURE_FP16));
        }
        feat = strtok(NULL, " ");
    }
}
