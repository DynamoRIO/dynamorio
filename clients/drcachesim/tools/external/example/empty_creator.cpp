
#include "../common/options.h"
#include "analyzer.h"
#include "empty_create.h"

using ::dynamorio::drmemtrace::analysis_tool_t;
using ::dynamorio::drmemtrace::op_alt_module_dir;
using ::dynamorio::drmemtrace::op_module_file;
using ::dynamorio::drmemtrace::op_verbose;

#ifdef WINDOWS
#    define EXPORT __declspec(dllexport)
#else /* UNIX */
#    define EXPORT __attribute__((visibility("default")))
#endif

extern "C" EXPORT const char *
get_id()
{
    return "empty";
}

extern "C" EXPORT analysis_tool_t *
analysis_tool_create()
{
    return empty_tool_create(op_module_file.get_value(), op_verbose.get_value(),
                             op_alt_module_dir.get_value());
}
