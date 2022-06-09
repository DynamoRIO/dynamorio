/* **********************************************************
 * Copyright (c) 2022 Google, Inc.  All rights reserved.
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

/**/

#ifndef _DRIPT_DECODER_H_
#define _DRIPT_DECODER_H_ 1

#include <string>
#include <vector>

#include "intel-pt.h"
#include "libipt-sb.h"

struct drpt_decoder_config_t {
    std::string cpu;
    std::string trace_file;
    std::vector<std::string> preload_image_config;

    std::string sb_sample_type;
    std::string kcore_file;
    uint64_t kernel_start;

    std::string sb_primary_file;
    std::vector<std::string> sb_secondary_files;
};

class drpt_decoder_t {
public:
    drpt_decoder_t();
    ~drpt_decoder_t();

    bool
    init(const drpt_decoder_config_t &config);

    bool
    decode();

private:
    bool
    load_preload_image_file(std::string &filepath, uint64_t foffset, uint64_t fsize,
                            uint64_t base);
    bool
    load_trace_file(std::string &filepath);

    bool
    alloc_instr_decoder();

    bool
    alloc_sb_decoder(char *filename);

    bool
    update_image(struct pt_image *image);

    /* libipt configuration */
    struct pt_config config_;

    /* libipt instruction decoder*/
    struct pt_insn_decoder *instr_decoder_;

    /* The image section cache. */
    struct pt_image_section_cache *iscache_;

    /* The preload image. */
    struct pt_image *preload_image_;

    /* The perf event sideband decoder configuration. */
    struct pt_sb_pevent_config sb_pevent_config_;

    /* The sideband session. */
    struct pt_sb_session *sb_session_;
}

#endif /* _DRIPT_DECODER_H_ */
