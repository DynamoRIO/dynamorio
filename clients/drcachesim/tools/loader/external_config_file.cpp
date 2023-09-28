#include "external_config_file.h"

#include <fstream>

namespace dynamorio {
namespace drmemtrace {

external_tool_config_file_t::external_tool_config_file_t(const std::string &root,
                                                         const std::string &filename)
{
    std::ifstream stream(filename);
    if (stream.good()) {
        std::string line;
        while (std::getline(stream, line)) {
            auto pos = line.find("TOOL_ID=");
            if (pos != std::string::npos) {
                id_ = line.substr(pos + 8);
            }

            pos = line.find("CREATOR_BIN=");
            if (pos != std::string::npos) {
                auto creator_lib_path = line.substr(pos + 12);
                creator_path_.append(root);
                creator_path_.append("/");
                creator_path_.append(creator_lib_path);
            }
        }

        valid_ = (!id_.empty() && !creator_path_.empty());
    }
}

} // namespace drmemtrace
} // namespace dynamorio
