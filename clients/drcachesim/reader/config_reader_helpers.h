/* **********************************************************
 * Copyright (c) 2018-2023 Google, LLC  All rights reserved.
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
 * * Neither the name of Google, LLC nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, LLC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef _CONFIG_READER_HELPERS_H_
#define _CONFIG_READER_HELPERS_H_ 1

#ifndef WINDOWS
#    include "cxxabi.h"
#endif
#include <map>
#include <string>

namespace dynamorio {
namespace drmemtrace {

// Configuration node. Can be scalar value (parameter) or map (nested parameters)
struct config_node_t {
    enum { UNKNOWN, SCALAR, MAP } type; // Type: scalar value or nested parameters

    std::string scalar; // Value in string representation.
                        // Will be converted to simple type later
    // Nested parameters
    typedef std::map<std::string, config_node_t> map_t;
    map_t children;
};
typedef config_node_t::map_t config_t;

bool
read_param_map(std::istream *fin, config_t *params);

#ifndef WINDOWS
// Returns real name for the type
template <typename T>
const char *
get_type_name()
{
    int status = 0;
    const std::type_info &ti = typeid(T);
    // De-mangle
    char *type_name = abi::__cxa_demangle(ti.name(), NULL, NULL, &status);
    if (type_name) {
        return type_name;
    } else {
        // Fallback: use mangled name
        return ti.name();
    }
}
#else
// Returns real name for the type
template <typename T>
const char *
get_type_name()
{
    const std::type_info &ti = typeid(T);
    // type_info in MSVC contains full type name
    return ti.name();
}
#endif

// Parse the value and return whether parse succeeded
template <typename T>
bool
parse_value(const std::string &val, T *dst)
{
    if (std::is_unsigned<T>::value && val[0] == '-') {
        // Unsigned value must not start with "minus" sign
        return false;
    }
    T res;
    std::stringstream ss { val };
    ss >> res;
    if (ss.eof()) {
        // EOF means the value has been extracted correctly
        *dst = res;
        return true;
    }
    return false;
}

// Specialization for boolean: handle "True" and "False"
template <>
inline bool
parse_value(const std::string &val, bool *dst)
{
    if (val == "true" || val == "True" || val == "TRUE") {
        *dst = true;
        return true;
    } else if (val == "false" || val == "False" || val == "FALSE") {
        *dst = false;
        return true;
    }
    return false;
}

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _CONFIG_READER_HELPERS_H_ */
