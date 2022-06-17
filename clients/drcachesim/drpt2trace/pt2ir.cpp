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

#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <linux/limits.h>
#include <fstream>

#include "load_elf.h"
#include "pt2ir.h"

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

pt2ir_t::pt2ir_t(IN const pt2ir_config_t &config, IN unsigned int verbosity)
    : config_(config)
    , pt_raw_buffer_(nullptr)
    , pt_raw_buffer_size_(0)
    , pt_instr_decoder_(nullptr)
    , pt_iscache_(nullptr)
    , pt_sb_session_(nullptr)
{
    pt_iscache_ = pt_iscache_alloc(NULL);
    if (pt_iscache_ == NULL) {
        ERRMSG("Failed to allocate iscache\n");
        exit(1);
    }
    pt_sb_session_ = pt_sb_alloc(pt_iscache_);
    if (pt_sb_session_ == NULL) {
        ERRMSG("Failed to allocate sb session\n");
        pt_iscache_free(pt_iscache_);
        exit(1);
    }
    if (parse_config()) {
        ERRMSG("Failed to parse config\n");
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
pt2ir_t::process_decode()
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

        /* Decode instructions. */
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

            /* Decode pt data to pt_insn. */
            status = pt_insn_next(pt_instr_decoder_, &insn, sizeof(insn));
            if (status < 0) {
                dx_decoding_error(status, "get next instruction error", insn.ip);
                exit(1);
            }

            /* TODO i#5505: Use drdecode to decode insn(pt_insn) to inst_t */

        }
    }
}

bool
pt2ir_t::parse_config()
{
    struct pt_config pt_config;
    pt_config_init(&pt_config);
    struct pt_sb_pevent_config sb_pevent_config;
    memset(&sb_pevent_config, 0, sizeof(sb_pevent_config));

    /* Parse the cpu type. */
    if (config_.cpu == "none") {
        pt_config.cpu = { 0, 0, 0, 0 };
    } else {
        pt_config.cpu.vendor = pcv_intel;
        int num = sscanf(config_.cpu.c_str(), "%d/%d/%d", &pt_config.cpu.family,
                         &pt_config.cpu.model, &pt_config.cpu.stepping);
        if (num < 2) {
            ERRMSG("Invalid cpu type: %s.\n", config_.cpu.c_str());
            return false;
        } else if (num == 2) {
            pt_config.cpu.stepping = 0;
        } else {
            /* Nothing */
        }
    }

    /* Load kcore to sideband kernel image cache. */
    if (config_.kcore_path.size() > 0) {
        if (load_kernel_image(config_.kcore_path)) {
            ERRMSG("Failed to load kernel image: %s\n", config_.kcore_path.c_str());
            return false;
        }
    }

    /* Parse the sideband perf event sample type*/
    uint64_t sample_type = 0;
    if (config_.sample_type.size() > 0) {
        if (sscanf(config_.sample_type, "%lx", &sample_type) != 1) {
            ERRMSG("Failed to parse sideband perf event config.\n");
            return false;
        }
    }
    sb_pevent_config.sample_type = sample_type;

    /* Parse the sideband images sysroot.*/
    char *sysroot = NULL;
    if (config_.sysroot.size() > 0) {
        sysroot = config_.sysroot.c_str();
    }
    sb_pevent_config.sysroot = sysroot;

    /* Parse the start address of the kernel image  */
    uint64_t kernel_start = 0;
    if (config_.kernel_start.size() > 0) {
        if (sscanf(config_.kernel_start.c_str(), "%lx", &kernel_start) != 1) {
            ERRMSG("Failed to parse kernel start address.\n");
            return false;
        }
    }
    sb_pevent_config.kernel_start = kernel_start;

    /* Allocate the primary sideband decoder */
    if (config_.sb_primary_file.size() > 0) {
        struct pt_sb_pevent_config sb_primary_config = sb_pevent_config;
        sb_primary_config.filename = config_.sb_primary_file.c_str();
        sb_primary_config.primary = 1;
        sb_primary_config.begin = (size_t)0;
        sb_primary_config.end = (size_t)0;
        if (alloc_sb_pevent_decoder(sb_primary_config)) {
            ERRMSG("Failed to allocate primary sideband perf event decoder.\n");
            return false;
        }
    }

    /* Allocate the secondary sideband decoders */
    for (auto sb_secondary_file : config_.sb_secondary_files) {
        if (sb_secondary_file.size() > 0) {
            struct pt_sb_pevent_config sb_primary_config = sb_pevent_config;
            sb_primary_config.filename = sb_secondary_file.c_str();
            sb_primary_config.primary = 0;
            sb_primary_config.begin = (size_t)0;
            sb_primary_config.end = (size_t)0;
            if (alloc_sb_pevent_decoder(sb_secondary_file)) {
                ERRMSG("Failed to allocate secondary sideband perf event decoder.\n");
                return false;
            }
        }
    }

    /* Load the pt trace file and allocate the instruction decoder.*/
    if (load_pt_raw_file(config_.raw_file_path)) {
        ERRMSG("Failed to load trace file: %s.\n", config_.raw_file_path.c_str());
        return false;
    }
    pt_config.begin = pt_raw_buffer_;
    pt_config.end = pt_raw_buffer_ + pt_raw_buf_size_;
    pt_instr_decoder_ = pt_insn_alloc_decoder(&pt_config);
    if (pt_instr_decoder_ == NULL) {
        ERRMSG("Failed to create libipt instruction "
               "decoder.\n", );
        return false;
    }

    return true;
}

bool
pt2ir_t::load_pt_raw_file(IN std::string &raw_file_path)
{
    std::ifstream raw_file;
    try {
        raw_file.open(raw_file_path, std::ios::binary);
        pt_raw_buffer_size_ = raw_file.tellg();

        raw_file.seekg(0, std::ios::beg);
        pt_raw_buffer_ = new uint8_t[pt_raw_buffer_size_];
        raw_file.read(pt_raw_buffer_, pt_raw_buffer_size_);

        raw_file.close();
    } catch (std::exception &e) {
        ERRMSG("Failed to load pt raw file %s: cache an exception [%s].\n", raw_file_path.c_str(), e.what());
        return false;
    }
    return true;
}

bool
pt2ir_t::load_kernel_image(IN const char* kcore_path)
{
    struct pt_image *kimage = pt_sb_kernel_image(pt_sb_session_);
    int errcode = load_elf(pt_iscache_, kimage, kcore_path, 0, LIBNAME, 0);
    if (errcode < 0) {
        ERRMSG("Failed to load kernel image %s: %s.\n", kcore_path,
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

    /* Get the current pt inst decoder's position. It will fill pt_instr_decoder_'s
     * position into pos. pt_insn_get_offset mainly used to report error. */
    err = pt_insn_get_offset(pt_instr_decoder_, &pos);
    if (err < 0) {
        ERRMSG("Could not determine offset: %s\n", pt_errstr(pt_errcode(err)));
        ERRMSG("[?, %" PRIx64 ": %s: %s]\n", ip, errtype, pt_errstr(pt_errcode(errcode)));
    } else
        ERRMSG("[%" PRIx64 ", %" PRIx64 ": %s: %s]\n", pos, ip, errtype,
               pt_errstr(pt_errcode(errcode)));
}


bool
pt2ir_t::load_kernel_image(IN std::string kcore_path)
{
    struct pt_image *kimage = pt_sb_kernel_image(pt_sb_session_);
    std::ifstream kcore;
    try
    {
        kcore.open(kcore_path, std::ios::binary | std::ios::in);
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }

    std::ifstream kcore(kcore_path, std::ios::binary | std::ios::in);
    if (errno != 0) {
        ERRMSG("Failed to open %s: %s.\n", kcore_path.c_str(), strerror(errno));
        return false;
    }
    uint8_t e_ident[EI_NIDENT];
    coukcore.read(reinterpret_cast<char*>(e_ident), EI_NIDENT);


    return true;
}
