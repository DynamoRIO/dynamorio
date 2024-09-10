#ifndef _REUSE_PATTERN_H_
#define _REUSE_PATTERN_H_ 1

#include <stdint.h>

#include "dr_api.h"
#include "analysis_tool.h"
#include "memref.h"
#include "trace_entry.h"
#include "dr_mir_api.h"
#include "replayer.h"

namespace dynamorio {
namespace drmemtrace {

class reuse_pattern_t : public analysis_tool_t {
public:
    reuse_pattern_t();
    ~reuse_pattern_t();

    std::string
    initialize() override;

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

    struct dcontext_cleanup_last_t {
    public:
        ~dcontext_cleanup_last_t()
        {
            if (dcontext != nullptr)
                dr_standalone_exit();
        }
        void *dcontext = nullptr;
    };

    /* We make this the first field so that dr_standalone_exit() is called after
     * destroying the other fields which may use DR heap.
     */
    dcontext_cleanup_last_t dcontext_;

    uint64_t direct_reuse_count;
    uint64_t strided_reuse_count;
    uint64_t indirection_count; 

    _memref_instr_t prev_instr;
    _memref_instr_t curr_instr;

    shard_type_t shard_type_ = SHARD_BY_THREAD;
    memtrace_stream_t *serial_stream_ = nullptr;

    Replayer *replayer;
};

} // namespace drmemtrace
} // namespace dynamorio
#endif /* _REUSE_PATTERN_H_ */