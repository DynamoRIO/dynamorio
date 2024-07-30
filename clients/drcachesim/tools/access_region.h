#ifndef _ACCESS_REGION_H_
#define _ACCESS_REGION_H_ 1

#include <stdint.h>

#include "analysis_tool.h"
#include "memref.h"

namespace dynamorio {
namespace drmemtrace {

class access_region_t : public analysis_tool_t {
public:
    access_region_t(uint64_t stack_start, uint64_t stack_end, uint64_t heap_start, uint64_t heap_end);
    ~access_region_t();

    std::string
    initialize_stream(memtrace_stream_t *serial_stream) override;
    
    bool
    parallel_shard_supported() override;
    void *
    parallel_shard_init_stream(int shard_index, void *worker_data, memtrace_stream_t *shard_stream) override;
    bool 
    parallel_shard_exit(void *shard_data) override;
    std::string 
    parallel_shard_error(void *shard_data) override;

    bool
    process_memref(const memref_t &memref) override;
    bool
    parallel_shard_memref(void *shard_data, const memref_t &memref) override;

    bool
    print_results() override;


protected:
    uint64_t stack_start;
    uint64_t stack_end;
    uint64_t heap_start;
    uint64_t heap_end;

    uint64_t stack_accesses;
    uint64_t heap_accesses;
    uint64_t between_accesses;
    uint64_t above_stack_accesses;
    uint64_t below_heap_accesses;

    shard_type_t shard_type_ = SHARD_BY_THREAD;
    memtrace_stream_t *serial_stream_ = nullptr;
};

} // namespace drmemtrace
} // namespace dynamorio
#endif /* _ACCESS_REGION_H_ */