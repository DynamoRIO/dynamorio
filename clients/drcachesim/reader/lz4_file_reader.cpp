
#include "lz4_file_reader.h"

namespace {

trace_entry_t *
read_next_entry_common(lz4_reader_t *reader, bool *eof)
{
    if (reader->cur_buf >= reader->max_buf) {
        int len = reader->file
                      ->read(reinterpret_cast<char *>(&reader->buf), sizeof(reader->buf))
                      .gcount();
        if (len < static_cast<int>(sizeof(trace_entry_t)) ||
            len % static_cast<int>(sizeof(trace_entry_t)) != 0) {
            *eof = (len >= 0);
            return nullptr;
        }
        reader->cur_buf = reader->buf;
        reader->max_buf = reader->buf + (len / sizeof(trace_entry_t));
    }
    trace_entry_t *res = reader->cur_buf;
    ++reader->cur_buf;
    return res;
}

} // namespace

/**************************************************
 * lz4_reader_t specializations for file_reader_t.
 */

/* clang-format off */ /* (make vera++ newline-after-type check happy) */
template <>
/* clang-format on */
file_reader_t<lz4_reader_t>::file_reader_t()
{
    input_file_.file = nullptr;
}

/* clang-format off */ /* (make vera++ newline-after-type check happy) */
template <>
/* clang-format on */
file_reader_t<lz4_reader_t>::~file_reader_t<lz4_reader_t>()
{
    if (input_file_.file != nullptr) {
        delete input_file_.file;
        input_file_.file = nullptr;
    }
}

template <>
bool
file_reader_t<lz4_reader_t>::open_single_file(const std::string &path)
{
    auto file = new lz4_istream_t(path);
    VPRINT(this, 1, "Opened input file %s\n", path.c_str());
    input_file_ = lz4_reader_t(file);
    return true;
}

template <>
trace_entry_t *
file_reader_t<lz4_reader_t>::read_next_entry()
{
    trace_entry_t *entry = read_queued_entry();
    if (entry != nullptr)
        return entry;
    entry = read_next_entry_common(&input_file_, &at_eof_);
    if (entry == nullptr)
        return entry;
    VPRINT(this, 4, "Read from file: type=%s (%d), size=%d, addr=%zu\n",
           trace_type_names[entry->type], entry->type, entry->size, entry->addr);
    entry_copy_ = *entry;
    return &entry_copy_;
}
