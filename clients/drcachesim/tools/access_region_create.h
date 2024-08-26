#ifndef _ACCESS_REGION_CREATE_H_
#define _ACCESS_REGION_CREATE_H_ 1

#include "analysis_tool.h"

namespace dynamorio {
namespace drmemtrace {

analysis_tool_t *
access_region_tool_create(uint64_t stack_start, uint64_t stack_end, uint64_t heap_start, uint64_t heap_end);

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _ACCESS_REGION_CREATE_H_ */