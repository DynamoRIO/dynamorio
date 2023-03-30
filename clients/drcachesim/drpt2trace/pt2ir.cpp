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
#include <errno.h>
#include <fstream>

#include "intel-pt.h"
#include "libipt-sb.h"

#include "../common/utils.h"
#include "elf_loader.h"
#include "pt2ir.h"

#define ERRMSG_HEADER "[drpt2ir] "

pt2ir_t::pt2ir_t()
    : pt2ir_initialized_(false)
    , pt_raw_buffer_(std::unique_ptr<uint8_t[]>(nullptr))
    , pt_raw_buffer_size_(0)
    , pt_instr_decoder_(nullptr)
    , pt_sb_iscache_(nullptr)
    , pt_sb_session_(nullptr)
    , pt_raw_buffer_data_size_(0)
    , cur_pt_data_end_offset_(0)
    , pt_decoder_status_(pts_eos)
{
}

pt2ir_t::~pt2ir_t()
{
    if (pt_sb_session_ != nullptr)
        pt_sb_free(pt_sb_session_);
    if (pt_sb_iscache_ != nullptr)
        pt_iscache_free(pt_sb_iscache_);
    if (pt_instr_decoder_ != nullptr)
        pt_insn_free_decoder(pt_instr_decoder_);
    pt2ir_initialized_ = false;
}

bool
pt2ir_t::init(IN pt2ir_config_t &pt2ir_config, IN const uint8_t *init_pt_data,
              IN size_t init_pt_data_size,
              IN struct pt_image_section_cache *shared_iscache)
{
    if (pt2ir_initialized_) {
        ERRMSG(ERRMSG_HEADER "pt2ir_t is already initialized.\n");
        return false;
    }

    /* Init the configuration for the libipt instruction decoder. */
    struct pt_config pt_config;
    pt_config_init(&pt_config);

    /* Set configurations for the libipt instruction decoder. */
    pt_config.cpu.vendor =
        pt2ir_config.pt_config.cpu.vendor == CPU_VENDOR_INTEL ? pcv_intel : pcv_unknown;
    pt_config.cpu.family = pt2ir_config.pt_config.cpu.family;
    pt_config.cpu.model = pt2ir_config.pt_config.cpu.model;
    pt_config.cpu.stepping = pt2ir_config.pt_config.cpu.stepping;
    pt_config.cpuid_0x15_eax = pt2ir_config.pt_config.cpuid_0x15_eax;
    pt_config.cpuid_0x15_ebx = pt2ir_config.pt_config.cpuid_0x15_ebx;
    pt_config.nom_freq = pt2ir_config.pt_config.nom_freq;
    pt_config.mtc_freq = pt2ir_config.pt_config.mtc_freq;
    if (pt_config.cpu.vendor == pcv_intel) {
        int errcode = pt_cpu_errata(&pt_config.errata, &pt_config.cpu);
        if (errcode < 0) {
            ERRMSG(ERRMSG_HEADER "Failed to get cpu errata: %s.\n",
                   pt_errstr(pt_errcode(errcode)));
            return false;
        }
    }
    pt_raw_buffer_size_ = pt2ir_config.pt_raw_buffer_size;
    pt_raw_buffer_ = std::unique_ptr<uint8_t[]>(new uint8_t[pt_raw_buffer_size_]);
    pt_config.begin = pt_raw_buffer_.get();
    pt_config.end = pt_raw_buffer_.get() + pt_raw_buffer_size_;
    if (init_pt_data != nullptr && init_pt_data_size > 0) {
        if (init_pt_data_size > pt_raw_buffer_size_) {
            ERRMSG(ERRMSG_HEADER
                   "Initial PT data size(%zu) is larger than the buffer size(%zu).\n",
                   init_pt_data_size, pt_raw_buffer_size_);
            return false;
        }
        memcpy(pt_raw_buffer_.get() + pt_raw_buffer_data_size_, init_pt_data,
               init_pt_data_size);
        pt_raw_buffer_data_size_ += init_pt_data_size;
        cur_pt_data_end_offset_ = 0;
    } else {
        ERRMSG(ERRMSG_HEADER "Initial PT data is not provided.\n");
        return false;
    }

    pt_instr_decoder_ = pt_insn_alloc_decoder(&pt_config);
    if (pt_instr_decoder_ == nullptr) {
        ERRMSG(ERRMSG_HEADER "Failed to create libipt instruction decoder.\n");
        return false;
    }

    /* If an ELF file is provided, load it into the image of the instruction decoder. */
    if (!pt2ir_config.elf_file_path.empty() && shared_iscache != nullptr) {
        if (!elf_loader_t::load(pt2ir_config.elf_file_path.c_str(), pt2ir_config.elf_base,
                                shared_iscache, pt_insn_get_image(pt_instr_decoder_))) {
            ERRMSG("Failed to load ELF file(%s) to.\n",
                   pt2ir_config.elf_file_path.c_str());
            return false;
        }
    }

    /* Init the sideband image section cache and session. */
    pt_sb_iscache_ = pt_iscache_alloc(nullptr);
    if (pt_sb_iscache_ == nullptr) {
        ERRMSG(ERRMSG_HEADER "Failed to allocate sideband image section cache.\n");
        return false;
    }
    pt_sb_session_ = pt_sb_alloc(pt_sb_iscache_);
    if (pt_sb_session_ == nullptr) {
        ERRMSG(ERRMSG_HEADER "Failed to allocate sb session.\n");
        return false;
    }

    /* Init the configuration for libipt-sb sideband decoders. */
    struct pt_sb_pevent_config sb_pevent_config;
    memset(&sb_pevent_config, 0, sizeof(sb_pevent_config));
    sb_pevent_config.size = sizeof(sb_pevent_config);

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

    /* Allocate sideband decoders in the sideband session. The sideband session may
     * allocate one primary decoder and many secondary decoders, which mainly work on
     * handling sideband perf records and help the PT decoder switch images.
     */
    if (!pt2ir_config.sb_primary_file_path.empty()) {
        /* Allocate the primary sideband decoder. */
        struct pt_sb_pevent_config sb_primary_config = sb_pevent_config;
        sb_primary_config.filename = pt2ir_config.sb_primary_file_path.c_str();
        sb_primary_config.primary = 1;
        sb_primary_config.begin = (size_t)0;
        sb_primary_config.end = (size_t)0;
        int errcode = pt_sb_alloc_pevent_decoder(pt_sb_session_, &sb_primary_config);
        if (errcode < 0) {
            ERRMSG(ERRMSG_HEADER
                   "Failed to allocate primary sideband perf event decoder: %s.\n",
                   pt_errstr(pt_errcode(errcode)));
            return false;
        }
    }
    for (auto sb_secondary_file : pt2ir_config.sb_secondary_file_path_list) {
        if (!sb_secondary_file.empty()) {
            /* Allocate one secondary sideband decoder. */
            struct pt_sb_pevent_config sb_secondary_config = sb_pevent_config;
            sb_secondary_config.filename = sb_secondary_file.c_str();
            sb_secondary_config.primary = 0;
            sb_secondary_config.begin = (size_t)0;
            sb_secondary_config.end = (size_t)0;
            int errcode =
                pt_sb_alloc_pevent_decoder(pt_sb_session_, &sb_secondary_config);
            if (errcode < 0) {
                ERRMSG(ERRMSG_HEADER
                       "Failed to allocate secondary sideband perf event decoder: %s.\n",
                       pt_errstr(pt_errcode(errcode)));
                return false;
            }
        }
    }

    /* Load the elf section in kcore to sideband session iscache and store the section
     * index to sideband kimage.
     */
    if (!pt2ir_config.sb_kcore_path.empty()) {
        if (!elf_loader_t::load(pt2ir_config.sb_kcore_path.c_str(), 0, pt_sb_iscache_,
                                pt_sb_kernel_image(pt_sb_session_))) {
            ERRMSG(ERRMSG_HEADER
                   "Failed to load kcore(%s) to sideband image sections cache.\n",
                   pt2ir_config.sb_kcore_path.c_str());
            return false;
        }
    }

    /* Initialize all sideband decoders. It needs to be called after all sideband decoders
     * have been allocated.
     */
    int errcode = pt_sb_init_decoders(pt_sb_session_);
    if (errcode < 0) {
        ERRMSG(ERRMSG_HEADER "Failed to initialize sideband session: %s.\n",
               pt_errstr(pt_errcode(errcode)));
        return false;
    }

    pt2ir_initialized_ = true;
    return true;
}

pt2ir_convert_status_t
pt2ir_t::convert(IN const uint8_t *next_pt_data, IN size_t next_pt_data_size,
                 INOUT drir_t *drir)
{
    if (!pt2ir_initialized_) {
        return PT2IR_CONV_ERROR_NOT_INITIALIZED;
    }

    if (drir == nullptr) {
        return PT2IR_CONV_ERROR_INVALID_INPUT;
    }

    if (pt_raw_buffer_data_size_ + next_pt_data_size > pt_raw_buffer_size_) {
        return PT2IR_CONV_ERROR_RAW_TRACE_TOO_LARGE;
    }

    /* Append the PT data to the raw buffer. */
    cur_pt_data_end_offset_ = pt_raw_buffer_data_size_;
    if (next_pt_data != nullptr && next_pt_data_size > 0) {
        memcpy(pt_raw_buffer_.get() + pt_raw_buffer_data_size_, next_pt_data,
               next_pt_data_size);
        pt_raw_buffer_data_size_ += next_pt_data_size;
    }

    /* Verify whether it is necessary to synchronize with the current PT data. */
    bool need_sync = true;
    int status = 0;
    if ((pt_decoder_status_ & pts_eos) == 0) {
        status = pt_decoder_status_;
        need_sync = false;
    }

    /* PT raw data consists of many packets. And PT trace data is surrounded by Packet
     * Stream Boundary. So, in the outermost loop, this function first finds the PSB. Then
     * it decodes the trace data.
     */
    for (;;) {
        struct pt_insn insn;
        memset(&insn, 0, sizeof(insn));

        /* Sync decoder to the first Packet Stream Boundary (PSB) packet, and then decode
         * instructions. If there is no PSB packet, the decoder will be synced to the end
         * of the trace. If an error occurs, we will call dx_decoding_error to print the
         * error information.
         * What are PSB packets? Below answer is quoted from Intel 64 and IA-32
         * Architectures Software Developer’s Manual 32.1.1.1 Packet Summary
         * "Packet Stream Boundary (PSB) packets: PSB packets act as 'heartbeats' that are
         * generated at regular intervals (e.g., every 4K trace packet bytes). These
         * packets allow the packet decoder to find the packet boundaries within the
         * output data stream; a PSB packet should be the first packet that a decoder
         * looks for when beginning to decode a trace."
         */
        if (need_sync) {
            status = pt_insn_sync_forward(pt_instr_decoder_);
            if (status < 0) {
                if (status == -pte_eos) {
                    pt_decoder_status_ = status;
                    break;
                }
                dx_decoding_error(status, "sync error", insn.ip);
                return PT2IR_CONV_ERROR_SYNC_PACKET;
            }

            /* After decoding the previous PSB-wrapped packet, all data following the next
             * PSB will be moved to the top of the buffer.
             */
            uint64_t pos_offset = 0;
            pt_insn_get_sync_offset(pt_instr_decoder_, &pos_offset);
            if (pos_offset > 0) {
                memcpy(pt_raw_buffer_.get(), pt_raw_buffer_.get() + pos_offset,
                       pt_raw_buffer_data_size_ - pos_offset);
                pt_raw_buffer_data_size_ -= pos_offset;
                cur_pt_data_end_offset_ -= pos_offset;
                status = pt_insn_sync_set(pt_instr_decoder_, 0);
                ERRMSG("pt_insn_sync_forward status: %d %d\n", status, -pte_eos);
            }
            if (status < 0) {
                if (status == -pte_eos) {
                    pt_decoder_status_ = status;
                    break;
                }
                dx_decoding_error(status, "sync error", insn.ip);
                return PT2IR_CONV_ERROR_SYNC_PACKET;
            }
            pt_decoder_status_ = status;

            /* Verify if the current position belongs to the current PT data; if not,
             * terminate the conversion process.
             */
            uint64_t cur_pos = 0;
            pt_insn_get_offset(pt_instr_decoder_, &cur_pos);
            if (cur_pos >= cur_pt_data_end_offset_) {
                return PT2IR_CONV_SUCCESS;
            }
        }
        need_sync = true;

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
                    return PT2IR_CONV_ERROR_SET_IMAGE;
                }
            }
            if ((nextstatus & pts_eos) != 0)
                break;

            /* Decode PT raw trace to pt_insn. */
            status = pt_insn_next(pt_instr_decoder_, &insn, sizeof(insn));
            if (status < 0) {
                dx_decoding_error(status, "get next instruction error", insn.ip);
                return PT2IR_CONV_ERROR_DECODE_NEXT_INSTR;
            }

            /* Use drdecode to decode insn(pt_insn) to instr_t. */
            void *drcontext = drir->get_drcontext();
            instr_t *instr = instr_create(drcontext);
            instr_init(drcontext, instr);
            instr_set_isa_mode(instr,
                               insn.mode == ptem_32bit ? DR_ISA_IA32 : DR_ISA_AMD64);
            decode(drcontext, insn.raw, instr);
            instr_set_translation(instr, (app_pc)insn.ip);
            instr_allocate_raw_bits(drcontext, instr, insn.size);
            /* TODO i#2103: Currently, the PT raw data may contain 'STAC' and 'CLAC'
             * instructions that are not supported by Dynamorio.
             */
            if (!instr_valid(instr)) {
                /* The decode() function will not correctly identify the raw bits for
                 * invalid instruction. So we need to set the raw bits of instr manually.
                 */
                instr_free_raw_bits(drcontext, instr);
                instr_set_raw_bits(instr, insn.raw, insn.size);
                instr_allocate_raw_bits(drcontext, instr, insn.size);
#ifdef DEBUG
                /* Print the invalid instruction‘s PC and raw bytes in DEBUG mode. */
                dr_fprintf(STDOUT, "<INVALID> <raw " PFX "-" PFX " ==", (app_pc)insn.ip,
                           (app_pc)insn.ip + insn.size);
                for (int i = 0; i < insn.size; i++) {
                    dr_fprintf(STDOUT, " %02x", insn.raw[i]);
                }
                dr_fprintf(STDOUT, ">\n");
#endif
            }
            drir->append(instr);
            pt_decoder_status_ = status;
            uint64_t pos = 0;
            pt_insn_get_offset(pt_instr_decoder_, &pos);
            if (pos >= cur_pt_data_end_offset_) {
                return PT2IR_CONV_SUCCESS;
            }
        }
    }
    return PT2IR_CONV_SUCCESS;
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
        ERRMSG(ERRMSG_HEADER "Could not determine offset: %s\n",
               pt_errstr(pt_errcode(err)));
        ERRMSG(ERRMSG_HEADER "[?, " HEX64_FORMAT_STRING "] %s: %s\n", ip, errtype,
               pt_errstr(pt_errcode(errcode)));
    } else {
        ERRMSG(ERRMSG_HEADER "[" HEX64_FORMAT_STRING ", IP:" HEX64_FORMAT_STRING
                             "] %s: %s\n",
               pos, ip, errtype, pt_errstr(pt_errcode(errcode)));
    }
}
