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
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <linux/limits.h>
#include <fstream>

#include "load_elf.h"
#include "pt_decoder.h"

#define LIBNAME "DRIPT Decoder"

pt_decoder_t::pt_decoder_t()
{
    trace_file_buf_ = NULL;

    instr_decoder_ = NULL;

    iscache_ = pt_iscache_alloc(NULL);
    if (iscache_ == NULL) {
        fprintf(stderr, "[PT Decoder Initilase] Failed to allocate iscache\n");
        exit(1);
    }

    preload_image_ = pt_image_alloc(NULL);
    if (preload_image_ == NULL) {
        pt_iscache_free(iscache_);
        fprintf(stderr, "Failed to allocate preload image\n");
        exit(1);
    }

    sb_session_ = pt_sb_alloc(iscache_);
    if (sb_session_ == NULL) {
        pt_iscache_free(iscache_);
        return -pte_nomem;
    }
}

pt_decoder_t::~pt_decoder_t()
{
    free(trace_file_buf_);
    pt_insn_free_decoder(instr_decoder_);
    pt_iscache_free(iscache_);
    pt_image_free(preload_image_);
    pt_sb_free(sb_session_);
}

bool
pt_decoder_t::init(const pt_decoder_config_t &config)
{
    struct pt_config pt_config;
    pt_config_init(&pt_config);
    /* Parse the cpu type. */
    if (config.cpu == "none") {
        pt_config.cpu = { 0, 0, 0, 0 };
    } else {
        pt_config.cpu.vendor = pcv_intel;
        int num = sscanf(config.cpu.c_str(), "%d/%d/%d", &config_.cpu.family,
                         &config_.cpu.model, &config_.cpu.stepping);
        if (num < 2) {
            fprintf(stderr, "[Configuration parsing error] - Invalid cpu type: %s\n",
                    config.cpu.c_str());
            return false;
        } else if (num == 2) {
            pt_config.cpu.stepping = 0;
        } else {
            /* Nothing */
        }
    }

    /* Parse preload image configs. */
    for (auto preload_image_config : config.preload_image_config) {
        uint64_t base, foffset, fsize;
        char filepath[PATH_MAX];
        /* Preprocessing a preload image config string. The format of the configuration
         * string is: <filepath>:<foffset>:<fsize>:<base>
         */
        if (sscanf(arg, "%[^:]:%lx:%lx:%lx", filepath, &foffset, &fsize, &base) != 4) {
            fprintf(stderr,
                    "[Configuration parsing error] - Invalid preload image config: %s\n",
                    preload_image_config.c_str());
            return false;
        }
        /* Loading a preload image. */
        if (load_preload_image(filepath, foffset, fsize, base) != 0) {
            fprintf(stderr,
                    "[Configuration parsing error] - Failed to load preload image: %s\n",
                    filepath);
            return false;
        }
    }

    /* Load the pt trace file and allocate the instruction decoder.*/
    if (load_trace_file(config.trace_file)) {
        fprintf(stderr, "[Configuration parsing error] - Failed to load trace file: %s\n",
                config.trace_file.c_str());
        return false;
    }
    instr_decoder_ = pt_insn_alloc_decoder(&config_);
    if (instr_decoder_ == NULL) {
        fprintf(stderr,
                "[Configuration parsing error] - Failed to create libipt instruction "
                "decoder.\n", );
        return false;
    }

    /* Parse the sideband perf event sample type*/
    uint64_t sample_type = 0;
    if (config.sb_sample_type.size() > 0) {
        if (sscanf(config.sb_pevent_config, "%lx", &sample_type) != 1) {
            fprintf(stderr,
                    "[Configuration parsing error] - Failed to parse sideband perf event "
                    "config.\n");
            return false;
        }
    }
    sb_pevent_config_.sample_type = sample_type;

    /* Parse the start address of the kernel image  */
    uint64_t kernel_start = 0;
    if (config.kernel_start.size() > 0) {
        if (sscanf(config.kernel_start.c_str(), "%lx", &kernel_start) != 1) {
            fprintf(stderr,
                    "[Configuration parsing error] - Failed to parse kernel start address.\n");
            return false;
        }
    }
    sb_pevent_config_.kernel_start = kernel_start;

    /* load kcore to sideband kernel image cache */
    if (config.kcore_file.size() > 0) {
        if (load_kernel_image(config.kcore_file)) {
            fprintf(stderr,
                    "[Configuration parsing error] - Failed to load kernel image: %s\n",
                    config.kcore_file.c_str());
            return false;
        }
    }

    /* allocate the primary sideband decoder */
    if (config.sb_primary_file.size() > 0) {
        if (alloc_sb_pevent_decoder(config.sb_primary_file)) {
            fprintf(stderr,
                    "[Configuration parsing error] - Failed to allocate primary sideband "
                    "perf event decoder.\n");
            return false;
        }
    }

    /* allocate the secondary sideband decoders */
    for (auto sb_secondary_file : config.sb_secondary_files) {
        if (alloc_sb_pevent_decoder(sb_secondary_file)) {
            fprintf(stderr,
                    "[Configuration parsing error] - Failed to allocate secondary sideband "
                    "perf event decoder.\n");
            return false;
        }
    }

}

bool
pt_decoder_t::load_preload_image(std::string &filepath, uint64_t foffset,
                                        uint64_t fsize, uint64_t base)
{
    /* libipt uses 'iscache' to store the image sections and use 'image' to store
     * the image identifier. So to initialize the cache, we need call
     * pt_iscache_add_file() first to load the image to cache. Then we call
     * pt_image_add_cached() to add the image section identifier to the image struct.
     */
    /* pt_iscache_add_file():  add a file section to an image section cache returns an
     * image section identifier(ISID) that uniquely identifies the section in this cache
     */
    int isid = pt_iscache_add_file(iscache_, filename.c_str(), foffset, fsize, base);
    if (isid < 0) {
        fprintf(stderr,
                "[Load preload image file error] - Failed to add %s at 0x%" PRIx64
                " to iscache: %s.\n",
                filename.c_str(), base, pt_errstr(pt_errcode(isid)));
        return false;
    }

    /* pt_image_add_cached(): add the section with isid to default image struct.
     */
    int errcode = pt_image_add_cached(preload_image_, iscache_, isid, NULL);
    if (errcode < 0) {
        fprintf(stderr,
                "[Load preload image file error] - Failed to add %s at 0x%" PRIx64
                " to image: %s.\n",
                filename.c_str(), base, pt_errstr(pt_errcode(errcode)));
        return false;
    }

    return true;
}

bool
pt_decoder_t::load_trace_file(std::string &filepath)
{
    errno = 0;
    std::ifstream trace_file(filepath, std::ios::binary | std::ios::ate);
    if (errno != 0) {
        fprintf(stderr, "[Load trace file error] - Failed to open %s: %s.\n",
                filepath.c_str(), strerror(errno));
        return false;
    }
    std::streamsize size = file.tellg();
    if (errno != 0) {
        fprintf(stderr, "[Load trace file error] - Failed to get file size of %s: %s.\n",
                filepath.c_str(), strerror(errno));
        return false;
    }
    file.seekg(0, std::ios::beg);
    if (errno != 0) {
        fprintf(stderr,
                "[Load trace file error] - Failed to seek to the beginning of %s: %s.\n",
                filepath.c_str(), strerror(errno));
        return false;
    }
    uint8_t *buffer = (uint8_t *)malloc(size * sizeof(uint8_t));
    file.read(buffer, size);
    if (errno != 0) {
        fprintf(stderr, "[Load trace file error] - Failed to read %s: %s.\n",
                filepath.c_str(), strerror(errno));
        return false;
    }
    config_->begin = buffer;
    config_->end = buffer + size;
    return true;
}

bool
pt_decoder_t::load_kernel_image(std::string &filepath)
{
    struct pt_image *kimage = pt_sb_kernel_image(sb_session_);
    int errcode = load_elf(iscache_, kimage, filepath, 0, LIBNAME, 0);
    if (errcode < 0) {
        fprintf(stderr,
                "[Load kernel image error] - Failed to load kernel image %s: %s.\n",
                filepath.c_str(), pt_errstr(pt_errcode(errcode)));
        return false;
    }
    return true;
}

bool
pt_decoder_t::alloc_sb_pevent_decoder(char *filename)
{
    struct pt_sb_pevent_config config = sb_pevent_config_;
    config.filename = filename;
    config.begin = (size_t)0;
    config.end = (size_t)0;

    int errcode = pt_sb_alloc_pevent_decoder(sb_session_, &config);
    if (errcode < 0) {
        fprintf(stderr,
                "[Allocate sideband perf event decoder error] - Failed to allocate "
                "sideband perf event decoder: %s.\n",
                pt_errstr(pt_errcode(errcode)));
        return false;
    }
    return true;
}

bool
pt_decoder_t::update_image(struct pt_image *image)
{
    int errcode = pt_insn_set_image(instr_decoder_, image);
    if (errcode < 0) {
        fprintf(stderr, "Failed to set image: %s.\n", pt_errstr(pt_errcode(errcode)));
        return false;
    }
    return true;
}
