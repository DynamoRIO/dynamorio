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

#include "reader/config_reader_helpers.h"

#include "utils.h"

#ifdef DEBUG
#    define DBGMSG ERRMSG
#else
#    define DBGMSG(...) /* nothing */
#endif

namespace dynamorio {
namespace drmemtrace {

bool
read_param_map_impl(config_tokenizer_t *tokenizer, config_t *params, int nest_level)
{
    while (!tokenizer->eof()) {
        std::string token;
        if (!tokenizer->next(token)) {
            if (tokenizer->eof()) {
                DBGMSG("\t\t\t\tEOF\n");
                if (nest_level > 0) {
                    // } missed
                    ERRMSG("Error: %d braces '}' missed at line %d\n", nest_level,
                           tokenizer->getline());
                } else {
                    // nest_level == 0: EOF is the correct behavior
                    return true;
                }
            } else {
                ERRMSG("Failed to read configuration\n");
                return false;
            }
        }
        int p_line = tokenizer->getline();
        int p_column = tokenizer->getcolumn();

        if (token == "{") {
            ERRMSG(
                "Error: Braces without parameter name not allowed at line %d column %d\n",
                p_line, p_column);
            return false;
        } else if (token == "}") {
            DBGMSG("\t\t\t\tEnd nested map\n");
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
            DBGMSG("\t\t\t\tKeyValue: '%s'='%s' position: (%d,%d) (%d,%d)\n",
                   name.c_str(), token.c_str(), p_line, p_column, v_line, v_column);
            if (token == "{") {
                DBGMSG("\t\t\t\tStart nested map\n");
                // This is nested parameter map
                config_param_node_t val { config_param_node_t::MAP, p_line, p_column,
                                          v_line, v_column };
                if (!read_param_map_impl(tokenizer, &(val.children), nest_level + 1)) {
                    ERRMSG("Error reading structure '%s' at line %d column %d\n",
                           name.c_str(), v_line, v_column);
                    return false;
                }
                if (val.children.size() == 0) {
                    // Empty map not allowed
                    ERRMSG("Error: empty structure '%s' at line %d column %d\n",
                           name.c_str(), v_line, v_column);
                    return false;
                }
                params->emplace(name, val);
            } else {
                // Parameter value
                config_param_node_t val { config_param_node_t::SCALAR, p_line, p_column,
                                          v_line, v_column };
                val.scalar = token;
                params->emplace(name, val);
            }
        }
    }
    return true;
}

config_tokenizer_t::config_tokenizer_t(std::istream *is)
    : is_(is)
    , line_(0)
    , column_(0)
    , eof_(false)
{
}

bool
config_tokenizer_t::next(std::string &token)
{
    do {
        std::string tmp;
        // Remove leading spaces
        ss_ >> std::ws;
        if (ss_.eof()) {
            // Stream ss_ is empty. Fetch next line
            std::string next_line;
            if (!std::getline(*is_, next_line)) {
                if (is_->eof()) {
                    eof_ = true;
                    return false;
                }
                ERRMSG("Failed to read the configuration line %d.\n", line_ + 1);
                return false;
            }
            // Add spaces before and after brace "{"
            size_t index = 0;
            while (true) {
                index = next_line.find("{", index);
                if (index == std::string::npos)
                    break;
                next_line.replace(index, 1, " { ");
                index += 3;
            }
            // Add spaces before and after brace "}"
            index = 0;
            while (true) {
                index = next_line.find("}", index);
                if (index == std::string::npos)
                    break;
                next_line.replace(index, 1, " } ");
                index += 3;
            }

            ss_.str(next_line);
            ss_.clear();
            ++line_;
            DBGMSG("\tLine %d: '%s'\n", line_, ss_.str().c_str());
            // Check if the stream ss_ empty again: the line next_line can be empty.
            continue;
        }
        column_ = int(ss_.tellg()) + 1;
        if (!(ss_ >> tmp)) {
            ERRMSG("Unable to extract token from line %d column %d\n", line_, column_);
            return false;
        }
        DBGMSG("\t\tToken: '%s'\n", tmp.c_str());
        if (tmp == "//") {
            // A comment. Skip it till the end of the line
            if (!std::getline(ss_, tmp) && !ss_.eof()) {
                ERRMSG("Comment expected but not found at line %d column %d\n", line_,
                       column_);
                return false;
            }
            // Read next token
            continue;
        }
        token = tmp;
        break;
    } while (1);
    return true;
}

} // namespace drmemtrace
} // namespace dynamorio
