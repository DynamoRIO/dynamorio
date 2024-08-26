#ifndef _REUSE_PATTERN_CREATE_H_
#define _REUSE_PATTERN_CREATE_H_ 1

#include "analysis_tool.h"

namespace dynamorio {
namespace drmemtrace {

analysis_tool_t *
reuse_pattern_tool_create();

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _REUSE_PATTERN_CREATE_H_ */