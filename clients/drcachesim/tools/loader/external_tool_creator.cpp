#include "external_tool_creator.h"

namespace dynamorio {
namespace drmemtrace {

external_tool_creator::external_tool_creator(const char *filename)
    : dynamic_lib(filename)
    , get_id_(load<get_tool_id_t>("get_id"))
    , create_tool_(load<create_tool_t>("analysis_tool_create"))
{
    if (create_tool_ == NULL)
        throw dynamic_lib_error(error());
}

} // namespace drmemtrace
} // namespace dynamorio
