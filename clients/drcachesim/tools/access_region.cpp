#include <iostream>
#include "access_region.h"

#include "analysis_tool.h"
#include "memref.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

analysis_tool_t *
access_region_tool_create(uint64_t stack_start, uint64_t stack_end, uint64_t heap_start,
                          uint64_t heap_end)
{
    return new access_region_t(stack_start, stack_end, heap_start, heap_end);
}

access_region_t::access_region_t(uint64_t stack_start, uint64_t stack_end,
                                 uint64_t heap_start, uint64_t heap_end)
    : stack_start(stack_start)
    , stack_end(stack_end)
    , heap_start(heap_start)
    , heap_end(heap_end)
    , stack_accesses(0)
    , heap_accesses(0)
    , between_accesses(0)
    , above_stack_accesses(0)
    , below_heap_accesses(0)
{
    // Empty.
    // ASSERT(stack_start > stack_end);
    // ASSERT(heap_start < heap_end);
}

access_region_t::~access_region_t()
{
    // Empty.
}

std::string
access_region_t::initialize_stream(memtrace_stream_t *serial_stream)
{
    serial_stream_ = serial_stream;
    return "";
}

bool
access_region_t::parallel_shard_supported()
{
    return false;
}

void *
access_region_t::parallel_shard_init_stream(int shard_index, void *worker_data,
                                            memtrace_stream_t *shard_stream)
{
    return shard_stream;
}

bool
access_region_t::parallel_shard_exit(void *shard_data)
{
    return true;
}

std::string
access_region_t::parallel_shard_error(void *shard_data)
{
    return error_string_;
}

bool
access_region_t::process_memref(const memref_t &memref)
{
    return parallel_shard_memref(serial_stream_, memref);
}

bool
access_region_t::parallel_shard_memref(void *shard_data, const memref_t &memref)
{
    // memtrace_stream_t *memstream = reinterpret_cast<memtrace_stream_t *>(shard_data);
    if (type_is_data(memref.data.type)) {
        if (memref.data.addr <= stack_start && memref.data.addr > stack_end) {
            stack_accesses++;
        } else if (memref.data.addr >= heap_start && memref.data.addr < heap_end) {
            heap_accesses++;
        } else if (memref.data.addr > stack_start) {
            above_stack_accesses++;
        } else if (memref.data.addr < heap_end) {
            below_heap_accesses++;
        } else {
            between_accesses++;
        }
    }
    // else, ignore
    return true;
}

bool
access_region_t::print_results()
{
    std::cerr << "Access region tool internal stats:\n";

    std::cerr << "Stack start: " << stack_start << std::endl;
    std::cerr << "Stack end: " << stack_end << std::endl;
    std::cerr << "Heap start: " << heap_start << std::endl;
    std::cerr << "Heap end: " << heap_end << std::endl;

    std::cerr << "Accesses by region:\n";
    std::cerr << "Stack accesses: " << stack_accesses << std::endl;
    std::cerr << "Heap accesses: " << heap_accesses << std::endl;
    std::cerr << "Between accesses: " << between_accesses << std::endl;
    std::cerr << "Above stack accesses: " << above_stack_accesses << std::endl;
    std::cerr << "Below heap accesses: " << below_heap_accesses << std::endl;
    return true;
}

} // namespace drmemtrace
} // namespace dynamorio