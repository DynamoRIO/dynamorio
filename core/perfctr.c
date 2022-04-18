/* **********************************************************
 * Copyright (c) 2019 Google, Inc.  All rights reserved.
 * Copyright (c) 2001-2008 VMware, Inc.  All rights reserved.
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
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */

#include "globals.h"

#ifdef PAPI /* around whole file */

#    include "perfctr.h"
#    include "x86-events.h"
#    include "papi.h"
#    include <stdlib.h>
#    include <stdio.h>
#    include <time.h>

extern dr_statistics_t *stats;
int perfctr_eventset;
struct {
    int num;
    char *name;
} papi_events[] = { { P6_CPU_CLK_UNHALTED << 8, "Total cycles" },
                    { P6_INST_DECODER << 8, "Instructions decoded" },
                    { P6_INST_RETIRED << 8, "Instructions Retired" },
                    { P6_UOPS_RETIRED << 8, "Micro-ops retired" },
                    { P6_DATA_MEM_REFS << 8, "Total data memory refs (loads + stores" },
                    { P6_DCU_MISS_OUTSTANDING << 8, "DCU miss cycles outstanding" },
                    { P6_IFU_FETCH << 8, "Instruction Fetches" },
                    { P6_IFU_FETCH_MISS << 8, "IFU (instruction L1 ?) misses" },
                    { P6_ITLB_MISS << 8, "ITLB misses" },
                    { P6_IFU_MEM_STALL << 8, "Cycles instruction fetch is stalled" },
                    { P6_ILD_STALL << 8, "Cycles instruction length decoder stalled" },
                    { ((0x0f << 8) | P6_L2_IFETCH) << 8, "L2 instruction fetches" },
                    { ((0x0f << 8) | P6_L2_LD) << 8, "L2 loads" },
                    { ((0x0f << 8) | P6_L2_ST) << 8, "L2 stores" },
                    { P6_BR_INST_DECODED << 8, "Branch instructions decoded" },
                    { P6_BR_INST_RETIRED << 8, "Branch instructions retired" },
                    { P6_BR_MISS_PRED_RETIRED << 8, "Branch insts mispredicted retired" },
                    { P6_BR_TAKEN_RETIRED << 8, "Branch insts taken retired" },
                    { P6_BR_MISS_PRED_TAKEN_RET << 8,
                      "Branch insts mispredicted, taken retired" },
                    { P6_BTB_MISSES << 8, "BTB misses" },
                    { P6_BR_BOGUS << 8, "Bogus Branches" },
                    { P6_RESOURCE_STALLS << 8, "Misc. resource stalls" },
                    { P6_BACLEARS << 8, "BACLEAR asserted" },
                    { P6_DCU_LINES_IN << 8, "DCU lines allocated" },
                    { P6_L2_LINES_IN << 8, "L2 lines allocated" },
                    { PIII_EMON_KNI_PREF_DISPATCHED << 8, "Prefetch NTA dispatched" },
                    { PIII_EMON_KNI_PREF_MISS << 8, "Prefetch NTA miss all caches" } };

void
hardware_perfctr_init()
{
    int a;
    perfctr_eventset = PAPI_NULL;

    ASSERT(NUM_EVENTS == (sizeof(papi_events) / 8));

#    ifdef UNIX
    ASSERT(!INTERNAL_OPTION(profile_pcs));
#    endif

    LOG(GLOBAL, LOG_TOP, 1, "Initializing PAPI\n");

    if (PAPI_library_init(PAPI_VER_CURRENT) != PAPI_VER_CURRENT) {
        LOG(GLOBAL, LOG_TOP, 1, "Error initializing PAPI.\n");
    }

    if (NUM_EVENTS > 2) {
        LOG(GLOBAL, LOG_TOP, 3, "Initializing PAPI multiplexing\n");
        if (PAPI_multiplex_init() != PAPI_OK) {
            if (d_r_stats->loglevel > 0 && (d_r_stats->logmask & LOG_TOP) != 0)
                LOG(GLOBAL, LOG_TOP, 1, "Error initializing PAPI multiplexing\n");
        }
    }
    if (PAPI_create_eventset(&perfctr_eventset) != PAPI_OK) {
        LOG(GLOBAL, LOG_TOP, 1, "Error creating PAPI eventset\n");
    }

    if (NUM_EVENTS > 2) {
        if (PAPI_set_multiplex(&perfctr_eventset) != PAPI_OK) {
            LOG(GLOBAL, LOG_TOP, 1, "Error setting multiplexed eventset in PAPI\n");
        }
    }

    for (a = 0; a < NUM_EVENTS; a++) {
        if (PAPI_add_event(&perfctr_eventset, papi_events[a].num) != PAPI_OK) {
            LOG(GLOBAL, LOG_TOP, 1, "Error adding events in PAPI\n");
        }
    }

    if (PAPI_start(perfctr_eventset) != PAPI_OK) {
        LOG(GLOBAL, LOG_TOP, 1, "Error starting hardware performance counters\n");
    }
}

void
hardware_perfctr_exit()
{
    int a;
    uint64 vals[NUM_EVENTS]; /* only used if nullcalls */
    uint64 *val_array;

    if (dynamo_options.nullcalls) {
        val_array = vals;
    } else {
        val_array = d_r_stats->perfctr_vals;
    }
    if (PAPI_stop(perfctr_eventset, val_array) != PAPI_OK) {
        LOG(GLOBAL, LOG_TOP, 1,
            "Error stopping and reading hardware performance counters\n");
    }
    for (a = 0; a < NUM_EVENTS; a++) {
        LOG(GLOBAL, LOG_TOP, 1, "Counter %d = %llu (%s)\n", a + 1, val_array[a],
            papi_events[a].name);
    }
}

void
perfctr_update_gui()
{
    if (PAPI_read(perfctr_eventset, d_r_stats->perfctr_vals) != PAPI_OK) {
        LOG(GLOBAL, LOG_TOP, 1,
            "Error stopping and reading hardware performance counters\n");
    }
}

#endif /* PAPI */
