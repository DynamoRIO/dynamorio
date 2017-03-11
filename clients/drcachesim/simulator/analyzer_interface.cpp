/* **********************************************************
 * Copyright (c) 2017 Google, Inc.  All rights reserved.
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

#include "../analysis_tool_interface.h"
#include "../analysis_tool.h"
#include "../common/options.h"
#include "../common/utils.h"
#include "cache_simulator.h"
#include "tlb_simulator.h"
/* XXX: we include these here for now but it's undecided whether they
 * should be separated and this should only include
 * cache-simulation-based tools.
 */
#include "../tools/histogram.h"
#include "../tools/reuse_distance.h"

analysis_tool_t *
drmemtrace_analysis_tool_create()
{
    if (op_simulator_type.get_value() == CPU_CACHE)
        return new cache_simulator_t;
    else if (op_simulator_type.get_value() == TLB)
        return new tlb_simulator_t;
    else if (op_simulator_type.get_value() == HISTOGRAM)
        return new histogram_t;
    else if (op_simulator_type.get_value() == REUSE_DIST)
        return new reuse_distance_t;
    else {
        ERRMSG("Usage error: unsupported analyzer type. "
               "Please choose " CPU_CACHE ", " TLB ", "
               HISTOGRAM ", or " REUSE_DIST ".\n");
        return NULL;
    }
}
