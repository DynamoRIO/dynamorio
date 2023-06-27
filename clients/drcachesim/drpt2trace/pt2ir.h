/* **********************************************************
 * Copyright (c) 2023 Google, Inc.  All rights reserved.
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
 * @file pt2ir.h
 * @brief Offline PT raw trace converter.
 */

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <memory>
#include <mutex>
#ifndef DR_FAST_IR
#    define DR_FAST_IR 1
#endif
#include "dr_api.h"
#include "drir.h"
#include "elf_loader.h"

#ifndef IN
#    define IN // nothing
#endif
#ifndef OUT
#    define OUT // nothing
#endif
#ifndef INOUT
#    define INOUT // nothing
#endif

// libipt global types.
struct pt_config;
struct pt_image;
struct pt_image_section_cache;
struct pt_sb_pevent_config;
struct pt_sb_session;
struct pt_insn_decoder;
struct pt_packet_decoder;

namespace dynamorio {
namespace drmemtrace {

/**
 * The auto cleanup wrapper of struct pt_image_section_cache.
 * \note This can ensure the instance of pt_image_section_cache is cleaned up when it is
 * out of scope.
 */
struct pt_iscache_autoclean_t {
public:
    pt_iscache_autoclean_t();

    ~pt_iscache_autoclean_t();

    struct pt_image_section_cache *iscache = nullptr;
};

/**
 * The type of pt2ir_t::convert() return value.
 */
enum pt2ir_convert_status_t {
    /** The conversion process is successful. */
    PT2IR_CONV_SUCCESS = 0,

    /** The conversion process fail to initiate for invalid input. */
    PT2IR_CONV_ERROR_INVALID_INPUT,

    /** The conversion process failed to initiate because the instance was not
     *  initialized.
     */
    PT2IR_CONV_ERROR_NOT_INITIALIZED,

    /** The conversion process failed to initiate because it attempted to copy a large
     *  amount of PT data into the raw buffer that was too small to accommodate it.
     */
    PT2IR_CONV_ERROR_RAW_TRACE_TOO_LARGE,
    /** The conversion process ends with a failure to sync to the PSB packet. */
    PT2IR_CONV_ERROR_SYNC_PACKET,

    /** The conversion process ends with a failure to handle a perf event. */
    PT2IR_CONV_ERROR_HANDLE_SIDEBAND_EVENT,

    /** The conversion process ends with a failure to get the pending event. */
    PT2IR_CONV_ERROR_GET_PENDING_EVENT,

    /** The conversion process ends with a failure to set the new image. */
    PT2IR_CONV_ERROR_SET_IMAGE,

    /** The conversion process ends with a failure to decode the next instruction. */
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
 * The struct pt2ir_config_t contains configuration details for one or multiple PT traces
 * and is utilized by drpt2trace and raw2trace to generate a pt2ir_t instance.
 */
struct pt2ir_config_t {
public:
    /**
     * The libipt config of PT raw trace.
     */
    pt_config_t pt_config;

    /**
     * The raw buffer size of PT decoder.
     */
    uint64_t pt_raw_buffer_size;

    /**
     * The elf file path.
     */
    std::string elf_file_path;

    /**
     * The runtime load address of the elf file.
     */
    uint64_t elf_base;

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
     * The kernel dump file path refers to the location where the kernel dump file is
     * stored. This file is utilized by the sideband decoder for decoding the kernel PT
     * raw trace.
     */
    std::string sb_kcore_path;

    pt2ir_config_t()
    {
        pt_config.cpu.vendor = CPU_VENDOR_UNKNOWN;
        pt_config.cpu.family = 0;
        pt_config.cpu.model = 0;
        pt_config.cpu.stepping = 0;
        pt_config.cpuid_0x15_eax = 0;
        pt_config.cpuid_0x15_ebx = 0;
        pt_config.mtc_freq = 0;
        pt_config.nom_freq = 0;
        pt_raw_buffer_size = 0;
        elf_file_path = "";
        elf_base = 0;

        sb_config.sample_type = 0;
        sb_config.kernel_start = 0;
        sb_config.sysroot = "";
        sb_config.time_shift = 0;
        sb_config.time_mult = 1;
        sb_config.time_zero = 0;
        sb_config.tsc_offset = 0;
        sb_primary_file_path = "";
        sb_secondary_file_path_list.clear();
        sb_kcore_path = "";
    }

    /**
     * Return true if the config is successfully initialized.
     * This function is used to parse the metadata of the PT raw trace.
     */
    bool
    init_with_metadata(IN const void *metadata_buffer)
    {
        if (metadata_buffer == NULL)
            return false;

        struct {
            uint16_t cpu_family;
            uint8_t cpu_model;
            uint8_t cpu_stepping;
            uint16_t time_shift;
            uint32_t time_mult;
            uint64_t time_zero;
        } __attribute__((__packed__)) metadata;

        memcpy(&metadata, metadata_buffer, sizeof(metadata));

        pt_config.cpu.family = metadata.cpu_family;
        pt_config.cpu.model = metadata.cpu_model;
        pt_config.cpu.stepping = metadata.cpu_stepping;
        pt_config.cpu.vendor =
            pt_config.cpu.family != 0 ? CPU_VENDOR_INTEL : CPU_VENDOR_UNKNOWN;
        sb_config.time_shift = metadata.time_shift;
        sb_config.time_mult = metadata.time_mult;
        sb_config.time_zero = metadata.time_zero;

        return true;
    }
};

/**
 * pt2ir_t is a class that can convert PT raw trace to DynamoRIO's IR.
 */
class pt2ir_t {
public:
    pt2ir_t();
    ~pt2ir_t();

    /**
     * Initialize the PT instruction decoder and the sideband session.
     * @param pt2ir_config The configuration of PT raw trace.
     * @param verbosity  The verbosity level for notifications. If set to 0, only error
     * logs are printed. If set to 1, all logs are printed. Default value is 0.
     * @return true if the instance is successfully initialized.
     */
    bool
    init(IN pt2ir_config_t &pt2ir_config, IN int verbosity = 0);

    /**
     * The convert function performs two processes: (1) decode the PT raw trace into
     * libipt's IR format pt_insn; (2) convert pt_insn into the DynamoRIO's IR format
     * instr_t and append it to ilist inside the drir object.
     * @param pt_data The PT raw trace.
     * @param pt_data_size The size of PT raw trace.
     * @param drir The drir object.
     * @return pt2ir_convert_status_t. If the conversion is successful, the function
     * returns #PT2IR_CONV_SUCCESS. Otherwise, the function returns the corresponding
     * error code.
     */
    pt2ir_convert_status_t
    convert(IN const uint8_t *pt_data, IN size_t pt_data_size, INOUT drir_t &drir);

private:
    /* Diagnose converting errors and output diagnostic results.
     * It will used to generate the error message during the decoding process.
     */
    void
    dx_decoding_error(IN int errcode, IN const char *errtype, IN uint64_t ip);

    /* It indicate if the instance of pt2ir_t has been initialized, signifying the
     * readiness of the conversion process from PT data to DR's IR.
     */
    bool pt2ir_initialized_;

    /* Buffer for caching the PT raw trace. */
    std::unique_ptr<uint8_t[]> pt_raw_buffer_;
    size_t pt_raw_buffer_size_;

    /* The libipt instruction decoder. */
    struct pt_insn_decoder *pt_instr_decoder_;

    /* The libipt sideband image section cache. */
    struct pt_image_section_cache *pt_sb_iscache_;

    /* The libipt sideband session. */
    struct pt_sb_session *pt_sb_session_;

    /* The size of the PT data within the raw buffer. */
    uint64_t pt_raw_buffer_data_size_;

    /* The shared image section cache.
     * The pt2ir_t instance is designed to work with a single thread, while the image is
     * shared among all threads. And the libipt library incorporates pthread mutex to wrap
     * all its data race operations, ensuring thread safety.
     */
    static pt_iscache_autoclean_t share_iscache_;

    /* Integer value representing the verbosity level for notifications. */
    int verbosity_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _PT2IR_H_ */
