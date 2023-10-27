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

#include "../common/utils.h"
#include "elf_loader.h"
#include "pt2ir.h"

namespace dynamorio {
namespace drmemtrace {

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

#ifdef __cplusplus
extern "C" {
#endif

/* The subsequent C code is a patch for libipt. This patch introduces embedded APIs,
 * enabling pt2ir_t to manage libipt's decoder's internal data structures and to
 * facilitate stream decoding.
 */

/* An Intel PT packet decoder.
 * The type mirrors the struct pt_packet_decoder found in
 * third_party/libipt/libipt/internal/include/pt_packet_decoder.h.
 */
struct pt_packet_decoder_t {
    /* The decoder configuration. */
    struct pt_config config;

    /* The current position in the trace buffer. */
    const uint8_t *pos;

    /* The position of the last PSB packet. */
    const uint8_t *sync;
};

/* A new C API for struct pt_packet_decoder *decoder is introduced. This API permits
 * setting the current position within the decoder's trace buffer.
 */
static void
pt_pkt_decoder_set_pos(struct pt_packet_decoder *decoder, uint8_t *pos)
{
    pt_packet_decoder_t *packet_decoder = (pt_packet_decoder_t *)decoder;
    packet_decoder->pos = pos;
}

/* A new C API for struct pt_packet_decoder *decoder is introduced. This API permits
 * setting the position of the last PSB packet within the decoder's trace buffer.
 */
static void
pt_pkt_decoder_set_sync(struct pt_packet_decoder *decoder, uint8_t *sync)
{
    pt_packet_decoder_t *packet_decoder = (pt_packet_decoder_t *)decoder;
    packet_decoder->sync = sync;
}

/* A new C API for struct pt_insn_decoder *decoder is introduced. Since the first field of
 * struct pt_insn_decoder *decoder is struct pt_packet_decoder, this API casts decoder to
 * struct pt_packet_decoder to access its pt_packet_decoder component.
 */
static struct pt_packet_decoder *
pt_instr_decoder_get_pkt_decoder(struct pt_insn_decoder *decoder)
{
    struct pt_packet_decoder *packet_decoder = (struct pt_packet_decoder *)decoder;
    return packet_decoder;
}

#ifdef __cplusplus
}
#endif

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

#define ERRMSG_HEADER "[drpt2ir] "

pt2ir_t::pt2ir_t()
    : pt2ir_initialized_(false)
    , pt_raw_buffer_size_(0)
    , pt_raw_buffer_data_size_(0)
    , pt_raw_buffer_lshift_offset_(0)
    , pt_instr_decoder_(nullptr)
    , pt_sb_iscache_(nullptr)
    , pt_sb_session_(nullptr)
    , pt_instr_status_(0)
    , pt_decoder_has_sync_(false)
    , pt_decoder_stop_offset_(0)
    , verbosity_(0)
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

    pt_raw_buffer_size_ = pt2ir_config.pt_raw_buffer_size;
    pt_raw_buffer_ = std::unique_ptr<uint8_t[]>(new uint8_t[pt_raw_buffer_size_]);
    pt_config.begin = pt_raw_buffer_.get();
    pt_config.end = pt_raw_buffer_.get() + pt_raw_buffer_size_;
    pt_instr_decoder_ = pt_insn_alloc_decoder(&pt_config);
    if (pt_instr_decoder_ == nullptr) {
        VPRINT(0, "Failed to create libipt instruction decoder.\n");
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

    pt2ir_stream_mode_ = pt2ir_config.stream_mode;
    pt2ir_initialized_ = true;
    return true;
}

bool
pt2ir_t::pre_decode()
{
    ASSERT(pt_instr_decoder_ != nullptr, "");
    ASSERT(pt_raw_buffer_data_size_ > 0, "");

    struct pt_packet_decoder *pt_pkt_decoder =
        pt_instr_decoder_get_pkt_decoder(pt_instr_decoder_);
    uint64_t pt_pkt_decoder_offset = 0;
    uint64_t pt_pkt_decoder_sync_offset = 0;

    /* Backup the offset and sync offset of the decoder. */
    int errcode = pt_pkt_get_offset(pt_pkt_decoder, &pt_pkt_decoder_offset);
    if (errcode < 0) {
        VPRINT(0, "Failed to get packet decoder offset: %s.\n",
               pt_errstr(pt_errcode(errcode)));
        return false;
    }
    errcode = pt_pkt_get_sync_offset(pt_pkt_decoder, &pt_pkt_decoder_sync_offset);
    if (errcode < 0) {
        VPRINT(0, "Failed to get packet decoder sync offset: %s.\n",
               pt_errstr(pt_errcode(errcode)));
        return false;
    }

    /* Attempt to perform pre-decoding to:
     * (1) Determine the offset of the final data packet, which will serve as the stop
     * position.
     * (2) Locate the last Packet Stream Boundary (PSB) packet within the current buffer.
     * Once identified, record its offset value. This offset value will then be used as
     * the reference point for the subsequent left shift operation on the buffer.
     */
    pt_decoder_stop_offset_ = 0;
    pt_raw_buffer_lshift_offset_ = 0;
    uint64_t offset = pt_pkt_decoder_offset;
    for (;;) {
        struct pt_packet packet;
        errcode = pt_pkt_next(pt_pkt_decoder, &packet, sizeof(packet));
        if (errcode < 0) {
            VPRINT(0, "Failed to get next packet: %s.\n", pt_errstr(pt_errcode(errcode)));
            return false;
        }
        if (packet.type > ppt_pad) {
            pt_decoder_stop_offset_ = offset + packet.size;
        }
        if (packet.type == ppt_psb) {
            pt_raw_buffer_lshift_offset_ = offset;
        }

        errcode = pt_pkt_get_offset(pt_pkt_decoder, &offset);
        if (errcode < 0) {
            VPRINT(0, "Failed to get offset of packet decoder: %s.\n",
                   pt_errstr(pt_errcode(errcode)));
            return false;
        }
        if (offset >= pt_raw_buffer_data_size_) {
            break;
        }
    }
    /* Reset the packet decoder to the beginning of the trace. */
    pt_pkt_decoder_set_pos(pt_pkt_decoder, pt_raw_buffer_.get() + pt_pkt_decoder_offset);
    pt_pkt_decoder_set_sync(pt_pkt_decoder,
                            pt_raw_buffer_.get() + pt_pkt_decoder_sync_offset);

    return true;
}

bool
pt2ir_t::post_decode()
{
    ASSERT(pt_instr_decoder_ != nullptr, "");
    ASSERT(pt_raw_buffer_data_size_ > pt_raw_buffer_lshift_offset_, "");

    if (pt_raw_buffer_lshift_offset_ != 0) {
        struct pt_packet_decoder *pt_pkt_decoder =
            pt_instr_decoder_get_pkt_decoder(pt_instr_decoder_);

        /* Implement a left-shift operation on the buffer to enable continuous stream
         * decoding and to make space for new data to be appended.
         */
        pt_raw_buffer_data_size_ -= pt_raw_buffer_lshift_offset_;
        memmove(pt_raw_buffer_.get(), pt_raw_buffer_.get() + pt_raw_buffer_lshift_offset_,
                pt_raw_buffer_data_size_);
        memset(pt_raw_buffer_.get() + pt_raw_buffer_data_size_, 0,
               pt_raw_buffer_size_ - pt_raw_buffer_data_size_);

        /* Reset the decoder's offset and sync offset. */
        uint64_t decode_offset = 0;
        int errcode = pt_pkt_get_offset(pt_pkt_decoder, &decode_offset);
        if (errcode < 0) {
            VPRINT(0, "Failed to get packet decoder offset: %s.\n",
                   pt_errstr(pt_errcode(errcode)));
            return false;
        }
        pt_pkt_decoder_set_pos(pt_pkt_decoder,
                               pt_raw_buffer_.get() + decode_offset -
                                   pt_raw_buffer_lshift_offset_);
        pt_pkt_decoder_set_sync(pt_pkt_decoder, pt_raw_buffer_.get());
    }
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

    ASSERT(pt_instr_decoder_ != nullptr, "");

    /* The PT raw trace comprises various packet types. Each packet can translate to
     * multiple instructions or none at all. Therefore, during instruction decoding,
     * libipt might process one or several packets for a single instruction. For our
     * decoding algorithm to ascertain the position of the currently decoded packet,
     * relying on the libipt's instruction decoder isn't sufficient for pinpointing the
     * exact packet position in the decoding process. Hence, we need to access the
     * libipt's packet decoder.
     */
    struct pt_packet_decoder *pt_pkt_decoder =
        pt_instr_decoder_get_pkt_decoder(pt_instr_decoder_);

    /* In non-stream mode, we reset both the decoder's offset and the sync offset before
     * decoding the subsequent complete PT data.
     */
    if (!pt2ir_stream_mode_) {
        pt_raw_buffer_data_size_ = 0;
        pt_pkt_decoder_set_pos(pt_pkt_decoder, pt_raw_buffer_.get());
        pt_pkt_decoder_set_sync(pt_pkt_decoder, nullptr);
    }

    /* Check if the raw buffer is large enough to hold the new data. */
    if (pt_raw_buffer_data_size_ + pt_data_size > pt_raw_buffer_size_) {
        return PT2IR_CONV_ERROR_RAW_TRACE_TOO_LARGE;
    }

    /* Append the new data to the buffer. */
    memcpy(pt_raw_buffer_.get() + pt_raw_buffer_data_size_, pt_data, pt_data_size);
    pt_raw_buffer_data_size_ += pt_data_size;

    /* In non-stream mode, we synchronize the decoder each time we start decoding a
     * complete PT raw trace.
     * In stream mode, synchronizing the decoder is necessary only once, prior to decoding
     * the first PT raw trace chunk.
     */
    if (!pt2ir_stream_mode_ || !pt_decoder_has_sync_) {
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
        pt_instr_status_ = pt_insn_sync_forward(pt_instr_decoder_);
        if (pt_instr_status_ < 0) {
            dx_decoding_error(pt_instr_status_, "sync instruction decoder error", 0);
            return PT2IR_CONV_ERROR_SYNC_PACKET;
        }
        pt_decoder_has_sync_ = true;
    }

    if (pt2ir_stream_mode_) {
        if (!pre_decode()) {
            return PT2IR_CONV_ERROR_PRE_DECODE;
        }
    } else {
        /* Set the stop offset greater than the data size to ensure that the last
         * packet is processed.
         */
        pt_decoder_stop_offset_ = pt_raw_buffer_data_size_ + 1;
    }

    struct pt_insn insn;
    memset(&insn, 0, sizeof(insn));
    int errcode = -pte_internal;
    for (;;) {
        bool reached_end = false;
        uint64_t decode_offset = 0;
        /* Before starting to decode instructions, we need to handle the event before
         * the instruction trace. For example, if a mmap2 event happens, we need to
         * switch the cached image.
         */
        while ((pt_instr_status_ & pts_event_pending) != 0) {

            /* Check if the decoder has arrived at the stop position before decoding the
             * next event.
             */
            errcode = pt_pkt_get_offset(pt_pkt_decoder, &decode_offset);
            if (errcode < 0) {
                dx_decoding_error(errcode, "get the offset of decoder error", insn.ip);
                return PT2IR_CONV_ERROR_GET_DECODER_OFFSET;
            }
            if (decode_offset >= pt_decoder_stop_offset_) {
                reached_end = true;
                break;
            }

            struct pt_event event;
            pt_instr_status_ = pt_insn_event(pt_instr_decoder_, &event, sizeof(event));
            if (pt_instr_status_ < 0) {
                dx_decoding_error(pt_instr_status_, "get pending event error", insn.ip);
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

        if (reached_end) {
            break;
        }

        /* Check if the decoder has arrived at the stop position before decoding the next
         * instruction.
         */
        errcode = pt_pkt_get_offset(pt_pkt_decoder, &decode_offset);
        if (errcode < 0) {
            dx_decoding_error(errcode, "get the offset of decoder error", insn.ip);
            return PT2IR_CONV_ERROR_GET_DECODER_OFFSET;
        }
        if (decode_offset >= pt_decoder_stop_offset_) {
            break;
        }
        if ((pt_instr_status_ & pts_eos) != 0) {
            break;
        }

        /* Decode PT raw trace to pt_insn. */
        pt_instr_status_ = pt_insn_next(pt_instr_decoder_, &insn, sizeof(insn));
        if (pt_instr_status_ < 0) {
            dx_decoding_error(pt_instr_status_, "get next instruction error", insn.ip);
            return PT2IR_CONV_ERROR_DECODE_NEXT_INSTR;
        }

        /* Use drdecode to decode insn(pt_insn) to instr_t. */
        instr_t *instr = instr_create(drir.get_drcontext());
        instr_init(drir.get_drcontext(), instr);
        instr_set_isa_mode(instr, insn.mode == ptem_32bit ? DR_ISA_IA32 : DR_ISA_AMD64);
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

    if (pt2ir_stream_mode_) {
        if (!post_decode()) {
            return PT2IR_CONV_ERROR_POST_DECODE;
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
        VPRINT(0, "Could not determine offset: %s\n", pt_errstr(pt_errcode(err)));
        VPRINT(0, "[?, " HEX64_FORMAT_STRING "] %s: %s\n", ip, errtype,
               pt_errstr(pt_errcode(errcode)));
    } else {
        VPRINT(0, "[" UINT64_FORMAT_STRING ", IP:" HEX64_FORMAT_STRING "] %s: %s\n", pos,
               ip, errtype, pt_errstr(pt_errcode(errcode)));
    }
}

} // namespace drmemtrace
} // namespace dynamorio
