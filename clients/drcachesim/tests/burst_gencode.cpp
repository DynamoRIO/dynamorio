/* **********************************************************
 * Copyright (c) 2016-2023 Google, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

// This application links in drmemtrace_static and acquires a trace during
// a "burst" of execution that includes generated code.

// This is set globally in CMake for other tests so easier to undef here.
#undef DR_REG_ENUM_COMPATIBILITY

#include "configure.h"
#include "dr_api.h"
#include "drmemtrace/drmemtrace.h"
#include "drmemtrace/raw2trace.h"
#include "raw2trace_directory.h"
#include "scheduler.h"
#include <assert.h>
#ifdef LINUX
#    include <signal.h>
#endif
#include <fstream>
#include <iostream>
#include <string>
#undef ALIGN_FORWARD // Conflicts with drcachesim utils.h.
#include "tools.h"   // Included after system headers to avoid printf warning.

namespace dynamorio {
namespace drmemtrace {

/***************************************************************************
 * Code generation.
 */

#ifdef LINUX
#    ifdef X86
static constexpr int UD2_LENGTH = 2;
#    else
static constexpr int UD2_LENGTH = 4;
#    endif
void
handle_signal(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (signal != SIGILL) {
        std::cerr << "Unexpected signal " << signal << "\n";
        return;
    }
    sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
    sc->SC_XIP += UD2_LENGTH;
    return;
}
#endif

class code_generator_t {
public:
    explicit code_generator_t(bool verbose = false)
        : verbose_(verbose)
    {
        generate_code();
    }
    ~code_generator_t()
    {
        free_mem(reinterpret_cast<char *>(map_), map_size_);
    }
    void
    execute_generated_code() const
    {
        reinterpret_cast<void (*)()>(map_)();
    }

    static constexpr int kGencodeMagic1 = 0x742;
    static constexpr int kGencodeMagic2 = 0x427;

    void
    modify_generated_code()
    {
        protect_mem(map_, map_size_, ALLOW_EXEC | ALLOW_READ | ALLOW_WRITE);
        memcpy(map_, replace_bytes_, nop_len_);
        protect_mem(map_, map_size_, ALLOW_EXEC | ALLOW_READ);
        icache_sync(map_);
    }

private:
    byte *map_ = nullptr;
    size_t map_size_ = 0;
    bool verbose_ = false;
    int nop_len_ = 0;
    byte replace_bytes_[MAX_INSTR_LENGTH];

    void
    generate_code()
    {
        void *dc = dr_standalone_init();
        assert(dc != nullptr);

        map_size_ = PAGE_SIZE;
        map_ = reinterpret_cast<byte *>(
            allocate_mem(map_size_, ALLOW_EXEC | ALLOW_READ | ALLOW_WRITE));
        assert(map_ != nullptr);

#ifdef ARM
        bool res = dr_set_isa_mode(dc, DR_ISA_ARM_A32, nullptr);
        assert(res);
#endif
        instrlist_t *ilist = instrlist_create(dc);
        reg_id_t base = IF_X86_ELSE(IF_X64_ELSE(DR_REG_RAX, DR_REG_EAX), DR_REG_R0);
        reg_id_t base4imm = IF_X86_64_ELSE(reg_64_to_32(base), base);
        int ptrsz = static_cast<int>(sizeof(void *));
        // An instruction we replace with something distinctive to check that
        // opcode_mix picked up the new encoding.
        instrlist_append(ilist, XINST_CREATE_nop(dc));
        nop_len_ = instr_length(dc, instrlist_last(ilist));
        // A two-immediate pattern we look for in the trace.
        instrlist_append(ilist,
                         XINST_CREATE_load_int(dc, opnd_create_reg(base4imm),
                                               OPND_CREATE_INT32(kGencodeMagic1)));
        instrlist_append(ilist,
                         XINST_CREATE_load_int(dc, opnd_create_reg(base4imm),
                                               OPND_CREATE_INT32(kGencodeMagic2)));
        instrlist_append(
            ilist,
            XINST_CREATE_move(dc, opnd_create_reg(base), opnd_create_reg(DR_REG_XSP)));
        instrlist_append(ilist,
                         XINST_CREATE_store(dc, OPND_CREATE_MEMPTR(base, -ptrsz),
                                            opnd_create_reg(base)));

#ifdef LINUX
        // Test a signal in non-module code.
#    ifdef X86
        instrlist_append(ilist, INSTR_CREATE_ud2(dc));
#    elif defined(AARCH64)
        // TODO i#4562: creating UDF is not yet supported so we create a
        // privileged instruction to SIGILL for us.
        instrlist_append(ilist, INSTR_CREATE_dc_ivac(dc, opnd_create_reg(base)));
#    else
        instrlist_append(ilist, INSTR_CREATE_udf(dc, OPND_CREATE_INT(0)));
#    endif
#endif

#ifdef ARM
        // XXX: Maybe XINST_CREATE_return should create "bx lr" like we need here
        // instead of the pop into pc which assumes the entry pushed lr?
        instrlist_append(ilist, INSTR_CREATE_bx(dc, opnd_create_reg(DR_REG_LR)));
#else
#    ifdef X86
        // Zero-iter rep-movs loop for testing emulation-marked code
        // written to the encoding file.
        instrlist_append(ilist,
                         INSTR_CREATE_xor(dc, opnd_create_reg(DR_REG_XCX),
                                          opnd_create_reg(DR_REG_XCX)));
        instrlist_append(ilist, INSTR_CREATE_rep_movs_1(dc));
#    endif

        instrlist_append(ilist, XINST_CREATE_return(dc));
#endif

        byte *last_pc = instrlist_encode(dc, ilist, map_, true);
        assert(last_pc <= map_ + map_size_);

        instrlist_clear_and_destroy(dc, ilist);

        protect_mem(map_, map_size_, ALLOW_EXEC | ALLOW_READ);

        // Prepare the encoding of an instr to replace the NOP.
        // The replacement must be the same size as the NOP and it
        // must be distinctive enough that it will not occur in
        // the hundreds in the code generated by the compiler to call
        // our generated code, so we can detect whether opcode_mix
        // recorded the new encoding.
        instr_t *replace = nullptr;
#ifdef X86
        replace = INSTR_CREATE_lahf(dc);
#elif defined(AARCH64)
        // OP_psb requires SPE feature.
        proc_set_feature(FEATURE_SPE, true);
        replace = INSTR_CREATE_psb_csync(dc);
#elif defined(ARM)
        replace = INSTR_CREATE_yield(dc);
#else
        assert(false);
#endif
        byte *next = instr_encode(dc, replace, replace_bytes_);
        assert(next != nullptr);
        assert(next - replace_bytes_ == nop_len_);
        instr_destroy(dc, replace);

        if (verbose_) {
            std::cerr << "Generated code:\n";
            byte *start_pc = reinterpret_cast<byte *>(map_);
            for (byte *pc = start_pc; pc < last_pc;) {
                pc = disassemble_with_info(dc, pc, STDERR, true, true);
                assert(pc != nullptr);
            }
        }

        dr_standalone_exit();
    }
};

/***************************************************************************
 * Top-level tracing.
 */

static int
do_some_work(code_generator_t &gen)
{
    static const int iters = 1000;
    for (int i = 0; i < iters; ++i) {
        gen.execute_generated_code();
        if (i == iters / 2)
            gen.modify_generated_code();
    }

    // TODO i#2062: Test code that triggers DR's "selfmod" instrumentation.

    // TODO i#2062: Test modified library code.

    return 1;
}

static void
exit_cb(void *)
{
    const char *encoding_path;
    drmemtrace_status_t res = drmemtrace_get_encoding_path(&encoding_path);
    assert(res == DRMEMTRACE_SUCCESS);
    std::ifstream stream(encoding_path);
    assert(stream.good());
}

static std::string
post_process()
{
    const char *raw_dir;
    drmemtrace_status_t mem_res = drmemtrace_get_output_path(&raw_dir);
    assert(mem_res == DRMEMTRACE_SUCCESS);
    std::string outdir = std::string(raw_dir) + DIRSEP + "post_processed";
    void *dr_context = dr_standalone_init();
    // Use a new scope to free raw2trace_directory_t before dr_standalone_exit().
    {
        raw2trace_directory_t dir;
        if (!dr_create_dir(outdir.c_str())) {
            std::cerr << "Failed to create output dir";
            assert(false);
        }
        std::string dir_err = dir.initialize(raw_dir, outdir);
        assert(dir_err.empty());
        raw2trace_t raw2trace(dir.modfile_bytes_, dir.in_files_, dir.out_files_,
                              dir.out_archives_, dir.encoding_file_,
                              dir.serial_schedule_file_, dir.cpu_schedule_file_,
                              dr_context);
        std::string error = raw2trace.do_conversion();
        if (!error.empty()) {
            std::cerr << "raw2trace failed: " << error << "\n";
            assert(false);
        }
    }
    dr_standalone_exit();
    return outdir;
}

static std::string
gather_trace()
{
#ifdef LINUX
    intercept_signal(SIGILL, handle_signal, false);
#endif

    if (!my_setenv("DYNAMORIO_OPTIONS",
#if defined(LINUX) && defined(X64)
                   // We pass -satisfy_w_xor_x to further stress that option
                   // interacting with standalone mode (xref i#5621).
                   "-satisfy_w_xor_x "
#endif
                   "-stderr_mask 0xc -client_lib ';;-offline"))
        std::cerr << "failed to set env var!\n";
    code_generator_t gen(false);
    std::cerr << "pre-DR init\n";
    dr_app_setup();
    assert(!dr_app_running_under_dynamorio());
    drmemtrace_status_t res = drmemtrace_buffer_handoff(nullptr, exit_cb, nullptr);
    assert(res == DRMEMTRACE_SUCCESS);
    std::cerr << "pre-DR start\n";
    dr_app_start();
    if (do_some_work(gen) < 0)
        std::cerr << "error in computation\n";
    std::cerr << "pre-DR detach\n";
    dr_app_stop_and_cleanup();
    std::cerr << "all done\n";
    return post_process();
}

static int
look_for_gencode(std::string trace_dir)
{
    void *dr_context = dr_standalone_init();
    scheduler_t scheduler;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(trace_dir);
    if (scheduler.init(sched_inputs, 1, scheduler_t::make_scheduler_serial_options()) !=
        scheduler_t::STATUS_SUCCESS) {
        std::cerr << "Failed to initialize scheduler " << scheduler.get_error_string()
                  << "\n";
    }
    bool found_magic1 = false, found_magic2 = false;
    bool have_instr_encodings = false;
#ifdef ARM
    // DR will auto-switch locally to Thumb for LSB=1 but not to ARM so we start as ARM.
    dr_set_isa_mode(dr_context, DR_ISA_ARM_A32, nullptr);
#endif
    auto *stream = scheduler.get_stream(0);
    memref_t memref;
    for (scheduler_t::stream_status_t status = stream->next_record(memref);
         status != scheduler_t::STATUS_EOF; status = stream->next_record(memref)) {
        assert(status == scheduler_t::STATUS_OK);
        if (memref.marker.type == TRACE_TYPE_MARKER &&
            memref.marker.marker_type == TRACE_MARKER_TYPE_FILETYPE &&
            TESTANY(OFFLINE_FILE_TYPE_ENCODINGS, memref.marker.marker_value)) {
            have_instr_encodings = true;
        }
        if (!type_is_instr(memref.instr.type)) {
            found_magic1 = false;
            continue;
        }
        app_pc pc = (app_pc)memref.instr.addr;
        if (!have_instr_encodings) {
            std::cerr << "Encodings are not present\n";
            return 1;
        }
        instr_t instr;
        instr_init(dr_context, &instr);
        app_pc next_pc =
            decode_from_copy(dr_context, memref.instr.encoding,
                             reinterpret_cast<byte *>(memref.instr.addr), &instr);
        assert(next_pc != nullptr && instr_valid(&instr));
        ptr_int_t immed;
        if (!found_magic1 && instr_is_mov_constant(&instr, &immed) &&
            immed == code_generator_t::kGencodeMagic1)
            found_magic1 = true;
        else if (found_magic1 && instr_is_mov_constant(&instr, &immed) &&
                 immed == code_generator_t::kGencodeMagic2)
            found_magic2 = true;
        else
            found_magic1 = false;
        instr_free(dr_context, &instr);
    }
    dr_standalone_exit();
    assert(found_magic2);
    return 0;
}

int
test_main(int argc, const char *argv[])
{
    std::string trace_dir = gather_trace();
    return look_for_gencode(trace_dir);
}

} // namespace drmemtrace
} // namespace dynamorio
