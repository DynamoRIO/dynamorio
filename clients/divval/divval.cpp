/* **********************************************************
 * Copyright (c) 2026 Google, Inc.  All rights reserved.
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

/* divval: records the operand values of AArch64 scalar integer divides
 * (UDIV and SDIV, both the 32-bit W and the 64-bit X forms).
 *
 * The goal is to characterize the operand distribution of divides in a
 * workload so that data-dependent divider latency schemes can be evaluated
 * offline and a simulator configured accordingly.
 *
 * Every dynamic divide appends a 24-byte record {dividend, divisor, site} to a
 * per-thread buffer using inline instrumentation (a handful of instructions,
 * no clean call per divide).  When the buffer fills up (or the thread exits)
 * a clean call folds the records into:
 *
 *   <prefix>.sites.csv  One row per static divide instruction: location,
 *                       opcode, width, execution count and value-locality
 *                       statistics.
 *   <prefix>.hist.csv   Joint histogram over (opcode, width, operand signs,
 *                       divisor-is-power-of-2, significant bits of |dividend|,
 *                       |divisor| and |quotient|).  This is enough to evaluate
 *                       any latency model that depends on operand/quotient
 *                       magnitudes, and its size does not grow with run length.
 *   <prefix>.trace.bin  With -trace: the full dynamic sequence of divides
 *   (or .trace.csv)     (site, dividend, divisor), for models that need exact
 *                       values or ordering (e.g., memoization).
 *
 * <prefix> is <outdir>/divval.<app>.<pid>.  See README.md and divval.py.
 */

#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "drvector.h"
#include "droption.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef AARCH64
#    error "divval only supports AArch64"
#endif

namespace dynamorio {
namespace divval {
namespace {

using ::dynamorio::droption::droption_parser_t;
using ::dynamorio::droption::DROPTION_SCOPE_ALL;
using ::dynamorio::droption::DROPTION_SCOPE_CLIENT;
using ::dynamorio::droption::droption_t;

droption_t<std::string>
    op_outdir(DROPTION_SCOPE_CLIENT, "outdir", "", "Directory for the output files.",
              "Directory where the output files are written.  Defaults to the "
              "current working directory of the application.");

droption_t<bool>
    op_trace(DROPTION_SCOPE_CLIENT, "trace", false,
             "Also write the full sequence of dynamic divides.",
             "In addition to the aggregate outputs, write one record per dynamic "
             "divide (site, dividend, divisor) in execution order.  The output can be "
             "large: 24 bytes per divide in binary format.");

droption_t<std::string>
    op_trace_format(DROPTION_SCOPE_CLIENT, "trace_format", "binary",
                    "Format of the -trace output: binary or csv.",
                    "Format of the -trace output.  'binary' writes a 16-byte header "
                    "followed by little-endian {u64 dividend, u64 divisor, u64 site} "
                    "records (see divval.py).  'csv' writes "
                    "seq,site,op,width,dividend,divisor rows.");

droption_t<unsigned long long>
    op_trace_max(DROPTION_SCOPE_CLIENT, "trace_max", 0,
                 "Maximum number of -trace records (0: unlimited).",
                 "Stop writing -trace records after this many.  The aggregate outputs "
                 "keep covering the whole run.  0 means unlimited.");

droption_t<unsigned int> op_verbose(DROPTION_SCOPE_CLIENT, "verbose", 0,
                                    "Verbosity level.",
                                    "Verbosity level for messages printed to stderr.");

/* One record per dynamic divide, written by the inline instrumentation.  The
 * register values are the raw 64-bit X registers; for 32-bit (W) divides the
 * upper half is ignored during processing.  The same layout, with normalized
 * values (see normalize()), is used for the binary -trace output.
 */
struct div_rec_t {
    uint64_t dividend; /* Rn. */
    uint64_t divisor;  /* Rm. */
    uint64_t site;     /* Index into g_sites. */
};
static_assert(sizeof(div_rec_t) == 24, "div_rec_t must be packed");

/* The inline code flushes when the buffer pointer becomes exactly equal to the
 * buffer end, so the size must be a multiple of the record size.
 */
constexpr size_t kRecordsPerBuffer = 8192;
constexpr size_t kBufferSize = kRecordsPerBuffer * sizeof(div_rec_t);

/* Raw TLS slots, accessed directly from the code cache. */
enum {
    TLS_SLOT_BUF_PTR, /* Next free record in this thread's buffer. */
    TLS_SLOT_BUF_END, /* End of this thread's buffer. */
    TLS_SLOT_COUNT,
};

/* Header of the binary -trace output. */
struct trace_header_t {
    char magic[8];        /* "DIVVALTR" */
    uint32_t version;     /* kTraceVersion */
    uint32_t record_size; /* sizeof(div_rec_t) */
};
static_assert(sizeof(trace_header_t) == 16, "trace_header_t must be packed");
constexpr uint32_t kTraceVersion = 1;

struct site_t {
    app_pc pc;
    bool is_signed;
    int width; /* 32 or 64. */
    std::string module;
    uint64_t offset; /* Offset of pc from the module base, or pc if no module. */
    std::string disasm;
    /* Dynamic statistics. */
    uint64_t count = 0;
    uint64_t div_by_zero = 0;
    uint64_t divisor_pow2 = 0;
    uint64_t quotient_zero = 0;
    uint64_t sum_quotient_bits = 0;
    /* Same divisor (or both operands) as the previous execution of this site. */
    uint64_t repeat_divisor = 0;
    uint64_t repeat_operands = 0;
    uint64_t prev_dividend = 0;
    uint64_t prev_divisor = 0;
};

/* Operand values after applying the width/signedness of the instruction. */
struct normalized_t {
    uint64_t dividend; /* Zero- (UDIV) or sign- (SDIV) extended to 64 bits. */
    uint64_t divisor;
    bool dividend_neg;
    bool divisor_neg;
    uint64_t dividend_mag; /* |dividend|. */
    uint64_t divisor_mag;
    uint64_t quotient_mag; /* |quotient| as computed by the hardware. */
};

/* All of the following are protected by g_lock. */
void *g_lock;
std::vector<site_t *> *g_sites;
/* Maps (pc | is_signed << 1 | is64) to an index into g_sites.  AArch64 pcs are
 * 4-byte aligned, so the low bits are free.
 */
std::unordered_map<uint64_t, uint64_t> *g_site_index;
/* Joint histogram; see hist_key(). */
std::unordered_map<uint32_t, uint64_t> *g_hist;
file_t g_trace_file = INVALID_FILE;
bool g_trace_csv;
uint64_t g_trace_written;
bool g_trace_truncated;
/* Staging buffer for the -trace output. */
char *g_trace_out;
size_t g_trace_out_len;
constexpr size_t kTraceOutSize = 1 << 20;

std::string g_prefix;
reg_id_t g_tls_seg;
uint g_tls_offs;
int g_tls_idx;

struct per_thread_t {
    /* Saved because thread exit events may run on another thread at process
     * exit, where dr_get_dr_segment_base() would return the wrong base.
     */
    byte *seg_base;
    byte *buf_base;
};

inline byte **
tls_slot(per_thread_t *pt, int slot)
{
    return (byte **)(pt->seg_base + g_tls_offs + slot * sizeof(void *));
}

inline int
sig_bits(uint64_t x)
{
    return x == 0 ? 0 : 64 - __builtin_clzll(x);
}

inline bool
is_pow2(uint64_t x)
{
    return x != 0 && (x & (x - 1)) == 0;
}

normalized_t
normalize(const site_t &site, uint64_t raw_dividend, uint64_t raw_divisor)
{
    normalized_t v;
    uint64_t n = raw_dividend, d = raw_divisor;
    if (site.width == 32) {
        if (site.is_signed) {
            n = (uint64_t)(int64_t)(int32_t)(uint32_t)n;
            d = (uint64_t)(int64_t)(int32_t)(uint32_t)d;
        } else {
            n = (uint32_t)n;
            d = (uint32_t)d;
        }
    }
    v.dividend = n;
    v.divisor = d;
    v.dividend_neg = site.is_signed && (int64_t)n < 0;
    v.divisor_neg = site.is_signed && (int64_t)d < 0;
    /* 0 - x yields the correct magnitude even for INT64_MIN. */
    v.dividend_mag = v.dividend_neg ? 0 - n : n;
    v.divisor_mag = v.divisor_neg ? 0 - d : d;
    /* AArch64 does not trap on division by zero: the result is 0.  SDIV rounds
     * towards zero, so |q| = |n| / |d|.  For INT_MIN / -1 this gives 2^(w-1),
     * which is also the magnitude of the architectural (wrapped) result INT_MIN.
     */
    v.quotient_mag = v.divisor_mag == 0 ? 0 : v.dividend_mag / v.divisor_mag;
    return v;
}

/* Packs the histogram dimensions into a key; see write_hist() for the layout. */
inline uint32_t
hist_key(const site_t &site, const normalized_t &v)
{
    return (uint32_t)site.is_signed | (uint32_t)(site.width == 64) << 1 |
        (uint32_t)v.dividend_neg << 2 | (uint32_t)v.divisor_neg << 3 |
        (uint32_t)is_pow2(v.divisor_mag) << 4 | (uint32_t)sig_bits(v.dividend_mag) << 8 |
        (uint32_t)sig_bits(v.divisor_mag) << 16 |
        (uint32_t)sig_bits(v.quotient_mag) << 24;
}

const char *
op_name(const site_t &site)
{
    return site.is_signed ? "sdiv" : "udiv";
}

void
trace_out_flush()
{
    if (g_trace_out_len == 0)
        return;
    if (dr_write_file(g_trace_file, g_trace_out, g_trace_out_len) !=
        (ssize_t)g_trace_out_len) {
        dr_fprintf(STDERR, "divval: error writing the trace file\n");
    }
    g_trace_out_len = 0;
}

void
trace_append(const site_t &site, uint64_t site_idx, const normalized_t &v)
{
    if (g_trace_csv) {
        /* Worst case line length is well below 128 bytes. */
        if (kTraceOutSize - g_trace_out_len < 128)
            trace_out_flush();
        int len;
        if (site.is_signed) {
            len = dr_snprintf(g_trace_out + g_trace_out_len,
                              kTraceOutSize - g_trace_out_len,
                              UINT64_FORMAT_STRING "," UINT64_FORMAT_STRING
                                                   ",%s,%d," INT64_FORMAT_STRING
                                                   "," INT64_FORMAT_STRING "\n",
                              g_trace_written, site_idx, op_name(site), site.width,
                              (int64_t)v.dividend, (int64_t)v.divisor);
        } else {
            len = dr_snprintf(g_trace_out + g_trace_out_len,
                              kTraceOutSize - g_trace_out_len,
                              UINT64_FORMAT_STRING "," UINT64_FORMAT_STRING
                                                   ",%s,%d," UINT64_FORMAT_STRING
                                                   "," UINT64_FORMAT_STRING "\n",
                              g_trace_written, site_idx, op_name(site), site.width,
                              v.dividend, v.divisor);
        }
        DR_ASSERT(len > 0);
        g_trace_out_len += len;
    } else {
        if (kTraceOutSize - g_trace_out_len < sizeof(div_rec_t))
            trace_out_flush();
        div_rec_t rec = { v.dividend, v.divisor, site_idx };
        memcpy(g_trace_out + g_trace_out_len, &rec, sizeof(rec));
        g_trace_out_len += sizeof(rec);
    }
    ++g_trace_written;
}

/* Folds the records in [buf_base, buf_base + size) into the global state. */
void
process_buffer(void *buf_base, size_t size)
{
    const div_rec_t *recs = (const div_rec_t *)buf_base;
    size_t num = size / sizeof(div_rec_t);
    dr_mutex_lock(g_lock);
    for (size_t i = 0; i < num; ++i) {
        const div_rec_t &rec = recs[i];
        if (rec.site >= g_sites->size()) {
            DR_ASSERT_MSG(false, "divval: corrupted record");
            continue;
        }
        site_t &site = *(*g_sites)[rec.site];
        normalized_t v = normalize(site, rec.dividend, rec.divisor);

        if (site.count > 0) {
            if (v.divisor == site.prev_divisor) {
                ++site.repeat_divisor;
                if (v.dividend == site.prev_dividend)
                    ++site.repeat_operands;
            }
        }
        site.prev_dividend = v.dividend;
        site.prev_divisor = v.divisor;
        ++site.count;
        if (v.divisor_mag == 0)
            ++site.div_by_zero;
        if (is_pow2(v.divisor_mag))
            ++site.divisor_pow2;
        if (v.quotient_mag == 0)
            ++site.quotient_zero;
        site.sum_quotient_bits += sig_bits(v.quotient_mag);

        ++(*g_hist)[hist_key(site, v)];

        if (g_trace_file != INVALID_FILE) {
            if (op_trace_max.get_value() == 0 ||
                g_trace_written < op_trace_max.get_value()) {
                trace_append(site, rec.site, v);
            } else
                g_trace_truncated = true;
        }
    }
    dr_mutex_unlock(g_lock);
}

/* Processes and empties the buffer of the thread owning drcontext. */
void
flush_thread_buffer(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, g_tls_idx);
    byte **ptr = tls_slot(pt, TLS_SLOT_BUF_PTR);
    process_buffer(pt->buf_base, *ptr - pt->buf_base);
    *ptr = pt->buf_base;
}

/* Clean call target, invoked from the code cache when the buffer is full. */
void
clean_call_flush()
{
    flush_thread_buffer(dr_get_current_drcontext());
}

void
event_thread_init(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)dr_thread_alloc(drcontext, sizeof(*pt));
    pt->seg_base = (byte *)dr_get_dr_segment_base(g_tls_seg);
    pt->buf_base = (byte *)dr_thread_alloc(drcontext, kBufferSize);
    *tls_slot(pt, TLS_SLOT_BUF_PTR) = pt->buf_base;
    *tls_slot(pt, TLS_SLOT_BUF_END) = pt->buf_base + kBufferSize;
    drmgr_set_tls_field(drcontext, g_tls_idx, pt);
}

void
event_thread_exit(void *drcontext)
{
    flush_thread_buffer(drcontext);
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, g_tls_idx);
    dr_thread_free(drcontext, pt->buf_base, kBufferSize);
    dr_thread_free(drcontext, pt, sizeof(*pt));
}

uint64_t
get_or_create_site(void *drcontext, instr_t *instr, bool is_signed, bool is64)
{
    app_pc pc = instr_get_app_pc(instr);
    uint64_t key = (uint64_t)pc | (uint64_t)is_signed << 1 | (uint64_t)is64;
    dr_mutex_lock(g_lock);
    auto it = g_site_index->find(key);
    if (it != g_site_index->end()) {
        uint64_t idx = it->second;
        dr_mutex_unlock(g_lock);
        return idx;
    }
    site_t *site = new site_t;
    site->pc = pc;
    site->is_signed = is_signed;
    site->width = is64 ? 64 : 32;
    module_data_t *mod = dr_lookup_module(pc);
    if (mod != nullptr) {
        const char *name = dr_module_preferred_name(mod);
        site->module = name == nullptr ? "" : name;
        site->offset = pc - mod->start;
        dr_free_module_data(mod);
    } else {
        site->module = "<unknown>";
        site->offset = (uint64_t)pc;
    }
    char dis[128];
    instr_disassemble_to_buffer(drcontext, instr, dis, sizeof(dis));
    site->disasm = dis;
    uint64_t idx = g_sites->size();
    g_sites->push_back(site);
    (*g_site_index)[key] = idx;
    dr_mutex_unlock(g_lock);
    if (op_verbose.get_value() > 1) {
        dr_fprintf(STDERR, "divval: site " UINT64_FORMAT_STRING ": %s+" PIFX " %s\n", idx,
                   site->module.c_str(), (ptr_uint_t)site->offset, site->disasm.c_str());
    }
    return idx;
}

inline bool
is_x_reg(reg_id_t reg)
{
    return (reg >= DR_REG_X0 && reg <= DR_REG_X30) || reg == DR_REG_XZR;
}

inline bool
is_w_reg(reg_id_t reg)
{
    return (reg >= DR_REG_W0 && reg <= DR_REG_W30) || reg == DR_REG_WZR;
}

/* Returns whether instr is a scalar UDIV/SDIV and if so its operands. */
bool
is_scalar_div(instr_t *instr, reg_id_t *dividend, reg_id_t *divisor, bool *is_signed,
              bool *is64)
{
    int opc = instr_get_opcode(instr);
    if (opc != OP_udiv && opc != OP_sdiv)
        return false;
    /* The SVE (vector) UDIV/SDIV share these opcodes, but they have a governing
     * predicate as an additional source and operate on Z registers.
     */
    if (instr_num_srcs(instr) != 2 || instr_num_dsts(instr) != 1)
        return false;
    opnd_t n = instr_get_src(instr, 0);
    opnd_t m = instr_get_src(instr, 1);
    if (!opnd_is_reg(n) || !opnd_is_reg(m))
        return false;
    reg_id_t rn = opnd_get_reg(n);
    reg_id_t rm = opnd_get_reg(m);
    if (is_x_reg(rn) && is_x_reg(rm))
        *is64 = true;
    else if (is_w_reg(rn) && is_w_reg(rm))
        *is64 = false;
    else
        return false;
    *dividend = rn;
    *divisor = rm;
    *is_signed = opc == OP_sdiv;
    return true;
}

#define MINSERT(bb, where, instr) instrlist_meta_preinsert((bb), (where), (instr))

/* Stores the application value of app_reg (as a full X register) at offset. */
void
insert_store_app_reg(void *drcontext, instrlist_t *bb, instr_t *where, reg_id_t app_reg,
                     reg_id_t buf_ptr, reg_id_t scratch, int offset)
{
    reg_id_t src;
    if (app_reg == DR_REG_XZR || app_reg == DR_REG_WZR) {
        instrlist_insert_mov_immed_ptrsz(drcontext, 0, opnd_create_reg(scratch), bb,
                                         where, nullptr, nullptr);
        src = scratch;
    } else {
        reg_id_t xreg = reg_to_pointer_sized(app_reg);
        /* The stolen register holds DR's TLS base: its app value lives in memory
         * and must be loaded into a scratch register.  For other registers this
         * restores the app value in place if a previous drreg reservation left a
         * tool value in it (lazy restore); usually it is a no-op.
         */
        src = xreg == dr_get_stolen_reg() ? scratch : xreg;
        drreg_status_t res = drreg_get_app_value(drcontext, bb, where, xreg, src);
        DR_ASSERT(res == DRREG_SUCCESS);
    }
    MINSERT(bb, where,
            XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(buf_ptr, offset),
                               opnd_create_reg(src)));
}

dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                      bool for_trace, bool translating, void *user_data)
{
    reg_id_t dividend, divisor;
    bool is_signed, is64;
    if (!instr_is_app(instr) ||
        !is_scalar_div(instr, &dividend, &divisor, &is_signed, &is64))
        return DR_EMIT_DEFAULT;

    /* Translation must produce identical code: the site lookup is idempotent. */
    uint64_t site = get_or_create_site(drcontext, instr, is_signed, is64);

    /* Keep the operand registers out of our scratch set so that their app
     * values are intact when we read them.
     */
    drvector_t allowed;
    drreg_init_and_fill_vector(&allowed, true);
    if (dividend != DR_REG_XZR && dividend != DR_REG_WZR)
        drreg_set_vector_entry(&allowed, reg_to_pointer_sized(dividend), false);
    if (divisor != DR_REG_XZR && divisor != DR_REG_WZR)
        drreg_set_vector_entry(&allowed, reg_to_pointer_sized(divisor), false);
    reg_id_t buf_ptr, scratch;
    if (drreg_reserve_register(drcontext, bb, instr, &allowed, &buf_ptr) !=
            DRREG_SUCCESS ||
        drreg_reserve_register(drcontext, bb, instr, &allowed, &scratch) !=
            DRREG_SUCCESS) {
        DR_ASSERT_MSG(false, "divval: failed to reserve scratch registers");
        drvector_delete(&allowed);
        return DR_EMIT_DEFAULT;
    }
    drvector_delete(&allowed);

    /* Emitted sequence (no arithmetic flags are touched):
     *   ldr  buf_ptr, [tls + BUF_PTR]
     *   mov  scratch, #site
     *   str  scratch, [buf_ptr, #16]
     *   str  Xm, [buf_ptr, #8]
     *   str  Xn, [buf_ptr, #0]
     *   add  buf_ptr, buf_ptr, #24
     *   str  buf_ptr, [tls + BUF_PTR]
     *   ldr  scratch, [tls + BUF_END]
     *   sub  scratch, scratch, buf_ptr
     *   cbnz scratch, done
     *   <clean call to clean_call_flush>
     * done:
     */
    dr_insert_read_raw_tls(drcontext, bb, instr, g_tls_seg,
                           g_tls_offs + TLS_SLOT_BUF_PTR * sizeof(void *), buf_ptr);
    instrlist_insert_mov_immed_ptrsz(drcontext, (ptr_int_t)site, opnd_create_reg(scratch),
                                     bb, instr, nullptr, nullptr);
    MINSERT(bb, instr,
            XINST_CREATE_store(drcontext,
                               OPND_CREATE_MEMPTR(buf_ptr, offsetof(div_rec_t, site)),
                               opnd_create_reg(scratch)));
    insert_store_app_reg(drcontext, bb, instr, divisor, buf_ptr, scratch,
                         offsetof(div_rec_t, divisor));
    insert_store_app_reg(drcontext, bb, instr, dividend, buf_ptr, scratch,
                         offsetof(div_rec_t, dividend));
    MINSERT(bb, instr,
            XINST_CREATE_add(drcontext, opnd_create_reg(buf_ptr),
                             OPND_CREATE_INT32(sizeof(div_rec_t))));
    dr_insert_write_raw_tls(drcontext, bb, instr, g_tls_seg,
                            g_tls_offs + TLS_SLOT_BUF_PTR * sizeof(void *), buf_ptr);
    dr_insert_read_raw_tls(drcontext, bb, instr, g_tls_seg,
                           g_tls_offs + TLS_SLOT_BUF_END * sizeof(void *), scratch);
    MINSERT(
        bb, instr,
        XINST_CREATE_sub(drcontext, opnd_create_reg(scratch), opnd_create_reg(buf_ptr)));
    instr_t *done = INSTR_CREATE_label(drcontext);
    MINSERT(
        bb, instr,
        INSTR_CREATE_cbnz(drcontext, opnd_create_instr(done), opnd_create_reg(scratch)));
    dr_insert_clean_call(drcontext, bb, instr, (void *)clean_call_flush,
                         false /*fpstate*/, 0);
    MINSERT(bb, instr, done);

    if (drreg_unreserve_register(drcontext, bb, instr, scratch) != DRREG_SUCCESS ||
        drreg_unreserve_register(drcontext, bb, instr, buf_ptr) != DRREG_SUCCESS)
        DR_ASSERT_MSG(false, "divval: failed to unreserve scratch registers");
    return DR_EMIT_DEFAULT;
}

file_t
open_output(const std::string &suffix)
{
    std::string path = g_prefix + suffix;
    file_t f = dr_open_file(path.c_str(), DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    if (f == INVALID_FILE)
        dr_fprintf(STDERR, "divval: failed to open %s\n", path.c_str());
    return f;
}

void
write_sites()
{
    file_t f = open_output(".sites.csv");
    if (f == INVALID_FILE)
        return;
    dr_fprintf(f,
               "site,pc,module,offset,op,width,count,div_by_zero,divisor_pow2,"
               "quotient_zero,repeat_divisor,repeat_operands,avg_quotient_bits,"
               "disasm\n");
    for (size_t i = 0; i < g_sites->size(); ++i) {
        const site_t &s = *(*g_sites)[i];
        /* Fixed point with 3 decimals, printed digit by digit: DR's printf
         * supports neither %f nor zero padding.
         */
        uint64_t avg_milli = s.count == 0 ? 0 : (s.sum_quotient_bits * 1000) / s.count;
        dr_fprintf(f,
                   UINT64_FORMAT_STRING
                   "," PIFX ",\"%s\"," PIFX ",%s,%d," UINT64_FORMAT_STRING
                   "," UINT64_FORMAT_STRING "," UINT64_FORMAT_STRING
                   "," UINT64_FORMAT_STRING "," UINT64_FORMAT_STRING
                   "," UINT64_FORMAT_STRING "," UINT64_FORMAT_STRING ".%d%d%d,\"%s\"\n",
                   (uint64_t)i, (ptr_uint_t)s.pc, s.module.c_str(), (ptr_uint_t)s.offset,
                   op_name(s), s.width, s.count, s.div_by_zero, s.divisor_pow2,
                   s.quotient_zero, s.repeat_divisor, s.repeat_operands, avg_milli / 1000,
                   (int)(avg_milli / 100 % 10), (int)(avg_milli / 10 % 10),
                   (int)(avg_milli % 10), s.disasm.c_str());
    }
    dr_close_file(f);
}

void
write_hist()
{
    file_t f = open_output(".hist.csv");
    if (f == INVALID_FILE)
        return;
    std::vector<std::pair<uint32_t, uint64_t>> rows(g_hist->begin(), g_hist->end());
    /* Order by op, width, dividend/divisor/quotient bits, then the flags. */
    auto order = [](uint32_t k) {
        uint64_t is_signed = k & 1, is64 = (k >> 1) & 1, flags = (k >> 2) & 0x7;
        uint64_t n_bits = (k >> 8) & 0xff, d_bits = (k >> 16) & 0xff;
        uint64_t q_bits = (k >> 24) & 0xff;
        return is_signed << 40 | is64 << 32 | n_bits << 24 | d_bits << 16 | q_bits << 8 |
            flags;
    };
    std::sort(rows.begin(), rows.end(), [&](const auto &a, const auto &b) {
        return order(a.first) < order(b.first);
    });
    dr_fprintf(f,
               "op,width,dividend_neg,divisor_neg,divisor_pow2,dividend_bits,"
               "divisor_bits,quotient_bits,count\n");
    for (const auto &row : rows) {
        uint32_t k = row.first;
        dr_fprintf(f, "%s,%d,%u,%u,%u,%u,%u,%u," UINT64_FORMAT_STRING "\n",
                   (k & 1) != 0 ? "sdiv" : "udiv", (k & 2) != 0 ? 64 : 32, (k >> 2) & 1,
                   (k >> 3) & 1, (k >> 4) & 1, (k >> 8) & 0xff, (k >> 16) & 0xff,
                   (k >> 24) & 0xff, row.second);
    }
    dr_close_file(f);
}

void
open_trace()
{
    const std::string &fmt = op_trace_format.get_value();
    g_trace_csv = fmt == "csv";
    g_trace_file = open_output(g_trace_csv ? ".trace.csv" : ".trace.bin");
    if (g_trace_file == INVALID_FILE)
        return;
    g_trace_out = (char *)dr_global_alloc(kTraceOutSize);
    g_trace_out_len = 0;
    if (g_trace_csv) {
        dr_fprintf(g_trace_file, "seq,site,op,width,dividend,divisor\n");
    } else {
        trace_header_t hdr;
        memcpy(hdr.magic, "DIVVALTR", sizeof(hdr.magic));
        hdr.version = kTraceVersion;
        hdr.record_size = sizeof(div_rec_t);
        dr_write_file(g_trace_file, &hdr, sizeof(hdr));
    }
}

void
event_exit()
{
    /* The thread exit events, which flush the last partial buffer of each thread,
     * have already run at this point.
     */
    bool have_trace = g_trace_file != INVALID_FILE;
    if (have_trace) {
        trace_out_flush();
        dr_close_file(g_trace_file);
        g_trace_file = INVALID_FILE;
        dr_global_free(g_trace_out, kTraceOutSize);
    }
    write_sites();
    write_hist();

    uint64_t counts[2][2] = { }; /* [is_signed][is64] */
    for (const site_t *s : *g_sites)
        counts[s->is_signed][s->width == 64] += s->count;
    uint64_t total = counts[0][0] + counts[0][1] + counts[1][0] + counts[1][1];
    dr_fprintf(STDERR,
               "divval: " UINT64_FORMAT_STRING " divides (udiv32 " UINT64_FORMAT_STRING
               ", udiv64 " UINT64_FORMAT_STRING ", sdiv32 " UINT64_FORMAT_STRING
               ", sdiv64 " UINT64_FORMAT_STRING ") at " UINT64_FORMAT_STRING " sites.\n",
               total, counts[0][0], counts[0][1], counts[1][0], counts[1][1],
               (uint64_t)g_sites->size());
    dr_fprintf(STDERR, "divval: wrote %s.sites.csv %s.hist.csv", g_prefix.c_str(),
               g_prefix.c_str());
    if (have_trace) {
        dr_fprintf(STDERR, " %s.trace.%s", g_prefix.c_str(), g_trace_csv ? "csv" : "bin");
    }
    dr_fprintf(STDERR, "\n");
    if (g_trace_truncated) {
        dr_fprintf(STDERR,
                   "divval: trace truncated at " UINT64_FORMAT_STRING
                   " records (-trace_max)\n",
                   g_trace_written);
    }

    if (!drmgr_unregister_bb_insertion_event(event_app_instruction) ||
        !drmgr_unregister_thread_init_event(event_thread_init) ||
        !drmgr_unregister_thread_exit_event(event_thread_exit) ||
        !drmgr_unregister_tls_field(g_tls_idx) ||
        !dr_raw_tls_cfree(g_tls_offs, TLS_SLOT_COUNT))
        DR_ASSERT(false);
    drreg_exit();
    drmgr_exit();

    for (site_t *s : *g_sites)
        delete s;
    delete g_sites;
    delete g_site_index;
    delete g_hist;
    dr_mutex_destroy(g_lock);
}

} // namespace
} // namespace divval
} // namespace dynamorio

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    using namespace ::dynamorio::divval;
    dr_set_client_name("divval: AArch64 divide operand profiler",
                       "http://dynamorio.org/issues");

    std::string parse_err;
    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_CLIENT, argc, argv, &parse_err,
                                       nullptr)) {
        dr_fprintf(STDERR, "divval: usage error: %s\nUsage:\n%s", parse_err.c_str(),
                   droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
        dr_abort();
    }
    if (op_trace_format.get_value() != "binary" && op_trace_format.get_value() != "csv") {
        dr_fprintf(STDERR, "divval: -trace_format must be 'binary' or 'csv'\n");
        dr_abort();
    }

    drreg_options_t ops = { sizeof(ops), 2 /*max slots needed*/, false };
    if (!drmgr_init() || drreg_init(&ops) != DRREG_SUCCESS)
        DR_ASSERT(false);

    g_lock = dr_mutex_create();
    g_sites = new std::vector<site_t *>();
    g_site_index = new std::unordered_map<uint64_t, uint64_t>();
    g_hist = new std::unordered_map<uint32_t, uint64_t>();

    std::string dir = op_outdir.get_value();
    if (dir.empty()) {
        char cwd[MAXIMUM_PATH];
        if (dr_get_current_directory(cwd, sizeof(cwd)))
            dir = cwd;
        else
            dir = ".";
    }
    const char *app = dr_get_application_name();
    g_prefix = dir + "/divval." + (app == nullptr ? "app" : app) + "." +
        std::to_string(dr_get_process_id());

    if (op_trace.get_value())
        open_trace();

    if (!dr_raw_tls_calloc(&g_tls_seg, &g_tls_offs, TLS_SLOT_COUNT, 0))
        DR_ASSERT(false);
    g_tls_idx = drmgr_register_tls_field();
    DR_ASSERT(g_tls_idx != -1);

    drmgr_register_exit_event(event_exit);
    if (!drmgr_register_thread_init_event(event_thread_init) ||
        !drmgr_register_thread_exit_event(event_thread_exit) ||
        !drmgr_register_bb_instrumentation_event(nullptr, event_app_instruction, nullptr))
        DR_ASSERT(false);
    if (op_verbose.get_value() > 0)
        dr_fprintf(STDERR, "divval: writing results to %s.*\n", g_prefix.c_str());
}
