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

#include <sys/stat.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <errno.h>
#include <fstream>

#include "intel-pt.h"
#include "libipt-sb.h"

#include "elf_loader.h"
#include "pt2ir.h"

#undef VPRINT_HEADER
#define VPRINT_HEADER()               \
    do {                              \
        fprintf(stderr, "drpt2ir: "); \
    } while (0)

#undef VPRINT
#define VPRINT(level, ...)                 \
    do {                                   \
        if (this->verbosity_ >= (level)) { \
            VPRINT_HEADER();               \
            fprintf(stderr, __VA_ARGS__);  \
            fflush(stderr);                \
        }                                  \
    } while (0)

pt_iscache_autoclean_t::pt_iscache_autoclean_t()
{
    iscache = pt_iscache_alloc(nullptr);
}

pt_iscache_autoclean_t::~pt_iscache_autoclean_t()
{
    if (iscache != nullptr) {
        pt_iscache_free(iscache);
        iscache = nullptr;
    }
}

pt_iscache_autoclean_t pt2ir_t::share_iscache_;

enum pt2ir_data_buffer_state_t {
    /* No data buffer is currently allocated.*/
    PT2IR_DATA_BUFFER_STATE_EMPTY,
    /* The data buffer has been allocated, but it contains no data. */
    PT2IR_DATA_BUFFER_STATE_INIT,
    /* The data in the buffer has just been expanded. */
    PT2IR_DATA_BUFFER_STATE_EXTEND,
    /* The data in the buffer has just been shrinked. */
    PT2IR_DATA_BUFFER_STATE_SHRINK,
    /* The data buffer is ready for use. */
    PT2IR_DATA_BUFFER_STATE_READY
};

/* The buffer stores the PT data chunk retrieved from the trace file. Since the end of the
 * data chunk may contain a partial packet,  we use 'data_size_' to denote the full size
 * of the data chunk, while 'valid_data_size_' represents the size of the data excluding
 * the partial packet at the end of the data.
 * Given that the Intel Libipt decoder doesn't support streaming decoding and can only
 * decode a chunk starting with a PSB packet, we define 'window_size_' to signify the size
 * of the data chunk that the decoder can process.
 *
 * The buffer usage follows the below steps:
 * (1) Initially, the data chunk retrieved from the trace file is appended to the buffer.
 * The 'data_size_' and 'valid_data_size_' are then updated.
 * (2) We determine the position of the last PSB Packet in the buffer, and set
 * 'window_size_' equal to the size of the data chunk excluding the last PSB packet itself
 * and the data after the last PSB packet.
 * (3) The decoder is then employed to decode the data chunk within the window.
 * (4) Upon completion of step 3, all data in the window is deleted. The data following
 * the window is shifted to the buffer's start, and 'data_size_' and 'valid_data_size_'
 * are updated accordingly. The window size is reset to the new 'valid_data_size_'.
 * (5) Reset the decoder and decode the data chunk from the window's start position.
 * (6) Once the end of the window is reached, the decoding process is halted and returns
 * to step 1.
 */
class pt2ir_data_buffer_t {
public:
    pt2ir_data_buffer_t()
        : raw_buffer_size_(0)
        , data_size_(0)
        , vaild_data_size_(0)
        , window_size_(0)
        , state_(PT2IR_DATA_BUFFER_STATE_EMPTY)
        , verbosity_(0)
    {
    }

    ~pt2ir_data_buffer_t()
    {
    }

    void
    init(IN size_t raw_buffer_size, IN int verbosity = 0)
    {
        raw_buffer_size_ = raw_buffer_size;
        raw_buffer_ = std::unique_ptr<uint8_t[]>(new uint8_t[raw_buffer_size_]);
        data_size_ = 0;
        vaild_data_size_ = 0;
        window_size_ = 0;
        state_ = PT2IR_DATA_BUFFER_STATE_INIT;
        verbosity_ = verbosity;
    }

    bool
    extend_data(IN const uint8_t *data, IN size_t data_size)
    {
        if (state_ != PT2IR_DATA_BUFFER_STATE_INIT &&
            state_ != PT2IR_DATA_BUFFER_STATE_READY) {
            VPRINT(
                0,
                "Data can only be appended to the buffer when it has been initialized or "
                "when the buffer is ready for use.\n");
            return false;
        }

        if (data == nullptr || data_size == 0) {
            VPRINT(0, "Attempt to append empty data.\n");
            return false;
        }

        if (vaild_data_size_ + data_size > raw_buffer_size_) {
            VPRINT(0,
                   "Attempt to append data that exceeds the permissible size limit.\n");
            return false;
        }

        memcpy(raw_buffer_.get() + vaild_data_size_, data, data_size);
        data_size_ += data_size;
        state_ = PT2IR_DATA_BUFFER_STATE_EXTEND;
        return true;
    }

    bool
    shrink_data()
    {
        if (state_ != PT2IR_DATA_BUFFER_STATE_READY) {
            VPRINT(0, "Data can only be shrinked when the buffer is ready for use.\n");
            return false;
        }

        if (vaild_data_size_ == 0) {
            VPRINT(0, "Attempt to shrink empty data.\n");
            return false;
        }

        memcpy(raw_buffer_.get(), raw_buffer_.get() + window_size_,
               vaild_data_size_ - window_size_);
        data_size_ -= window_size_;
        state_ = PT2IR_DATA_BUFFER_STATE_SHRINK;
        return true;
    }

    /* Each time new data is added to the buffer, it necessitates the expansion of the
     * processing window.
     */
    bool
    extend_window(IN size_t window_size, IN size_t vaild_data_size)
    {
        if (state_ != PT2IR_DATA_BUFFER_STATE_EXTEND) {
            VPRINT(0,
                   "Data window can only be extend when the buffer has been extended.\n");
            return false;
        }

        if (window_size > data_size_ || vaild_data_size > data_size_ ||
            window_size > vaild_data_size) {
            VPRINT(0, "Invaild input arguments.\n");
            return false;
        }

        window_size_ = window_size;
        vaild_data_size_ = vaild_data_size;
        state_ = PT2IR_DATA_BUFFER_STATE_READY;
        return true;
    }

    /* Each time the data in the buffer diminishes, it's necessary to shrink the
     * processing window correspondingly.
     */
    bool
    shrink_window()
    {
        if (state_ != PT2IR_DATA_BUFFER_STATE_SHRINK) {
            VPRINT(0,
                   "Data window can only be shrink when the buffer has been shrinked.\n");
            return false;
        }
        vaild_data_size_ -= window_size_;
        window_size_ = vaild_data_size_;
        state_ = PT2IR_DATA_BUFFER_STATE_READY;
        return true;
    }

    uint8_t *
    get_buffer_start() const
    {
        if (state_ == PT2IR_DATA_BUFFER_STATE_EMPTY | raw_buffer_.get() == nullptr) {
            VPRINT(0, "The buffer is not initialized.\n");
            return nullptr;
        }

        return raw_buffer_.get();
    }

    uint8_t *
    get_buffer_end() const
    {
        if (state_ == PT2IR_DATA_BUFFER_STATE_EMPTY | raw_buffer_.get() == nullptr) {
            VPRINT(0, "The buffer is not initialized.\n");
            return nullptr;
        }

        return raw_buffer_.get() + raw_buffer_size_;
    }

    size_t
    get_data_size() const
    {
        return data_size_;
    }

    size_t
    get_window_size() const
    {
        return window_size_;
    }

    bool
    can_shrink()
    {
        return window_size_ < vaild_data_size_;
    }

private:
    /* Buffer for caching the PT raw trace. */
    std::unique_ptr<uint8_t[]> raw_buffer_;

    /* The size of the raw buffer. */
    size_t raw_buffer_size_;

    /* The size of the data in the buffer.
     * The data at the end of the buffer may contain some invalid data. For example an
     * incomplete packet.
     */
    size_t data_size_;

    /* The size of the vaild data in the buffer.
     * Valid data encompasses the last fully formed packet and all preceding data up to
     * that packet.
     */
    size_t vaild_data_size_;

    /* The size of the data window.
     * There are two types windows:
     * 1. The window start with a Packet Stream Boundary(PSB) packet and end with a packet
     * before the last PSB packet.
     * 2. The window start with a PSB packet and end with the last complete packet.
     */
    size_t window_size_;

    /* The state of the data buffer. */
    pt2ir_data_buffer_state_t state_;

    /* Integer value representing the verbosity level for notifications. */
    int verbosity_;
};

pt2ir_t::pt2ir_t()
    : pt2ir_initialized_(false)
    , pt_pkt_decoder_(nullptr)
    , pt_instr_decoder_(nullptr)
    , pt_sb_iscache_(nullptr)
    , pt_sb_session_(nullptr)
    , pt_instr_status_(0)
    , pt_decoder_auto_sync_(true)
    , verbosity_(0)
{
}

pt2ir_t::~pt2ir_t()
{
    if (pt_sb_session_ != nullptr)
        pt_sb_free(pt_sb_session_);
    if (pt_sb_iscache_ != nullptr)
        pt_iscache_free(pt_sb_iscache_);
    if (pt_pkt_decoder_ != nullptr)
        pt_pkt_free_decoder(pt_pkt_decoder_);
    if (pt_instr_decoder_ != nullptr)
        pt_insn_free_decoder(pt_instr_decoder_);
    pt2ir_initialized_ = false;
}

bool
pt2ir_t::init(IN pt2ir_config_t &pt2ir_config, IN int verbosity)
{
    verbosity_ = verbosity;
    if (pt2ir_initialized_) {
        VPRINT(0, "pt2ir_t is already initialized.\n");
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
            VPRINT(0, "Failed to get cpu errata: %s.\n", pt_errstr(pt_errcode(errcode)));
            return false;
        }
    }

    pt2ir_buffer_ = std::make_unique<pt2ir_data_buffer_t>();
    pt2ir_buffer_.get()->init(pt2ir_config.pt_raw_buffer_size);
    pt_config.begin = pt2ir_buffer_.get()->get_buffer_start();
    pt_config.end = pt2ir_buffer_.get()->get_buffer_end();
    pt_instr_decoder_ = pt_insn_alloc_decoder(&pt_config);
    if (pt_instr_decoder_ == nullptr) {
        VPRINT(0, "Failed to create libipt instruction decoder.\n");
        return false;
    }

    pt_pkt_decoder_ = pt_pkt_alloc_decoder(&pt_config);
    if (pt_pkt_decoder_ == nullptr) {
        VPRINT(0, "Failed to create libipt packet decoder.\n");
        return false;
    }

    /* If an ELF file is provided, load it into the image of the instruction decoder. */
    if (!pt2ir_config.elf_file_path.empty() && share_iscache_.iscache != nullptr) {
        if (!elf_loader_t::load(pt2ir_config.elf_file_path.c_str(), pt2ir_config.elf_base,
                                share_iscache_.iscache,
                                pt_insn_get_image(pt_instr_decoder_))) {
            VPRINT(0, "Failed to load ELF file(%s) to.\n",
                   pt2ir_config.elf_file_path.c_str());
            return false;
        }
    }

    /* The following code initializes sideband decoders. However, decoding DRMEMTRACE
     * format kernel traces does not require sideband decoders.
     */

    /* Init the sideband image section cache and session. */
    pt_sb_iscache_ = pt_iscache_alloc(nullptr);
    if (pt_sb_iscache_ == nullptr) {
        VPRINT(0, "Failed to allocate sideband image section cache.\n");
        return false;
    }
    pt_sb_session_ = pt_sb_alloc(pt_sb_iscache_);
    if (pt_sb_session_ == nullptr) {
        VPRINT(0, "Failed to allocate sb session.\n");
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
            VPRINT(0, "Failed to allocate primary sideband perf event decoder: %s.\n",
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
                VPRINT(0,
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
            VPRINT(0, "Failed to load kcore(%s) to sideband image sections cache.\n",
                   pt2ir_config.sb_kcore_path.c_str());
            return false;
        }
    }

    /* Initialize all sideband decoders. It needs to be called after all sideband decoders
     * have been allocated.
     */
    int errcode = pt_sb_init_decoders(pt_sb_session_);
    if (errcode < 0) {
        VPRINT(0, "Failed to initialize sideband session: %s.\n",
               pt_errstr(pt_errcode(errcode)));
        return false;
    }

    pt2ir_initialized_ = true;
    return true;
}

pt2ir_convert_status_t
pt2ir_t::convert(IN const uint8_t *pt_data, IN size_t pt_data_size, INOUT drir_t &drir)
{
    if (!pt2ir_initialized_) {
        return PT2IR_CONV_ERROR_NOT_INITIALIZED;
    }

    if (pt_data == nullptr || pt_data_size <= 0) {
        return PT2IR_CONV_ERROR_INVALID_INPUT;
    }

    if (!pt2ir_buffer_.get()->extend_data(pt_data, pt_data_size)) {
        return PT2IR_CONV_ERROR_RAW_TRACE_TOO_LARGE;
    }

    int pkt_status = 0;
    if (pt_decoder_auto_sync_) {
        /* Sync decoder to next Packet Stream Boundary (PSB) packet, and then
         * decode instructions. If there is no PSB packet, the decoder will be synced
         * to the end of the trace. If an error occurs, we will call dx_decoding_error
         * to print the error information.
         * What are PSB packets? Below answer is quoted from Intel 64 and IA-32
         * Architectures Software Developer’s Manual 32.1.1.1 Packet Summary:
         * “Packet Stream Boundary (PSB) packets: PSB packets act as ‘heartbeats’ that are
         * generated at regular intervals (e.g., every 4K trace packet bytes). These
         * packets allow the packet decoder to find the packet boundaries within the
         * output data stream; a PSB packet should be the first packet that a decoder
         * looks for when beginning to decode a trace.”
         */
        int pkt_status = pkt_status = pt_pkt_sync_forward(pt_pkt_decoder_);
        if (pkt_status < 0) {
            VPRINT(0, "Failed to sync packet decoder: %s.\n",
                   pt_errstr(pt_errcode(pkt_status)));
            return PT2IR_CONV_ERROR_PKT_DECODER_SYNC;
        }
        pt_instr_status_ = pt_insn_sync_forward(pt_instr_decoder_);
        if (pt_instr_status_ < 0) {
            VPRINT(0, "Failed to sync instruction decoder: %s.\n",
                   pt_errstr(pt_errcode(pt_instr_status_)));
            return PT2IR_CONV_ERROR_INSTR_DECODER_SYNC;
        }
        pt_decoder_auto_sync_ = false;
    }

    uint64_t last_data_pkt_offset = 0;
    uint64_t last_in_window_data_pkt_offset = 0;
    uint64_t vaild_data_size = 0;
    uint64_t last_psb_offset = 0;
    uint64_t pkt_decoder_offset = 0;
    enum pt_packet_type last_data_pkt_type = ppt_invalid;
    enum pt_packet_type last_in_window_data_pkt_type = ppt_invalid;
    for (;;) {
        struct pt_packet packet;
        int errcode = pt_pkt_next(pt_pkt_decoder_, &packet, sizeof(packet));
        if (errcode < 0) {
            VPRINT(0, "Failed to get next packet: %s.\n", pt_errstr(pt_errcode(errcode)));
            return PT2IR_CONV_ERROR_DECODE_NEXT_PKT;
        }
        if (packet.type == ppt_psb) {
            last_psb_offset = pkt_decoder_offset;
        }
        vaild_data_size = pkt_decoder_offset;
        if (packet.type > ppt_pad) {
            if (last_psb_offset == 0) {
                last_in_window_data_pkt_offset = pkt_decoder_offset;
                last_in_window_data_pkt_type = packet.type;
            }
            last_data_pkt_offset = pkt_decoder_offset;
            last_data_pkt_type = packet.type;
        }
        // if (packet.type == ppt_psb || packet.type == ppt_psbend) {
        //     VPRINT(0, "pkt_decoder_offset: %lu, pkt_type: %d\n", pkt_decoder_offset,
        //            packet.type);
        // }
        VPRINT(0, "pkt_decoder_offset: %lu, pkt_type: %d\n", pkt_decoder_offset,
               packet.type);
        errcode = pt_pkt_get_offset(pt_pkt_decoder_, &pkt_decoder_offset);
        if (errcode < 0) {
            VPRINT(0, "Failed to get offset of packet decoder: %s.\n",
                   pt_errstr(pt_errcode(errcode)));
            return PT2IR_CONV_ERROR_GET_PKT_DECODER_OFFSET;
        }
        if (pkt_decoder_offset > pt2ir_buffer_.get()->get_data_size()) {
            break;
        }
    }
    VPRINT(
        0,
        "last_psb_offset: %lu, last_data_pkt_offset: %lu, "
        "last_in_window_data_pkt_offset: %lu, "
        "vaild_data_size: %lu, last_data_pkt_type %d, last_in_window_data_pkt_type: %d\n",
        last_psb_offset, last_data_pkt_offset, last_in_window_data_pkt_offset,
        vaild_data_size, last_data_pkt_type, last_in_window_data_pkt_type);
    if (last_psb_offset > 0) {
        pt2ir_buffer_.get()->extend_window(last_psb_offset, vaild_data_size);
    } else {
        pt2ir_buffer_.get()->extend_window(vaild_data_size, vaild_data_size);
    }

    for (;;) {
        VPRINT(0,
               "last_in_window_data_pkt_offset: %lu,  windown_size: %lu, "
               "vaild_data_size: %lu, pt_instr_status_ %d\n",
               last_in_window_data_pkt_offset, pt2ir_buffer_.get()->get_window_size(),
               pt2ir_buffer_.get()->get_data_size(), pt_instr_status_);
        struct pt_insn insn;
        memset(&insn, 0, sizeof(insn));
        uint64_t insn_decoder_offset = 0;
        /* Decode the raw trace data surround by PSB. */
        for (;;) {
            int errcode = pt_insn_get_offset(pt_instr_decoder_, &insn_decoder_offset);
            if (errcode < 0) {
                VPRINT(0, "Failed to get offset of instruction decoder: %s.\n",
                       pt_errstr(pt_errcode(errcode)));
                return PT2IR_CONV_ERROR_GET_INSTR_DECODER_OFFSET;
            }

            if (insn_decoder_offset >= last_in_window_data_pkt_offset) {
                VPRINT(0,
                       "insn_decoder_offset: %lu, last_in_window_data_pkt_offset: %lu, "
                       "pt_instr_status_ %d\n",
                       insn_decoder_offset, last_in_window_data_pkt_offset,
                       pt_instr_status_);
                break;
            }
            // VPRINT(0, "insn_decoder_offset: %lx, last_in_window_data_pkt_offset:
            // %lx\n",
            //        insn_decoder_offset, last_in_window_data_pkt_offset);

            /* Before starting to decode instructions, we need to handle the event before
             * the instruction trace. For example, if a mmap2 event happens, we need to
             * switch the cached image.
             */
            while ((pt_instr_status_ & pts_event_pending) != 0) {
                struct pt_event event;

                pt_instr_status_ =
                    pt_insn_event(pt_instr_decoder_, &event, sizeof(event));
                if (pt_instr_status_ < 0) {
                    dx_decoding_error(pt_instr_status_, "get pending event error",
                                      insn.ip);
                    return PT2IR_CONV_ERROR_GET_PENDING_EVENT;
                }

                /* Use a sideband session to check if pt_event is an image switch event.
                 * If so, change the image in 'pt_instr_decoder_' to the target image.
                 */
                struct pt_image *image = nullptr;
                int errcode =
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

            /* Decode PT raw trace to pt_insn. */
            pt_instr_status_ = pt_insn_next(pt_instr_decoder_, &insn, sizeof(insn));
            if (pt_instr_status_ < 0) {
                dx_decoding_error(pt_instr_status_, "get next instruction error",
                                  insn.ip);
                return PT2IR_CONV_ERROR_DECODE_NEXT_INSTR;
            }

            /* Use drdecode to decode insn(pt_insn) to instr_t. */
            instr_t *instr = instr_create(drir.get_drcontext());
            instr_init(drir.get_drcontext(), instr);
            instr_set_isa_mode(instr,
                               insn.mode == ptem_32bit ? DR_ISA_IA32 : DR_ISA_AMD64);
            bool instr_valid = false;
            if (decode(drir.get_drcontext(), insn.raw, instr) != nullptr)
                instr_valid = true;
            instr_set_translation(instr, (app_pc)insn.ip);
            instr_allocate_raw_bits(drir.get_drcontext(), instr, insn.size);
            /* TODO i#2103: Currently, the PT raw data may contain 'STAC' and 'CLAC'
             * instructions that are not supported by Dynamorio.
             */
            if (!instr_valid) {
                /* The decode() function will not correctly identify the raw bits for
                 * invalid instruction. So we need to set the raw bits of instr manually.
                 */
                instr_free_raw_bits(drir.get_drcontext(), instr);
                instr_set_raw_bits(instr, insn.raw, insn.size);
                instr_allocate_raw_bits(drir.get_drcontext(), instr, insn.size);
#ifdef DEBUG

                /* Print the invalid instruction‘s PC and raw bytes in DEBUG builds. */
                if (verbosity_ >= 1) {
                    fprintf(stderr,
                            "drpt2ir: <INVALID> <raw " PFX "-" PFX " ==", (app_pc)insn.ip,
                            (app_pc)insn.ip + insn.size);
                    for (int i = 0; i < insn.size; i++) {
                        fprintf(stderr, " %02x", insn.raw[i]);
                    }
                    fprintf(stderr, ">\n");
                }
#endif
            }
            drir.append(instr);
        }
        if (pt2ir_buffer_.get()->can_shrink()) {
            last_in_window_data_pkt_offset =
                last_data_pkt_offset - pt2ir_buffer_.get()->get_window_size();
            last_data_pkt_offset = last_in_window_data_pkt_offset;
            pt2ir_buffer_.get()->shrink_data();
            pt2ir_buffer_.get()->shrink_window();
            /* Before decoding a new data block, the decoder must reset the decode
             * position to the start of the buffer. Since Libipt does not provide an API
             * to reset the decode position, pt_insn_sync_set() is used for manual
             * synchronization.
             */
            int pkt_status = pt_pkt_sync_set(pt_pkt_decoder_, 0);
            if (pkt_status < 0) {
                VPRINT(0, "Failed to sync packet decoder: %s.\n",
                       pt_errstr(pt_errcode(pkt_status)));
                return PT2IR_CONV_ERROR_PKT_DECODER_SYNC;
            }
            pt_instr_status_ = pt_insn_sync_set(pt_instr_decoder_, 0);
            if (pt_instr_status_ < 0) {
                VPRINT(0,
                       "Failed to sync instruction decoder after data shrinking: %s.\n",
                       pt_errstr(pt_errcode(pt_instr_status_)));
                return PT2IR_CONV_ERROR_INSTR_DECODER_SYNC;
            }
        } else {
            break;
        }
    }
    instr_t *instr = instrlist_first(drir.get_ilist());
    uint64_t count = 0;
    while (instr != NULL) {
        count++;
        instr = instr_get_next(instr);
    }
    VPRINT(0, "Decoding finished(%lu).\n", count);
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
        VPRINT(0, "Could not determine offset: %s\n", pt_errstr(pt_errcode(err)));
        VPRINT(0, "[?, " HEX64_FORMAT_STRING "] %s: %s\n", ip, errtype,
               pt_errstr(pt_errcode(errcode)));
    } else {
        VPRINT(0, "[" UINT64_FORMAT_STRING ", IP:" HEX64_FORMAT_STRING "] %s: %s\n", pos,
               ip, errtype, pt_errstr(pt_errcode(errcode)));
    }
}
