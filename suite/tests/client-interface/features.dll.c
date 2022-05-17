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

/* These features are supported by almost all base v8.0 h/w, i.e. at least one
 * of them will appear in /proc/cpuinfo's 'Features' string.
 */
const char *test_features[] = { "aes", "pmull", "sha1", "sha2", "crc32", "" };

int
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

DR_EXPORT void
dr_init(client_id_t client_id)
{
    char feat_str[MAX_FEATURES_LEN];
    char *feat;

    if (read_hw_features(feat_str) == -1)
        dr_fprintf(STDERR, "Error retrieving 'Features' string from /proc/cpuinfo\n");

    feat = strtok(feat_str, " ");
    while (feat != NULL) {
        int i = 0;
        while (strcmp(test_features[i++], "") != 0) {
            if (strcmp(feat, "aes") == 0)
                ASSERT(proc_has_feature(FEATURE_AESX));
            if (strcmp(feat, "pmull") == 0)
                ASSERT(proc_has_feature(FEATURE_PMULL));
            if (strcmp(feat, "sha1") == 0)
                ASSERT(proc_has_feature(FEATURE_SHA1));
            if (strcmp(feat, "sha2") == 0)
                ASSERT(proc_has_feature(FEATURE_SHA256));
            if (strcmp(feat, "crc32") == 0)
                ASSERT(proc_has_feature(FEATURE_CRC32));
        }
        feat = strtok(NULL, " ");
    }
}
