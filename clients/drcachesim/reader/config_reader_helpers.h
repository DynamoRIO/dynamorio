/* **********************************************************
 * Copyright (c) 2018-2026 Google, LLC  All rights reserved.
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
#include <istream>
#include <sstream>

namespace dynamorio {
namespace drmemtrace {

// Configuration parameter node.
// Can be scalar value (parameter) or map (nested parameters).
struct config_param_node_t {
    // Type: scalar value or nested parameters.
    enum { UNKNOWN, SCALAR, MAP } type;

    // Line and column where the parameter name is defined.
    int name_line;
    int name_column;
    // Line and column where the value is defined.
    int val_line;
    int val_column;

    // Value in string representation. Will be converted to simple type later.
    std::string value;
    // Nested parameters.
    typedef std::map<std::string, config_param_node_t> config_param_map_t;
    config_param_map_t children;
};
typedef config_param_node_t::config_param_map_t config_t;

class config_tokenizer_t {
public:
    config_tokenizer_t(std::istream *is);

    // Read the next token from the stream.
    // Return true if the token successfully read, false otherwise.
    // In case of EOF the function returns false and eof_ flag set.
    bool
    next(std::string &token);

    inline bool
    eof() const
    {
        return eof_;
    }

    // Returns current line number.
    inline int
    getline() const
    {
        return line_;
    }

    // Returns current column number in the line.
    inline int
    getcolumn() const
    {
        return column_;
    }

private:
    std::istream *is_;
    std::stringstream ss_;
    int line_;
    int column_;
    bool eof_;
};

bool
read_param_map_impl(config_tokenizer_t *, config_t *, int nest_level = 0);

// Read configuration parameters from stream.
// Supported scalar parameters:
//      name0 val0 name1 val1
// And parameter maps:
//      name0 val0 name2 { name3 val3 name4 val4 }
// Nested maps supported:
//      name0 val0 name5 { name6 val6 name7 { name8 val8 name9 val9 } }
// Tokens separated with spaces.
inline bool
read_param_map(std::istream *is, config_t *params)
{
    config_tokenizer_t tokenizer(is);
    return read_param_map_impl(&tokenizer, params);
}

#ifndef WINDOWS
// Returns real name for the type.
template <typename T>
const char *
get_type_name()
{
    int status = 0;
    const std::type_info &ti = typeid(T);
    // De-mangle.
    char *type_name = abi::__cxa_demangle(ti.name(), NULL, NULL, &status);
    if (type_name) {
        return type_name;
    } else {
        // Fallback: use mangled name.
        return ti.name();
    }
}
#else
// Returns real name for the type.
template <typename T>
const char *
get_type_name()
{
    const std::type_info &ti = typeid(T);
    // type_info in MSVC contains full type name
    return ti.name();
}
#endif

// Parse the value and return whether parse succeeded.
template <typename T>
bool
parse_value(const std::string &val, T *dst)
{
    if (std::is_unsigned<T>::value && val[0] == '-') {
        // Unsigned value must not start with "minus" sign.
        return false;
    }
    T res;
    std::stringstream ss { val };
    ss >> res;
    if (ss.eof()) {
        // EOF means the value has been extracted correctly.
        *dst = res;
        return true;
    }
    return false;
}

// Specialization for boolean: handle "True" and "False".
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

// Specialization for std::string: just return copy of the input string.
template <>
inline bool
parse_value(const std::string &val, std::string *dst)
{
    *dst = val;
    return true;
}

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _CONFIG_READER_HELPERS_H_ */
