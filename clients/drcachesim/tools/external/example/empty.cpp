
#include "dr_api.h"
#include "empty.h"
#include <iostream>

const std::string empty_t::TOOL_NAME = "Empty tool";

analysis_tool_t *
empty_tool_create(const std::string &module_file_path, unsigned int verbose,
                  const std::string &alt_module_dir)
{
    return new empty_t(module_file_path, verbose, alt_module_dir);
}

empty_t::empty_t(const std::string &module_file_path, unsigned int verbose,
                 const std::string &alt_module_dir)
{
    std::cout << "Empty tool created" << std::endl;
}

std::string
empty_t::initialize()
{
    return std::string("");
}

empty_t::~empty_t()
{
}

bool
empty_t::parallel_shard_supported()
{
    return true;
}

void *
empty_t::parallel_worker_init(int worker_index)
{

    return NULL;
}

std::string
empty_t::parallel_worker_exit(void *worker_data)
{
    return std::string("");
}

void *
empty_t::parallel_shard_init(int shard_index, void *worker_data)
{
    return NULL;
}

bool
empty_t::parallel_shard_exit(void *shard_data)
{
    return true;
}

bool
empty_t::parallel_shard_memref(void *shard_data, const memref_t &memref)
{
    return true;
}

std::string
empty_t::parallel_shard_error(void *shard_data)
{
    return std::string("");
}

bool
empty_t::process_memref(const memref_t &memref)
{
    return true;
}

bool
empty_t::print_results()
{
    return true;
}
