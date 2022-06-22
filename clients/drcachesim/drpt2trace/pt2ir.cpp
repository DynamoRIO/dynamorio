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

#include <sys/stat.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <fstream>

#include "intel-pt.h"
#include "libipt-sb.h"
#include "pt2ir.h"
extern "C" {
#include "load_elf.h"
}
#define DR_FAST_IR 1
#include "dr_api.h"

#define ERRMSG_HEADER()                       \
    do {                                      \
        fprintf(stderr, "[drpt2ir] ERROR: "); \
    } while (0)

#define ERRMSG(...)                   \
    do {                              \
        ERRMSG_HEADER();              \
        fprintf(stderr, __VA_ARGS__); \
        fflush(stderr);               \
    } while (0)

pt2ir_t::pt2ir_t(IN const pt2ir_config_t config)
    : config_(config)
    , pt_raw_buffer_(nullptr)
    , pt_raw_buffer_size_(0)
    , pt_instr_decoder_(nullptr)
    , pt_iscache_(nullptr)
    , pt_sb_session_(nullptr)
{
    pt_iscache_ = pt_iscache_alloc(NULL);
    if (pt_iscache_ == NULL) {
        ERRMSG("Failed to allocate iscache.\n");
        exit(1);
    }
    pt_sb_session_ = pt_sb_alloc(pt_iscache_);
    if (pt_sb_session_ == NULL) {
        ERRMSG("Failed to allocate sb session.\n");
        pt_iscache_free(pt_iscache_);
        exit(1);
    }
    if (!init()) {
        ERRMSG("Failed to initialize.\n");
        pt_iscache_free(pt_iscache_);
        pt_sb_free(pt_sb_session_);
        exit(1);
    }
}

pt2ir_t::~pt2ir_t()
{
    pt_sb_free(pt_sb_session_);
    pt_iscache_free(pt_iscache_);
    pt_insn_free_decoder(pt_instr_decoder_);
    if (pt_raw_buffer_ != NULL) {
        free(pt_raw_buffer_);
    }
}

void
pt2ir_t::convert()
{
    for (;;) {
        struct pt_insn insn;
        memset(&insn, 0, sizeof(insn));
        int status = 0;

        /* Sync decoder to first Packet Stream Boundary (PSB) packet, and then decode
         * instructions. If there is no PSB packet, the decoder will be synced to the end
         * of the trace. If error occurs, we will call dx_decoding_error to print the
         * error information. What is PSB packets? Below answer is quoted from Intel® 64
         * and IA-32 Architectures Software Developer’s Manual 32.1.1.1 Packet Summary
         * “Packet Stream Boundary (PSB) packets: PSB packets act as ‘heartbeats’ that are
         * generated at regular intervals (e.g., every 4K trace packet bytes). These
         * packets allow the packet decoder to find the packet boundaries within the
         * output data stream; a PSB packet should be the first packet that a decoder
         * looks for when beginning to decode a trace.”
         */
        status = pt_insn_sync_forward(pt_instr_decoder_);
        if (status < 0) {
            if (status == -pte_eos)
                break;
            dx_decoding_error(status, "sync error", insn.ip);
            exit(1);
        }

        for (;;) {
            int nextstatus = status;
            int errcode = 0;
            struct pt_image *image;

            /* Handle next status and all pending perf events. */
            while (nextstatus & pts_event_pending) {
                struct pt_event event;

                nextstatus = pt_insn_event(pt_instr_decoder_, &event, sizeof(event));
                if (nextstatus < 0) {
                    errcode = nextstatus;
                    break;
                }

                /* Use a sideband session to check if pt_event is an image switch event.
                 * If so, change the image in 'pt_instr_decoder_' to the target image. */
                image = NULL;
                errcode =
                    pt_sb_event(pt_sb_session_, &image, &event, sizeof(event), stdout, 0);
                if (errcode < 0)
                    break;
                if (image == NULL)
                    continue;

                errcode = pt_insn_set_image(pt_instr_decoder_, image);
                if (errcode < 0) {
                    break;
                }
            }
            if (errcode < 0) {
                dx_decoding_error(errcode, "handle sideband event error", insn.ip);
                exit(1);
            }
            if ((nextstatus & pts_eos) != 0) {
                break;
            }

            /* Decode PT raw data to pt_insn. */
            status = pt_insn_next(pt_instr_decoder_, &insn, sizeof(insn));
            if (status < 0) {
                dx_decoding_error(status, "get next instruction error", insn.ip);
                exit(1);
            }

            pt_instr_list_.push_back(insn);

            /* TODO i#5505: Use drdecode to decode insn(pt_insn) to inst_t. */
        }
    }
}

uint64_t
pt2ir_t::get_instr_count()
{
    return pt_instr_list_.size();
}

void
pt2ir_t::print_instrs_to_stdout()
{
    void *dcontext = GLOBAL_DCONTEXT;
    for (auto it = pt_instr_list_.begin(); it != pt_instr_list_.end(); it++) {
        disassemble_with_info(dcontext, it->raw, STDOUT, false, false);
    }
}

bool
pt2ir_t::init()
{
    struct pt_config pt_config;
    pt_config_init(&pt_config);

    struct pt_sb_pevent_config sb_pevent_config;
    memset(&sb_pevent_config, 0, sizeof(sb_pevent_config));
    sb_pevent_config.size = sizeof(sb_pevent_config);

    /* Set configurations for libipt instruction decoder. */
    pt_config.cpu.vendor = config_.pt_config.cpu.vendor == 1? pcv_intel : pcv_unknown;
    pt_config.cpu.family = config_.pt_config.cpu.family;
    pt_config.cpu.model = config_.pt_config.cpu.model;
    pt_config.cpu.stepping = config_.pt_config.cpu.stepping;
    pt_config.cpuid_0x15_eax = config_.pt_config.cpuid_0x15_eax;
    pt_config.cpuid_0x15_ebx = config_.pt_config.cpuid_0x15_ebx;
    pt_config.nom_freq = config_.pt_config.nom_freq;
    pt_config.mtc_freq = config_.pt_config.mtc_freq;

    /* Parse the sideband perf event sample type*/
    sb_pevent_config.sample_type = config_.sb_config.sample_type;

    /* Parse the sideband images sysroot.*/
    if (config_.sb_config.sysroot.size() > 0) {
        sb_pevent_config.sysroot = config_.sb_config.sysroot.c_str();
    } else {
        sb_pevent_config.sysroot = "";
    }

    /* Load kcore to sideband kernel image cache. */
    if (config_.sb_config.kcore_path.size() > 0) {
        if (!load_kernel_image(config_.sb_config.kcore_path)) {
            ERRMSG("Failed to load kernel image: %s\n",
                   config_.sb_config.kcore_path.c_str());
            return false;
        }
    }

    /* Parse time synchronization related configuration. */
    sb_pevent_config.tsc_offset = config_.sb_config.tsc_offset;
    sb_pevent_config.time_shift = config_.sb_config.time_shift;
    sb_pevent_config.time_mult =
        config_.sb_config.time_mult > 0 ? config_.sb_config.time_mult : 1;
    sb_pevent_config.time_zero = config_.sb_config.time_zero;

    /* Parse the start address of the kernel image  */
    sb_pevent_config.kernel_start = config_.sb_config.kernel_start;

    /* Allocate the primary sideband decoder */
    if (config_.sb_primary_file_path.size() > 0) {
        struct pt_sb_pevent_config sb_primary_config = sb_pevent_config;
        sb_primary_config.filename = config_.sb_primary_file_path.c_str();
        sb_primary_config.primary = 1;
        sb_primary_config.begin = (size_t)0;
        sb_primary_config.end = (size_t)0;
        if (!alloc_sb_pevent_decoder(sb_primary_config)) {
            ERRMSG("Failed to allocate primary sideband perf event decoder.\n");
            return false;
        }
    }

    /* Allocate the secondary sideband decoders */
    for (auto sb_secondary_file : config_.sb_secondary_file_path_list) {
        if (sb_secondary_file.size() > 0) {
            struct pt_sb_pevent_config sb_secondary_config = sb_pevent_config;
            sb_secondary_config.filename = sb_secondary_file.c_str();
            sb_secondary_config.primary = 0;
            sb_secondary_config.begin = (size_t)0;
            sb_secondary_config.end = (size_t)0;
            if (!alloc_sb_pevent_decoder(sb_secondary_config)) {
                ERRMSG("Failed to allocate secondary sideband perf event decoder.\n");
                return false;
            }
        }
    }

    /* Initialize all sideband decoders. It needs to be called after all sideband decoders
     * have been allocated.
     */
    int errcode = pt_sb_init_decoders(pt_sb_session_);
    if (errcode < 0) {
        ERRMSG("Failed to initialize sideband session: %s.\n",
               pt_errstr(pt_errcode(errcode)));
        return false;
    }

    /* Load the PT raw data file and allocate the instruction decoder.*/
    if (config_.raw_file_path.size() == 0) {
        ERRMSG("No PT raw data file specified.\n");
        return false;
    }
    if (pt_config.cpu.vendor) {
        int errcode = pt_cpu_errata(&pt_config.errata, &pt_config.cpu);
        if (errcode < 0) {
            ERRMSG("Failed to get cpu errata: %s.\n", pt_errstr(pt_errcode(errcode)));
            return false;
        }
    }
    if (!load_pt_raw_file(config_.raw_file_path)) {
        ERRMSG("Failed to load trace file: %s.\n", config_.raw_file_path.c_str());
        return false;
    }
    pt_config.begin = static_cast<uint8_t *>(pt_raw_buffer_);
    pt_config.end = static_cast<uint8_t *>(pt_raw_buffer_) + pt_raw_buffer_size_;
    pt_instr_decoder_ = pt_insn_alloc_decoder(&pt_config);
    if (pt_instr_decoder_ == NULL) {
        ERRMSG("Failed to create libipt instruction decoder.\n");
        return false;
    }

    return true;
}

bool
pt2ir_t::load_pt_raw_file(IN std::string &path)
{
    /* Under C++11, there is no good solution to get the file size after using ifstream to
     * open a file. Because we will not change the PT raw data file during converting, we
     * don't need to think about write-after-read. We get the file size from file stat
     * first and then use ifstream to open and read the PT raw data file.
     */
    errno = 0;
    struct stat fstat;
    if (stat(path.c_str(), &fstat) == -1) {
        ERRMSG("Failed to get file size of trace file: %s: %d.\n", path.c_str(), errno);
        return false;
    }
    pt_raw_buffer_size_ = static_cast<size_t>(fstat.st_size);

    pt_raw_buffer_ = new uint8_t[pt_raw_buffer_size_];
    std::ifstream f(path, std::ios::binary | std::ios::in);
    if (!f.is_open()) {
        ERRMSG("Failed to open trace file: %s.\n", path.c_str());
        return false;
    }
    f.read(static_cast<char *>(pt_raw_buffer_), pt_raw_buffer_size_);
    if (f.fail()) {
        ERRMSG("Failed to read trace file: %s.\n", path.c_str());
        return false;
    }

    f.close();
    return true;
}

bool
pt2ir_t::load_kernel_image(IN std::string &path)
{
    struct pt_image *kimage = pt_sb_kernel_image(pt_sb_session_);
    /* Load all ELF sections in kcore to the shared image cache.
     * XXX: load_elf() is implemented in libipt's client ptxed. Currently we directly use
     * it. We may need to implement a c++ version in our client.
     */
    int errcode =
        load_elf(pt_iscache_, pt_sb_kernel_image(pt_sb_session_), path.c_str(), 0, "", 0);
    if (errcode < 0) {
        ERRMSG("Failed to load kernel image %s: %s.\n", path.c_str(),
               pt_errstr(pt_errcode(errcode)));
        return false;
    }
    return true;
}

bool
pt2ir_t::alloc_sb_pevent_decoder(IN struct pt_sb_pevent_config &config)
{
    int errcode = pt_sb_alloc_pevent_decoder(pt_sb_session_, &config);
    if (errcode < 0) {
        ERRMSG("Failed to allocate sideband perf event decoder: %s.\n",
               pt_errstr(pt_errcode(errcode)));
        return false;
    }
    return true;
}

void
pt2ir_t::dx_decoding_error(IN int errcode, IN const char *errtype, IN uint64_t ip)
{
    int err = -pte_internal;
    uint64_t pos = 0;

    /* Get the current position of 'pt_instr_decoder_'. It will fill the position into
     * pos. The 'pt_insn_get_offset' function mainly used to report error. */
    err = pt_insn_get_offset(pt_instr_decoder_, &pos);
    if (err < 0) {
        ERRMSG("Could not determine offset: %s\n", pt_errstr(pt_errcode(err)));
        ERRMSG("[?, %" PRIx64 "] %s: %s\n", ip, errtype, pt_errstr(pt_errcode(errcode)));
    } else
        ERRMSG("[%" PRIx64 ", IP:%" PRIx64 "] %s: %s\n", pos, ip, errtype,
               pt_errstr(pt_errcode(errcode)));
}
