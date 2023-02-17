#include <string>
#include <vector>
#include <iostream>
#include "droption.h"
#include "dr_frontend.h"
#include "drmemtrace/analysis_tool.h"
#include "drmemtrace/analyzer.h"

#define FATAL_ERROR(msg, ...)                               \
    do {                                                    \
        fprintf(stderr, "ERROR: " msg "\n", ##__VA_ARGS__); \
        fflush(stderr);                                     \
        exit(1);                                            \
    } while (0)

static droption_t<std::string>
    op_trace(DROPTION_SCOPE_FRONTEND, "trace", "", "[Required] Trace input file",
             "Specifies the file containing the trace to be analyzed.");

class analyzer_example_t : public analysis_tool_t {
public:
    analyzer_example_t(const std::string &module_file_path)
    {
    }
    bool
    process_memref(const memref_t &memref) override
    {
        if (type_is_instr(memref.instr.type)) {
            num_instrs++;
        }
        return true;
    }
    bool
    print_results() override
    {
        std::cerr << "Found " << num_instrs << " instructions\n";
        return true;
    }

protected:
    int num_instrs = 0;
};

int
_tmain(int argc, const TCHAR *targv[])
{
    // Convert to UTF-8 if necessary
    char **argv;
    drfront_status_t sc = drfront_convert_args(targv, &argv, argc);
    if (sc != DRFRONT_SUCCESS)
        FATAL_ERROR("Failed to process args: %d", sc);

    std::string parse_err;
    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_FRONTEND, argc, (const char **)argv,
                                       &parse_err, NULL) ||
        op_trace.get_value().empty()) {
        if (parse_err.find("help") != std::string::npos) {
            FATAL_ERROR("Usage error: %s\nUsage:\n%s", parse_err.c_str(),
                        droption_parser_t::usage_long(DROPTION_SCOPE_ALL).c_str());
        } else {
            FATAL_ERROR("Usage error: %s\nUsage:\n%s", parse_err.c_str(),
                        droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
        }
    }

    analysis_tool_t *d = new analyzer_example_t("");
    std::vector<analysis_tool_t *> tools;
    tools.push_back(d);
    analyzer_t analyzer(op_trace.get_value(), &tools[0], (int)tools.size());
    if (!analyzer)
        FATAL_ERROR("failed to initialize analyzer");
    if (!analyzer.run())
        FATAL_ERROR("failed to run analyzer");
    analyzer.print_stats();
    delete d;

    sc = drfront_cleanup_args(argv, argc);
    if (sc != DRFRONT_SUCCESS)
        FATAL_ERROR("drfront_cleanup_args failed: %d\n", sc);

    return 0;
}
