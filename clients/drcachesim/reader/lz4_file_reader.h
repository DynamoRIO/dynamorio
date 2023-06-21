/* lz4_file_reader: reads compressed files containing memory traces. */

#ifndef _LZ4_FILE_READER_H_
#define _LZ4_FILE_READER_H_ 1

#include "common/lz4_istream.h"
#include "file_reader.h"
#include "record_file_reader.h"

struct lz4_reader_t {
    lz4_reader_t()
        : file(nullptr) {};
    explicit lz4_reader_t(std::istream *file)
        : file(file)
    {
    }
    std::istream *file;
    trace_entry_t buf[4096];
    trace_entry_t *cur_buf = buf;
    trace_entry_t *max_buf = buf;
};

typedef file_reader_t<lz4_reader_t> lz4_file_reader_t;

#endif /* _LZ4_FILE_READER_H_ */
