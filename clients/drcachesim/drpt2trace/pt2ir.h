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

#ifndef _PT2IR_H_
#define _PT2IR_H_ 1

#include <string>
#include <vector>

#include "intel-pt.h"
#include "libipt-sb.h"

#ifndef IN
#    define IN // nothing
#endif
#ifndef OUT
#    define OUT // nothing
#endif
#ifndef INOUT
#    define INOUT // nothing
#endif

typedef struct _pt2ir_config_t {
    std::string cpu;
    std::string sample_type;
    std::string sysroot;
    uint64_t kernel_start;

    std::string raw_file_path;
    std::string kcore_path;
    std::string sb_primary_file;
    std::vector<std::string> sb_secondary_files;
} pt2ir_config_t;

class pt2ir_t {
public:
    pt2ir_t(IN const pt2ir_config_t &config, IN unsigned int verbosity);
    ~pt2ir_t();

    void
    process_decode();

private:
    bool
    parse_config();

    bool
    load_pt_raw_file(IN std::string &raw_file_path);

    /* Load the elf section in kcore to sideband session iscache and store the section
     * index to sideband kimage.
     * XXX: Could we not use kcore? We can store all kernel modules and the mapping
     * information.
     */
    bool
    load_kernel_image(IN const char* kcore_path);

    bool
    alloc_sb_pevent_decoder(IN struct pt_sb_pevent_config &config);

    /* Diagnose decoding errors and output diagnostic results.
     * It will used to generate the error message during the decoding process.
     */
    void
    dx_decoding_error(IN int errcode, IN const char *errtype, IN uint64_t ip);

private:
    /* The config of pt2ir_t. */
    pt2ir_config_t config_;

    /* Buffer for caching the pt raw file. */
    uint8_t *pt_raw_buffer_;

    size_t pt_raw_buffer_size_;

    /* The libipt instruction decoder. */
    struct pt_insn_decoder *pt_instr_decoder_;

    /* The libipt image section cache. */
    struct pt_image_section_cache *pt_iscache_;

    /* The libipt sideband session. */
    struct pt_sb_session *pt_sb_session_;
}

#endif /* _PT2IR_H_ */
