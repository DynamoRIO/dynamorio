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

/* drcpusim options, separated out for use in docs generation */

#include <string>
#include "droption.h"
#include "options.h"

droption_t<std::string> op_cpu
(DROPTION_SCOPE_CLIENT, "cpu", "Pentium", "CPU model to simulate",
 "Specifies the CPU model to simulate.  It can be one of the following values:\n"
 "<ul>\n"
 "<li><b>Pentium</b></li>\n"
 "<li><b>PentiumMMX</b></li>\n"
 "<li><b>PentiumPro</b></li>\n"
 "<li><b>Pentium2</b> (Klamath)</li>\n"
 "<li><b>Klamath</b> (Pentium2</li>\n"
 "<li><b>Deschutes</b> (Pentium2)</li>\n"
 "<li><b>Pentium3</b> (Coppermine)</li>\n"
 "<li><b>Coppermine</b> (Pentium3)</li>\n"
 "<li><b>Tualatin</b> (Pentium3)</li>\n"
 "</ul>");

droption_t<unsigned int> op_verbose
(DROPTION_SCOPE_CLIENT, "verbose", 0, 0, 64, "Verbosity level",
 "Verbosity level for notifications.");
