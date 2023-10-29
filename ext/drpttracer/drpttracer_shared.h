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

/* DynamoRIO Intel PT Tracing Extension. */

#ifndef _DRPTTRACER_SHARED_H_
#define _DRPTTRACER_SHARED_H_ 1

/**
 * @file drpttracer_shared.h
 * @brief Header for shared structs related to the DynamoRIO Intel PT Tracing Extension.
 * These structs were extracted from drpttracer.h to allow easier sharing with DR
 * clients in cases when we want to avoid pulling in other stuff in drpttracer.h.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

#ifdef WINDOWS
#    define START_PACKED_STRUCTURE ACTUAL_PRAGMA(pack(push, 1))
#    define END_PACKED_STRUCTURE ACTUAL_PRAGMA(pack(pop))
#else
#    define START_PACKED_STRUCTURE /* nothing */
#    define END_PACKED_STRUCTURE __attribute__((__packed__))
#endif

/**
 * The type of PT trace's metadata.
 *
 * \note drpttracer uses the cpuid instruction to get the cpu_family, cpu_model and
 * cpu_stepping. The cpu_family, cpu_model and cpu_stepping are used to initialize the PT
 * config of pt2ir_t when decoding a PT trace.
 *
 * \note drpttracer gets the time_shift, time_mult and time_zero from the opened perf
 * event file's head. The time_shift, time_mult and time_zero are used to initialize the
 * PT sideband config of pt2ir_t when decoding a PT trace.
 *
 *  All fields are little-endian.
 */
START_PACKED_STRUCTURE
typedef struct _pt_metadata_t {
    uint16_t cpu_family;  /**< The CPU family. */
    uint8_t cpu_model;    /**< The CPU mode. */
    uint8_t cpu_stepping; /**< The CPU stepping. */

    /**
     * The time shift. pt2ir_t uses it to synchronize the time of the PT trace and
     * sideband data.
     * \note time_shift = perf_event_mmap_page.time_shift
     */
    uint16_t time_shift;

    /**
     * The time multiplier. pt2ir_t uses it to synchronize the time of the PT trace and
     * sideband data.
     * \note time_mult = perf_event_mmap_page.time_mult
     */
    uint32_t time_mult;

    /**
     * The time zero. pt2ir_t uses it to synchronize the time of the PT trace and
     * sideband data. \note time_zero = perf_event_mmap_page.time_zero
     */
    uint64_t time_zero;
} END_PACKED_STRUCTURE pt_metadata_t;

#ifdef __cplusplus
}
#endif

#endif /* _DRPTTRACER_SHARED_H_ */
