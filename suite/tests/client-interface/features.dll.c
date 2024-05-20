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

struct feature_strings {
    const char *feature_name;
    ushort feature_code;
};

/*  These features appear in /proc/cpuinfo's 'Features' string on a Neoverse V1 machine.
 */
static const struct feature_strings features[] = {
    { "aes", FEATURE_AESX },      { "pmull", FEATURE_PMULL }, { "sha1", FEATURE_SHA1 },
    { "sha2", FEATURE_SHA256 },   { "crc32", FEATURE_CRC32 }, { "sve", FEATURE_SVE },
    { "sha512", FEATURE_SHA512 }, { "atomics", FEATURE_LSE }, { "bf16", FEATURE_BF16 },
    { "jscvt", FEATURE_JSCVT },   { "lrcpc", FEATURE_LRCPC }, { "sm3", FEATURE_SM3 },
    { "sm4", FEATURE_SM4 },       { "i8mm", FEATURE_I8MM },   { "rng", FEATURE_RNG },
    { "fphp", FEATURE_FP16 },     { "mte", FEATURE_MTE2 },
};

DR_EXPORT void
dr_init(client_id_t client_id)
{
    char feat_str[MAX_FEATURES_LEN];
    char *feat;

    check_for_pauth();

    if (read_hw_features(feat_str) == -1)
        dr_fprintf(STDERR, "Error retrieving 'Features' string from /proc/cpuinfo\n");

    int num_features_to_check = sizeof(features) / sizeof(*features);

    feat = strtok(feat_str, " ");
    while (feat != NULL) {
        for (int i = 0; i < num_features_to_check; i++) {
            if (strcmp(feat, features[i].feature_name) == 0) {
                ASSERT(proc_has_feature(features[i].feature_code));
                break;
            }
        }
        feat = strtok(NULL, " ");
    }
}
