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

/* The struct 'pt2ir_config_t' is the config of class 'pt2ir_t'. It can be used in
 * drpt2trace and raw2trace to construct 'pt2ir_t' type objects.
 */
struct pt2ir_config_t {
    /* A cpu identifier. */
    struct pt_cpu cpu;

    /* The PT raw data file path. */
    std::string raw_file_path;

    /* perf_event_attr.sample_type */
    uint64_t sample_type;

    /* The values of time_shift, time_mult, time_zero, and tsc_offset are used to
     * synchronize trace time, and the sideband recodes time.
     */
    /* perf_event_mmap_page.ttime_shift */
    uint16_t time_shift;

    /* perf_event_mmap_page.time_mult */
    uint32_t time_mult;

    /* perf_event_mmap_page.time_zero */
    uint64_t time_zero;

    /* If the trace contains coarse timing information, the sideband events may be applied
     * too late to cause "no memory mmapped at this address" issue. To set the tsc_offset
     * can fix the issue.
     * XXX: The libipt documentation said we need to set a suitable value for tsc_offset.
     * But it didn't give a method to get a suitable value. So we may need to find a way
     * to generate the suitable tsc_offset.
     */
    uint64_t tsc_offset;

    /* The sysroot is used for remote trace decoding. If the image locates at
     * /path/to/image in remote machine, it will load it from  ${sysroot}/path/to/image in
     * local machine.
     */
    std::string sysroot;


    std::string sb_primary_file;
    std::vector<std::string> sb_secondary_files;

    std::string kernel_start;
    std::string kcore_path;
};

class pt2ir_t {
public:
    pt2ir_t(IN const pt2ir_config_t config);
    ~pt2ir_t();

    /* The convert function performs two processes: (1) decode the PT raw data into
     * libipt's IR format pt_insn; (2) convert pt_insn into the doctor's IR format
     * instr_t. */
    void
    convert();

    /* Get the count number of instructions in the converted IR. */
    uint64_t
    get_instr_count();

    /* Print the disassembled text of all valid instructions that can be decoded from PT
     * raw data. */
    void
    print_instrs(IN file_t outfile);

private:
    /* Parse the 'pt2ir_config_t' and initialize PT instruction decoder, the sideband session, and images caches. */
    bool
    init();

    /* Load PT raw file to buffer. The 'pt_insn_decoder' will decode this buffer to 'pt_insn_t' s. */
    bool
    load_pt_raw_file(IN std::string &path);

    /* Load the elf section in kcore to sideband session iscache and store the section
     * index to sideband kimage.
     * XXX: Could we not use kcore? We can store all kernel modules and the mapping
     * information.
     */
    bool
    load_kernel_image(IN std::string &path);

    /* Allocate a sideband decoder in the sideband session. The sideband session may
     * allocate many decoders, which mainly work on handling sideband perf records and
     * help the PT decoder switch images. */
    bool
    alloc_sb_pevent_decoder(IN struct pt_sb_pevent_config &config);

    /* Diagnose converting errors and output diagnostic results.
     * It will used to generate the error message during the decoding process.
     */
    void
    dx_decoding_error(IN int errcode, IN const char *errtype, IN uint64_t ip);

    /* The config of 'pt2ir_t'. */
    pt2ir_config_t config_;

    /* Buffer for caching the PT raw data. */
    void *pt_raw_buffer_;
    size_t pt_raw_buffer_size_;

    /* The libipt instruction decoder. */
    struct pt_insn_decoder *pt_instr_decoder_;

    /* The libipt image section cache. */
    struct pt_image_section_cache *pt_iscache_;

    /* The libipt sideband session. */
    struct pt_sb_session *pt_sb_session_;

    /* All valid instructions that can be decoded from PT raw data. */
    std::vector<struct pt_insn> pt_instr_list_;
};

#endif /* _PT2IR_H_ */
