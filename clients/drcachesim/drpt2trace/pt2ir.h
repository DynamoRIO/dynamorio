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

/* pt2ir: convert an PT raw trace to DynamoRIO's IR format. */

#ifndef _PT2IR_H_
#define _PT2IR_H_ 1

/**
 * @file drpt2trace/pt2ir.h
 * @brief Offline PT raw trace converter.
 */

#include <string>
#include <vector>

#ifndef IN
#    define IN // nothing
#endif
#ifndef OUT
#    define OUT // nothing
#endif
#ifndef INOUT
#    define INOUT // nothing
#endif

/**
 * The struct pt2ir_config_t is a collection of one PT trace's configurations. drpt2trace
 * and raw2trace can use it to construct pt2ir_t.
 * XXX: Multiple PT raw traces for a process will share one kcore dump file. We may need a
 * kernel image manager to avoid loading the same dump file many times.
 */
struct pt2ir_config_t {
    /* The class pt2ir_t does not want to expose libipt to the upper layer. So we redefine
     * some configurations of libipt decoder in pt_config. The libipt pt decoder requires
     * these parameters.
     */
    struct {
        /* A cpu identifier.*/
        struct {
            /* The vendor of the cpu(0 for unknown, 1 for Intel). */
            uint8_t vendor;

            /** The cpu family, modle and stepping */
            uint16_t family;
            uint8_t model;
            uint8_t stepping;
        } cpu;

        /* cpuid_0x15_eax and cpuid_0x15_ebx represent the CTC frequency*/
        uint32_t cpuid_0x15_eax;
        uint32_t cpuid_0x15_ebx;

        /* The MTC frequency. */
        uint8_t mtc_freq;

        /* The nominal frequency. */
        uint8_t nom_freq;
    } pt_config;

    /* The PT raw trace file path. */
    std::string raw_file_path;

    /* The class pt2ir_t does not want to expose libipt-sb to the upper layer too. So we
     * redefine some configurations of libipt-sb in sb_config. The libipt-sb sideband
     * session requires these parameters.
     */
    struct {
        /* perf_event_attr.sample_type */
        uint64_t sample_type;

        /* The start address of kernel. The sideband session use it to distinguish kernel
         * from user addresses: kernel >= @kernel_start user   <  @kernel_start
         */
        uint64_t kernel_start;

        /* The path of the kernel dump file. */
        std::string kcore_path;

        /* The sysroot is used for remote trace decoding. If the image locates at
         * /path/to/image in remote machine, it will load it from ${sysroot}/path/to/image
         * in local machine.
         */
        std::string sysroot;

        /* The values of time_shift, time_mult, and time_zero are used to synchronize
         * trace time, and the sideband recodes time.
         *
         *   time_shift = perf_event_mmap_page.ttime_shift
         *   time_mult = perf_event_mmap_page.time_mult
         *   time_zero = perf_event_mmap_page.time_zero
         */
        uint16_t time_shift;
        uint32_t time_mult;
        uint64_t time_zero;

        /* If the trace contains coarse timing information, the sideband events may be
         * applied too late to cause "no memory mmapped at this address" issue. To set the
         * tsc_offset can fix the issue.
         * XXX: The libipt documentation said we need to set a suitable value for
         * tsc_offset. But it didn't give a method to get a suitable value. So we may need
         * to find a way to generate the suitable tsc_offset.
         */
        uint64_t tsc_offset;
    } sb_config;

    /* sb_primary_file_path is the path of the primary sideband file.
     * sb_secondary_file_path_list is a list of paths to secondary sideband files.
     * The sideband primary file and secondary files are stored perf event records. A
     * primary sideband file contain perf event records for the traced cpu.  A secondary
     * sideband contain perf event records for other cpus on the system.
     */
    std::string sb_primary_file_path;
    std::vector<std::string> sb_secondary_file_path_list;
};

struct pt_image_section_cache;
struct pt_sb_pevent_config;
struct pt_sb_session;
struct pt_insn_decoder;
struct pt_ins;

/**
 * pt2ir_t is a class that can convert PT raw trace to DynamoRIO's IR.
 * It use libipt and libipt-sb to decode the PT raw trace. Then it use libdrdecode to
 * convert libipt's IR(struct pt_insn) to DynamoRIO's IR(instr_t).
 */
class pt2ir_t {
public:
    pt2ir_t(IN const pt2ir_config_t config);
    ~pt2ir_t();

    /* The convert function performs two processes: (1) decode the PT raw trace into
     * libipt's IR format pt_insn; (2) convert pt_insn into the doctor's IR format
     * instr_t. */
    void
    convert();

    /* Get the count number of instructions in the converted IR. */
    uint64_t
    get_instr_count();

    /* Print the disassembled text of all valid instructions to STDOUT. */
    void
    print_instrs_to_stdout();

private:
    /* Parse struct pt2ir_config_t and initialize PT instruction decoder, the sideband
     * session, and images caches. */
    bool
    init();

    /* Load PT raw file to buffer. The struct pt_insn_decoder will decode this buffer to
     * libipt's IR. */
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

    /* The config of pt2ir_t. */
    pt2ir_config_t config_;

    /* Buffer for caching the PT raw trace. */
    void *pt_raw_buffer_;
    size_t pt_raw_buffer_size_;

    /* The libipt instruction decoder. */
    struct pt_insn_decoder *pt_instr_decoder_;

    /* The libipt image section cache. */
    struct pt_image_section_cache *pt_iscache_;

    /* The libipt sideband session. */
    struct pt_sb_session *pt_sb_session_;

    /* All valid instructions that can be decoded from PT raw trace. */
    std::vector<struct pt_insn> pt_instr_list_;
};

#endif /* _PT2IR_H_ */
