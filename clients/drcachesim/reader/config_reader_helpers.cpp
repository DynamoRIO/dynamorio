#include "reader/config_reader_helpers.h"

namespace dynamorio {
namespace drmemtrace {

bool
read_param_map_impl(config_tokenizer_t *tokenizer, config_t *params, int nest_level = 0)
{
    while (!tokenizer->eof()) {
        std::string token;
        if (!tokenizer->next(token)) {
            if (tokenizer->eof()) {
                if (nest_level > 1) {
                    // Multiple "}" missed
                    ERRMSG("%d braces '}' missed at line %d\n", nest_level,
                           tokenizer->getline());
                    return false;
                } else if (nest_level == 1) {
                    // Single "}" missed
                    ERRMSG("Brace '}' missed at line %d\n", tokenizer->getline());
                    return false;
                } else {
                    // nest_level == 0: EOF is the correct behavior
                    return true;
                }
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
                if (!read_param_map_impl(tokenizer, &(val.children), nest_level + 1)) {
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

bool
read_param_map(std::istream *is, config_t *params)
{
    config_tokenizer_t tokenizer(is);
    return read_param_map_impl(&tokenizer, params);
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
            // Check if the stream ss_ empty again: the line next_line can be empty.
            continue;
        }
        column_ = int(ss_.tellg()) + 1;

        std::string tmp;
        if (!(ss_ >> tmp)) {
            ERRMSG("Unable to extract token from line %d column %d\n", line_, column_);
            return false;
        }
        if (tmp == "//") {
            // A comment. Skip it till the end of the line
            if (!std::getline(ss_, tmp)) {
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
