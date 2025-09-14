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
#include <istream>

namespace dynamorio {
namespace drmemtrace {

// Configuration node. Can be scalar value (parameter) or map (nested parameters)
struct config_node_t {
    enum { UNKNOWN, SCALAR, MAP } type; // Type: scalar value or nested parameters

    // Line and column where the parameter name is defined
    int param_line;
    int param_column;
    // Line and column where the value is defined
    int val_line;
    int val_column;

    std::string scalar; // Value in string representation.
                        // Will be converted to simple type later
    // Nested parameters
    typedef std::map<std::string, config_node_t> map_t;
    map_t children;
};
typedef config_node_t::map_t config_t;

class config_tokenizer_t {
public:
    config_tokenizer_t(std::istream *is)
        : is_(is)
        , line_(-1)
        , column_(-1)
    {
    }

    // TODO: Treat braces as spaces:
    //      name{n0 v0 n1 v1}
    bool
    next(std::string &token)
    {
        std::string tmp;
        do {
            if (!(*is_ >> tmp)) {
                ERRMSG("Unable to extract token from line %d column %d\n", line_,
                       column_);
                return false;
            }
            if (tmp == "//") {
                // A comment. Skip it till the end of the line
                if (!std::getline(*is_, tmp)) {
                    ERRMSG("Comment expected but not found at line %d column %d\n", line_,
                           column_);
                    return false;
                }
                // Read next token
                continue;
            }

            if (!is_->eof()) {
                *is_ >> std::ws;
                if (!(*is_)) {
                    ERRMSG("Unable to read from the configuration\n");
                    return false;
                }
            }
        } while (0);
        token = tmp;
        return true;
    }

    // TODO: Implement
    bool
    eof() const
    {
        return false;
    }

    // TODO: Implement
    int
    getline() const
    {
        return line_;
    }

    // TODO: Implement
    int
    getcolumn() const
    {
        return column_;
    }

private:
    std::istream *is_;
    std::string buffer_;
    int line_;
    int column_;
};

bool
read_param_map_impl(config_tokenizer_t *, config_t *);

// Read configuration parameters from stream
// Supported scalar parameters:
//      name0 val0 name1 val1
// And parameter maps:
//      name0 val0 name2 { name3 val3 name4 val4 }
// Nested maps supported:
//      name0 val0 name5 { name6 val6 name7 { name8 val8 name9 val9 } }
// Tokens separated with spaces
bool
read_param_map(std::istream *is, config_t *params)
{
    config_tokenizer_t tokenizer(is);
    return read_param_map_impl(&tokenizer, params);
}

bool
read_param_map_impl(config_tokenizer_t *tokenizer, config_t *params)
{

    while (!tokenizer->eof()) {
        std::string token;
        if (!tokenizer->next(token)) {
            if (tokenizer->eof()) {
                // ?
            } else {
                ERRMSG("Failed to read the configuration file\n");
                return false;
            }
        }
        int p_line = tokenizer->getline();
        int p_column = tokenizer->getcolumn();

        if (token == "{") {
            ERRMSG("Braces without parameter name not allowed at line %d column %d\n",
                   p_line, p_column);
            return false;
        } else if (token == "}") {
            // Parameter map ended. Just end processing
            return true;
        } else {
            std::string name = token;
            if (!tokenizer->next(token)) {
                ERRMSG("Error reading '%s' from line %d column %d\n", name.c_str(),
                       p_line, p_column);
                return false;
            }
            int v_line = tokenizer->getline();
            int v_column = tokenizer->getcolumn();
            if (token == "{") {
                // This is nested parameter map
                config_node_t val { config_node_t::MAP, p_line, p_column, v_line,
                                    v_column };
                if (!read_param_map_impl(tokenizer, &(val.children))) {
                    ERRMSG("Error reading structure '%s' from the configuration file\n",
                           name.c_str());
                    return false;
                }
                params->emplace(name, val);
            } else {
                // Parameter value
                config_node_t val { config_node_t::SCALAR, p_line, p_column, v_line,
                                    v_column };
                val.scalar = token;
                params->emplace(name, val);
            }
        }
    }
    return true;
}

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
