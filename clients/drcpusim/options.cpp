/* **********************************************************
 * Copyright (c) 2015-2023 Google, Inc.  All rights reserved.
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

namespace dynamorio {
namespace drcpusim {

using ::dynamorio::droption::DROPTION_SCOPE_CLIENT;
using ::dynamorio::droption::droption_t;

#ifdef WINDOWS
#    define IF_WINDOWS_ELSE(x, y) x
#else
#    define IF_WINDOWS_ELSE(x, y) y
#endif

droption_t<std::string> op_cpu(
    DROPTION_SCOPE_CLIENT, "cpu", "Westmere",
    "CPU model to simulate.  Typical values:\n"
    "                                Pentium,PentiumMMX,PentiumPro,Klamath,Deschutes,\n"
    "                                Pentium3,Banias,Dothan,Prescott,Presler,Merom,\n"
    "                                Penryn,Westmere,Sandybridge,Ivybridge.",
    "Specifies the CPU model to simulate.  It can be one of the following names, which \n"
    "correspond to the given CPU family, model, and major ISA features (as well as\n"
    "numerous other minor features):\n"
    "<table>\n"
    "<tr><td><b>Parameter</b></td><td><b>Notes</b></td><td><b>Family</b></td>"
    "<td><b>Model</b></td><td><b>Major ISA Features</b></td></tr>\n"
    "<tr><td>Pentium   </td><td>&nbsp;</td><td>5</td><td>2</td>"
    "<td>&nbsp;</td></tr>\n"
    "<tr><td>PentiumMMX</td><td>&nbsp;</td><td>5</td><td>4</td>"
    "<td>MMX</td></tr>\n"
    "<tr><td>PentiumPro</td><td>&nbsp;</td><td>6</td><td>1</td>"
    "<td>&nbsp;</td></tr>\n"
    "<tr><td>Pentium2</td><td>alias for Klamath</td><td>6</td><td>3</td>"
    "<td>MMX</td></tr>\n"
    "<tr><td>Klamath</td><td>Pentium2</td><td>6</td><td>3</td>"
    "<td>MMX</td></tr>\n"
    "<tr><td>Deschutes</td><td>Pentium2</td><td>6</td><td>5</td>"
    "<td>MMX</td></tr>\n"
    "<tr><td>Pentium3</td><td>alias for Coppermine</td><td>6</td><td>7</td>"
    "<td>MMX, SSE</td></tr>\n"
    "<tr><td>Coppermine</td><td>Pentium3</td><td>6</td><td>7</td>"
    "<td>MMX, SSE</td></tr>\n"
    "<tr><td>Tualatin</td><td>Pentium3</td><td>6</td><td>7</td>"
    "<td>MMX, SSE</td></tr>\n"
    "<tr><td>PentiumM</td><td>alias for Banias</td><td>15</td><td>2</td>"
    "<td>MMX, SSE, SSE2</td></tr>\n"
    "<tr><td>Banias</td><td>PentiumM</td><td>15</td><td>2</td>"
    "<td>MMX, SSE, SSE2</td></tr>\n"
    "<tr><td>Dothan</td><td>PentiumM</td><td>15</td><td>2</td>"
    "<td>MMX, SSE, SSE2</td></tr>\n"
    "<tr><td>Willamette</td><td>early Pentium4</td><td>15</td><td>2</td>"
    "<td>MMX, SSE, SSE2</td></tr>\n"
    "<tr><td>Northwood</td><td>early Pentium4</td><td>15</td><td>2</td>"
    "<td>MMX, SSE, SSE2</td></tr>\n"
    "<tr><td>Pentium4</td><td>alias for Prescott</td><td>15</td><td>4</td>"
    "<td>MMX, SSE, SSE2, SSE3</td></tr>\n"
    "<tr><td>Prescott</td><td>Pentium4</td><td>15</td><td>4</td>"
    "<td>MMX, SSE, SSE2, SSE3</td></tr>\n"
    "<tr><td>Presler</td><td>Pentium4</td><td>15</td><td>4</td>"
    "<td>MMX, SSE, SSE2, SSE3</td></tr>\n"
    "<tr><td>Core2</td><td>alias for Merom</td><td>6</td><td>15</td>"
    "<td>MMX, SSE, SSE2, SSE3, SSSE3</td></tr>\n"
    "<tr><td>Merom</td><td>Core2</td><td>6</td><td>15</td>"
    "<td>MMX, SSE, SSE2, SSE3, SSSE3</td></tr>\n"
    "<tr><td>Penryn</td><td>Core2</td><td>6</td><td>23</td>"
    "<td>MMX, SSE, SSE2, SSE3, SSSE3, SSE4.1</td></tr>\n"
    "<tr><td>Nehalem</td><td>Core i7</td><td>6</td><td>26</td>"
    "<td>MMX, SSE, SSE2, SSE3, SSSE3, SSE4.1, SSE4.2</td></tr>\n"
    "<tr><td>Westmere</td><td>Core i7</td><td>6</td><td>44</td>"
    "<td>MMX, SSE, SSE2, SSE3, SSSE3, SSE4.1, SSE4.2</td></tr>\n"
    "<tr><td>Sandybridge</td><td>Core i7</td><td>6</td><td>42</td>"
    "<td>MMX, SSE, SSE2, SSE3, SSSE3, SSE4.1, SSE4.2, AVX</td></tr>\n"
    "<tr><td>Ivybridge</td><td>Core i7</td><td>6</td><td>58</td>"
    "<td>MMX, SSE, SSE2, SSE3, SSSE3, SSE4.1, SSE4.2, AVX, F16C</td></tr>\n"
    "</table>\n"
    "Some simplifications are made: for example, drcpusim assumes that all Prescott "
    "models support 64-bit, ignoring the early E-series models.  Furthermore, drcpusim "
    "focuses on cpuid features rather than the family, and ends up treating requests "
    "for slightly different cpu models that have insignificant cpuid feature differences "
    "as identical: for example, a request for Northwood will result in a Banias model.");

droption_t<bool> op_continue(
    DROPTION_SCOPE_CLIENT, "continue", false, "Continue (don't abort) on bad instr.",
    "By default, drcpusim aborts when it encounters an invalid instruction.  This option "
    "requests that the tool continue, simply printing each invalid instruction it "
    "encounters.  It may print the same instruction twice, depending on whether the "
    "underlying tool engine needs to re-translate that code again.");

droption_t<bool> op_fool_cpuid(
    DROPTION_SCOPE_CLIENT, "fool_cpuid", true, "Fake CPUID to match CPU model.",
    "When the application executes the CPUID instruction, when this option is enabled, "
    "drcpusim will supply CPUID results that match the CPU model being simulated.");

droption_t<bool> op_allow_prefetchw(
    DROPTION_SCOPE_CLIENT, "allow_prefetchw", true, "Consider PREFETCHW to be harmless.",
    "The PREFETCHW instruction is only fully supported by AMD processors, yet most Intel "
    "processors, while they do not officially support it, will turn it into a NOP. "
    "As it is commonly seen on Windows, by default drcpusim does not complain about it.");

droption_t<std::string> op_blocklist(
    // We support the legacy name as an alias for compatibility.
    // We explicitly specity the vector type to avoid ambiguity with iterators
    // when we have just 2 elements in the list.
    DROPTION_SCOPE_CLIENT, std::vector<std::string>({ "blocklist", "blacklist" }),
    IF_WINDOWS_ELSE("ntdll.dll", ""), ":-separated list of libs to ignore.",
    "The blocklist is a :-separated list of library names for which violations "
    "should not be reported.");

droption_t<bool> op_ignore_all_libs(
    DROPTION_SCOPE_CLIENT, "ignore_all_libs", false,
    "Ignore all libraries: only check app itself.",
    "Violations in libraries are ignored: only violations in the application executable "
    "itself are reported.");

droption_t<unsigned int> op_verbose(DROPTION_SCOPE_CLIENT, "verbose", 0, 0, 64,
                                    "Verbosity level",
                                    "Verbosity level for notifications.");

} // namespace drcpusim
} // namespace dynamorio
