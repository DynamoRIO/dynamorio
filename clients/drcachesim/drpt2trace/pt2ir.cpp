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

pt2ir_t::pt2ir_t()
    : pt_raw_buffer_(std::unique_ptr<unsigned char>(nullptr))
    , pt_raw_buffer_size_(0)
    , pt_instr_decoder_(nullptr)
    , pt_iscache_(nullptr)
    , pt_sb_session_(nullptr)
{
}

pt2ir_t::~pt2ir_t()
{
    if (pt_sb_session_ != nullptr)
        pt_sb_free(pt_sb_session_);
    if (pt_iscache_ != nullptr)
        pt_iscache_free(pt_iscache_);
    if (pt_instr_decoder_ != nullptr)
        pt_insn_free_decoder(pt_instr_decoder_);
}

bool
pt2ir_t::init(IN pt2ir_config_t &pt2ir_config)
{
    pt_iscache_ = pt_iscache_alloc(nullptr);
    if (pt_iscache_ == nullptr) {
        ERRMSG("Failed to allocate iscache.\n");
        return false;
    }
    pt_sb_session_ = pt_sb_alloc(pt_iscache_);
    if (pt_sb_session_ == nullptr) {
        ERRMSG("Failed to allocate sb session.\n");
        return false;
    }

    struct pt_config pt_config;
    pt_config_init(&pt_config);

    struct pt_sb_pevent_config sb_pevent_config;
    memset(&sb_pevent_config, 0, sizeof(sb_pevent_config));
    sb_pevent_config.size = sizeof(sb_pevent_config);

    /* Set configurations for libipt instruction decoder. */
    pt_config.cpu.vendor =
        pt2ir_config.pt_config.cpu.vendor == CPU_VENDOR_INTEL ? pcv_intel : pcv_unknown;
    pt_config.cpu.family = pt2ir_config.pt_config.cpu.family;
    pt_config.cpu.model = pt2ir_config.pt_config.cpu.model;
    pt_config.cpu.stepping = pt2ir_config.pt_config.cpu.stepping;
    pt_config.cpuid_0x15_eax = pt2ir_config.pt_config.cpuid_0x15_eax;
    pt_config.cpuid_0x15_ebx = pt2ir_config.pt_config.cpuid_0x15_ebx;
    pt_config.nom_freq = pt2ir_config.pt_config.nom_freq;
    pt_config.mtc_freq = pt2ir_config.pt_config.mtc_freq;

    /* Parse the sideband perf event sample type. */
    sb_pevent_config.sample_type = pt2ir_config.sb_config.sample_type;

    /* Parse the sideband images sysroot. */
    if (!pt2ir_config.sb_config.sysroot.empty()) {
        sb_pevent_config.sysroot = pt2ir_config.sb_config.sysroot.c_str();
    } else {
        sb_pevent_config.sysroot = "";
    }

    /* Parse time synchronization related configuration. */
    sb_pevent_config.tsc_offset = pt2ir_config.sb_config.tsc_offset;
    sb_pevent_config.time_shift = pt2ir_config.sb_config.time_shift;
    sb_pevent_config.time_mult = pt2ir_config.sb_config.time_mult;
    sb_pevent_config.time_zero = pt2ir_config.sb_config.time_zero;

    /* Parse the start address of the kernel image. */
    sb_pevent_config.kernel_start = pt2ir_config.sb_config.kernel_start;

    /* Allocate the primary sideband decoder. */
    if (!pt2ir_config.sb_primary_file_path.empty()) {
        struct pt_sb_pevent_config sb_primary_config = sb_pevent_config;
        sb_primary_config.filename = pt2ir_config.sb_primary_file_path.c_str();
        sb_primary_config.primary = 1;
        sb_primary_config.begin = (size_t)0;
        sb_primary_config.end = (size_t)0;
        if (!alloc_sb_pevent_decoder(&sb_primary_config)) {
            ERRMSG("Failed to allocate primary sideband perf event decoder.\n");
            return false;
        }
    }

    /* Allocate the secondary sideband decoders. */
    for (auto sb_secondary_file : pt2ir_config.sb_secondary_file_path_list) {
        if (!sb_secondary_file.empty()) {
            struct pt_sb_pevent_config sb_secondary_config = sb_pevent_config;
            sb_secondary_config.filename = sb_secondary_file.c_str();
            sb_secondary_config.primary = 0;
            sb_secondary_config.begin = (size_t)0;
            sb_secondary_config.end = (size_t)0;
            if (!alloc_sb_pevent_decoder(&sb_secondary_config)) {
                ERRMSG("Failed to allocate secondary sideband perf event decoder.\n");
                return false;
            }
        }
    }

    /* Load kcore to sideband kernel image cache. */
    if (!pt2ir_config.kcore_path.empty()) {
        if (!load_kernel_image(pt2ir_config.kcore_path)) {
            ERRMSG("Failed to load kernel image: %s\n", pt2ir_config.kcore_path.c_str());
            return false;
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

    /* Load the PT raw trace file and allocate the instruction decoder. */
    if (pt2ir_config.raw_file_path.empty()) {
        ERRMSG("No PT raw trace file specified.\n");
        return false;
    }
    if (pt_config.cpu.vendor == pcv_intel) {
        int errcode = pt_cpu_errata(&pt_config.errata, &pt_config.cpu);
        if (errcode < 0) {
            ERRMSG("Failed to get cpu errata: %s.\n", pt_errstr(pt_errcode(errcode)));
            return false;
        }
    }
    if (!load_pt_raw_file(pt2ir_config.raw_file_path)) {
        ERRMSG("Failed to load trace file: %s.\n", pt2ir_config.raw_file_path.c_str());
        return false;
    }
    pt_config.begin = static_cast<uint8_t *>(pt_raw_buffer_.get());
    pt_config.end = static_cast<uint8_t *>(pt_raw_buffer_.get()) + pt_raw_buffer_size_;
    pt_instr_decoder_ = pt_insn_alloc_decoder(&pt_config);
    if (pt_instr_decoder_ == nullptr) {
        ERRMSG("Failed to create libipt instruction decoder.\n");
        return false;
    }

    return true;
}

pt2ir_convert_status_t
pt2ir_t::convert(OUT instrlist_t **ilist)
{
    /* Initializes an empty instruction list to store all DynamoRIO's IR list converted
     * from PT IR.
     */
    *ilist = instrlist_create(GLOBAL_DCONTEXT);

    /* PT raw data consists of many packets. And PT trace data is surrounded by Packet
     * Stream Boundary. So, in the outermost loop, this function first finds the PSB. Then
     * it decodes the trace data.
     */
    for (;;) {
        struct pt_insn insn;
        memset(&insn, 0, sizeof(insn));
        int status = 0;

        /* Sync decoder to the first Packet Stream Boundary (PSB) packet, and then decode
         * instructions. If there is no PSB packet, the decoder will be synced to the end
         * of the trace. If an error occurs, we will call dx_decoding_error to print the
         * error information. What are PSB packets? Below answer is quoted from Intel 64
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
            instrlist_clear_and_destroy(GLOBAL_DCONTEXT, *ilist);
            return PT2IR_CONV_ERROR_SYNC_PACKET;
        }

        /* Decode the raw trace data surround by PSB. */
        for (;;) {
            int nextstatus = status;
            int errcode = 0;
            struct pt_image *image;

            /* Before starting to decode instructions, we need to handle the event before
             * the instruction trace. For example, if a mmap2 event happens, we need to
             * switch the cached image.
             */
            while ((nextstatus & pts_event_pending) != 0) {
                struct pt_event event;

                nextstatus = pt_insn_event(pt_instr_decoder_, &event, sizeof(event));
                if (nextstatus < 0) {
                    errcode = nextstatus;
                    dx_decoding_error(errcode, "get pending event error", insn.ip);
                    instrlist_clear_and_destroy(GLOBAL_DCONTEXT, *ilist);
                    return PT2IR_CONV_ERROR_GET_PENDING_EVENT;
                }

                /* Use a sideband session to check if pt_event is an image switch event.
                 * If so, change the image in 'pt_instr_decoder_' to the target image.
                 */
                image = nullptr;
                errcode =
                    pt_sb_event(pt_sb_session_, &image, &event, sizeof(event), stdout, 0);
                if (errcode < 0) {
                    dx_decoding_error(errcode, "handle sideband event error", insn.ip);
                    instrlist_clear_and_destroy(GLOBAL_DCONTEXT, *ilist);
                    return PT2IR_CONV_ERROR_HANDLE_SIDEBAND_EVENT;
                }

                /* If it is not an image switch event, the PT instruction decoder
                 * will not switch their cached image.
                 */
                if (image == nullptr)
                    continue;

                errcode = pt_insn_set_image(pt_instr_decoder_, image);
                if (errcode < 0) {
                    dx_decoding_error(errcode, "set image error", insn.ip);
                    instrlist_clear_and_destroy(GLOBAL_DCONTEXT, *ilist);
                    return PT2IR_CONV_ERROR_SET_IMAGE;
                }
            }
            if ((nextstatus & pts_eos) != 0)
                break;

            /* Decode PT raw trace to pt_insn. */
            status = pt_insn_next(pt_instr_decoder_, &insn, sizeof(insn));
            if (status < 0) {
                dx_decoding_error(status, "get next instruction error", insn.ip);
                instrlist_clear_and_destroy(GLOBAL_DCONTEXT, *ilist);
                return PT2IR_CONV_ERROR_DECODE_NEXT_INSTR;
            }

            /* Use drdecode to decode insn(pt_insn) to instr_t. */
            instr_t *instr = instr_create(GLOBAL_DCONTEXT);
            instr_init(GLOBAL_DCONTEXT, instr);
            decode(GLOBAL_DCONTEXT, insn.raw, instr);
            instr_set_translation(instr, (app_pc)insn.ip);
            instr_allocate_raw_bits(GLOBAL_DCONTEXT, instr, insn.size);
            if (!instr_valid(instr)) {
                ERRMSG("Failed to convert the libipt's IR to Dynamorio's IR.\n");
                instrlist_clear_and_destroy(GLOBAL_DCONTEXT, *ilist);
                return PT2IR_CONV_ERROR_DR_IR_CONVERT;
            }
            instrlist_append(*ilist, instr);
        }
    }
    return PT2IR_CONV_SUCCESS;
}

bool
pt2ir_t::load_pt_raw_file(IN std::string &path)
{
    /* Under C++11, there is no good solution to get the file size after using ifstream to
     * open a file. Because we will not change the PT raw trace file during converting, we
     * don't need to think about write-after-read. We get the file size from file stat
     * first and then use ifstream to open and read the PT raw trace file.
     * XXX: We may need to update Dynamorio to support C++17+. Then we can implement the
     * following logic without opening the file twice.
     */
    errno = 0;
    struct stat fstat;
    if (stat(path.c_str(), &fstat) == -1) {
        ERRMSG("Failed to get file size of trace file: %s: %d.\n", path.c_str(), errno);
        return false;
    }
    pt_raw_buffer_size_ = static_cast<size_t>(fstat.st_size);

    pt_raw_buffer_ = std::unique_ptr<unsigned char>(
        reinterpret_cast<unsigned char *>(new char[pt_raw_buffer_size_]));
    std::ifstream f(path, std::ios::binary | std::ios::in);
    if (!f.is_open()) {
        ERRMSG("Failed to open trace file: %s.\n", path.c_str());
        return false;
    }
    f.read(reinterpret_cast<char *>(pt_raw_buffer_.get()), pt_raw_buffer_size_);
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
pt2ir_t::alloc_sb_pevent_decoder(IN struct pt_sb_pevent_config *config)
{
    int errcode = pt_sb_alloc_pevent_decoder(pt_sb_session_, config);
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
     * pos. The 'pt_insn_get_offset' function is mainly used to report errors.
     */
    err = pt_insn_get_offset(pt_instr_decoder_, &pos);
    if (err < 0) {
        ERRMSG("Could not determine offset: %s\n", pt_errstr(pt_errcode(err)));
        ERRMSG("[?, %" PRIx64 "] %s: %s\n", ip, errtype, pt_errstr(pt_errcode(errcode)));
    } else {
        ERRMSG("[%" PRIx64 ", IP:%" PRIx64 "] %s: %s\n", pos, ip, errtype,
               pt_errstr(pt_errcode(errcode)));
    }
}
