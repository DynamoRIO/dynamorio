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

#include "external_tool_creator.h"

namespace dynamorio {
namespace drmemtrace {

external_tool_creator_t::external_tool_creator_t(const std::string &filename)
    : dynamic_lib_t(filename)
    , get_tool_name_(get_export<get_tool_name_t>("get_tool_name"))
    , create_tool_(get_export<create_tool_t>("analysis_tool_create"))
{
    if (error_string_.empty() && (get_tool_name_ == nullptr || create_tool_ == nullptr)) {
        error_string_ = "Symbol can not be exported";
    }
}

std::string
external_tool_creator_t::get_tool_name()
{
    std::string res;
    if (get_tool_name_ != nullptr) {
        res = get_tool_name_();
    }

    return res;
}

analysis_tool_t *
external_tool_creator_t::create_tool()
{
    analysis_tool_t *res = nullptr;
    if (create_tool_ != nullptr) {
        res = create_tool_();
    }

    return res;
}

} // namespace drmemtrace
} // namespace dynamorio
