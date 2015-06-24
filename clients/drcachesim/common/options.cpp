/* **********************************************************
 * Copyright (c) 2015 Google, Inc.  All rights reserved.
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

/* shared options for both the frontend and the client */

#include <string>
#include "droption.h"
#include "options.h"

droption_t<std::string> op_ipc_name
(DROPTION_SCOPE_ALL, "ipc_name", "", "Base name of named pipe",
 "Specifies the base name of the named pipe used to communicate between the target "
 "application processes and the cache simulator.");

droption_t<unsigned int> op_verbose
(DROPTION_SCOPE_ALL, "verbose", 0, 0, 64, "Verbosity level",
 "Verbosity level for notifications.");

droption_t<std::string> op_dr_root
(DROPTION_SCOPE_FRONTEND, "dr", "", "Path to DynamoRIO root directory",
 "Specifies the path of the DynamoRIO root directory.");

droption_t<bool> op_dr_debug
(DROPTION_SCOPE_FRONTEND, "dr_debug", false, "Use DynamoRIO debug build",
 "Requests use of the debug build of DynamoRIO rather than the release build.");

droption_t<std::string> op_dr_ops
(DROPTION_SCOPE_FRONTEND, "dr_ops", "", "Options to pass to DynamoRIO",
 "Specifies the options to pass to DynamoRIO.");

droption_t<std::string> op_tracer
(DROPTION_SCOPE_FRONTEND, "tracer", "", "Path to the tracer",
 "The full path to the tracer library.");

droption_t<std::string> op_tracer_ops
(DROPTION_SCOPE_FRONTEND, "tracer_ops", DROPTION_FLAG_SWEEP | DROPTION_FLAG_ACCUMULATE,
 "", "(For internal use: sweeps up tracer options)",
 "This is an internal option that sweeps up other options to pass to the tracer.");
