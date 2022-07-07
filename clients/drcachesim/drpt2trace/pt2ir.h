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

/* pt2ir: convert a PT raw trace to DynamoRIO's IR format. */

#ifndef _PT2IR_H_
#define _PT2IR_H_ 1

/**
 * @file drmemtrace/pt2ir.h
 * @brief Offline PT raw trace converter.
 */

#include <string>
#include <vector>
#include <memory>
#define DR_FAST_IR 1
#include "dr_api.h"

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
 * The type of pt2ir_t::convert() return value.
 */
enum pt2ir_convert_status_t {
    /** The conversion process is successful. */
    PT2IR_CONV_SUCCESS = 0,

    /** The conversion process ends with a failure to sync to the PSB packet. */
    PT2IR_CONV_ERROR_SYNC_PACKET,

    /** The conversion process ends with a failure to handle a perf event. */
    PT2IR_CONV_ERROR_HANDLE_SIDEBAND_EVENT,

    /** The conversion process ends with a failure to get the pending event. */
    PT2IR_CONV_ERROR_GET_PENDING_EVENT,

    /** The conversion process ends with a failure to set the new image. */
    PT2IR_CONV_ERROR_SET_IMAGE,

    /** The conversion process ends with a failure to decode the next intruction. */
    PT2IR_CONV_ERROR_DECODE_NEXT_INSTR,

    /**
     * The conversion process ends with a failure to convert the libipt's IR to
     * Dynamorio's IR.
     */
    PT2IR_CONV_ERROR_DR_IR_CONVERT
};

/**
 * The types of the CPU vendor.
 */
enum pt_cpu_vendor_t {
    CPU_VENDOR_UNKNOWN = 0, /**< The CPU vendor is unknown. */
    CPU_VENDOR_INTEL        /**< The CPU vendor is Intel. */
};

/**
 * The types of the CPU model.
 */
struct pt_cpu_t {
    pt_cpu_vendor_t vendor; /**< The vendor of the CPU. */
    uint16_t family;        /**< The CPU family. */
    uint8_t model;          /**< The CPU mode. */
    uint8_t stepping;       /**< The CPU stepping. */
};

/**
 * The type of PT raw trace's libipt config.
 * \note The class pt2ir_t does not want to expose libipt to the upper layer. So we
 * redefine some configurations of libipt decoder in pt_config. The libipt pt decoder
 * requires these parameters. We can get these parameters by running
 * libipt/scirpts/perf-get-opts.bash.
 */
struct pt_config_t {
    pt_cpu_t cpu; /**< The CPU identifier. */

    /** The value of cpuid[0x15].eax. It represents the CTC frequency. */
    uint32_t cpuid_0x15_eax;

    /** The value of cpuid[0x15].ebx. It represents the CTC frequency. */
    uint32_t cpuid_0x15_ebx;
    uint8_t mtc_freq; /**< The MTC frequency. */
    uint8_t nom_freq; /**< The nominal frequency. */
};

/**
 * The type of PT raw trace's libipt-sb config.
 * \note The class pt2ir_t does not want to expose libipt-sb to the upper layer too.
 * So we redefine some configurations of libipt-sb in sb_config. The libipt-sb
 * sideband session requires these parameters. We can get these parameters by running
 * libipt/scirpts/perf-get-opts.bash.
 */
struct pt_sb_config_t {
    /**
     * The value of perf_event_attr.sample_type.
     */
    uint64_t sample_type;

    /**
     * The start address of kernel. The sideband session use it to distinguish kernel
     * from user addresses: user < kernel_start < kernel.
     */
    uint64_t kernel_start;

    /**
     * The sysroot is used for remote trace decoding. If the image locates at
     * /path/to/image in remote machine, it will load it from ${sysroot}/path/to/image
     * in local machine.
     */
    std::string sysroot;

    /**
     * The time shift. It is used to synchronize trace time, and the sideband recodes
     * time.
     * \note time_shift = perf_event_mmap_page.time_shift
     */
    uint16_t time_shift;

    /**
     * The time multiplier. It is used to synchronize trace time, and the sideband
     * recodes time.
     * \note time_mult = perf_event_mmap_page.time_mult
     */
    uint32_t time_mult;

    /**
     * The time zero. It is used to synchronize trace time, and the sideband recodes
     * time.
     * \note time_zero = perf_event_mmap_page.time_zero
     */
    uint64_t time_zero;

    /* If the trace contains coarse timing information, the sideband events may be
     * applied too late to cause "no memory mmapped at this address" issue. To set the
     * tsc_offset can fix the issue.
     * \note XXX: The libipt documentation said we need to set a suitable value for
     * tsc_offset. But it didn't give a method to get a suitable value. So we may need
     * to find a way to generate the suitable tsc_offset.
     */
    uint64_t tsc_offset;
};

/**
 * The struct pt2ir_config_t is a collection of one PT trace's configurations. drpt2trace
 * and raw2trace can use it to construct pt2ir_t.
 * \note XXX: Multiple PT raw traces for a process will share one kcore dump file. We may
 * need a kernel image manager to avoid loading the same dump file many times.
 */
struct pt2ir_config_t {
    /**
     * The libipt config of PT raw trace.
     */
    pt_config_t pt_config;

    /**
     * The PT raw trace file path.
     */
    std::string raw_file_path;

    /**
     * The libipt-sb config of PT raw trace.
     */
    pt_sb_config_t sb_config;

    /**
     * The primary sideband file path. A primary sideband file contains perf event records
     * for the traced CPU.
     */
    std::string sb_primary_file_path;

    /**
     * The secondary sideband file paths of PT raw trace. A secondary sideband file
     * contains perf event records for other CPUs on the system.
     */
    std::vector<std::string> sb_secondary_file_path_list;

    /**
     * The path of the kernel dump file. The kernel dump file is used to decode the kernel
     * PT raw trace.
     */
    std::string kcore_path;
};

struct pt_image_section_cache;
struct pt_sb_pevent_config;
struct pt_sb_session;
struct pt_insn_decoder;
struct pt_ins;

/**
 * pt2ir_t is a class that can convert PT raw trace to DynamoRIO's IR.
 */
class pt2ir_t {
public:
    pt2ir_t();
    ~pt2ir_t();

    /**
     * Returns true if the pt2ir_t is successfully initialized. Returns false on failure.
     * \note Parse struct pt2ir_config_t and initialize PT instruction decoder, the
     * sideband session, and images caches.
     */
    bool
    init(IN pt2ir_config_t &pt2ir_config);

    /**
     * Returns pt2ir_convert_status_t. If the convertion is successful, the function
     * returns PT2IR_CONV_SUCCESS. Otherwise, the function returns the corresponding error
     * code.
     * \note The convert function performs two processes:
     * (1) decode the PT raw trace into libipt's IR format pt_insn;
     * (2) convert pt_insn into the DynamoRIO's IR format instr_t and append it to ilist.
     * \note The caller does not need to initialize ilist. But if the convertion is
     * successful, the caller needs to destory the ilist.
     */
    pt2ir_convert_status_t
    convert(OUT instrlist_t **ilist);

private:
    /* Load PT raw file to buffer. The struct pt_insn_decoder will decode this buffer to
     * libipt's IR.
     */
    bool
    load_pt_raw_file(IN std::string &path);

    /* Load the elf section in kcore to sideband session iscache and store the section
     * index to sideband kimage.
     * \note XXX: Could we not use kcore? We can store all kernel modules and the mapping
     * information.
     */
    bool
    load_kernel_image(IN std::string &path);

    /* Allocate a sideband decoder in the sideband session. The sideband session may
     * allocate many decoders, which mainly work on handling sideband perf records and
     * help the PT decoder switch images.
     */
    bool
    alloc_sb_pevent_decoder(IN struct pt_sb_pevent_config *config);

    /* Diagnose converting errors and output diagnostic results.
     * It will used to generate the error message during the decoding process.
     */
    void
    dx_decoding_error(IN int errcode, IN const char *errtype, IN uint64_t ip);

    /* Buffer for caching the PT raw trace. */
    std::unique_ptr<unsigned char> pt_raw_buffer_;
    size_t pt_raw_buffer_size_;

    /* The libipt instruction decoder. */
    struct pt_insn_decoder *pt_instr_decoder_;

    /* The libipt image section cache. */
    struct pt_image_section_cache *pt_iscache_;

    /* The libipt sideband session. */
    struct pt_sb_session *pt_sb_session_;
};

#endif /* _PT2IR_H_ */
