/* ******************************************************************************
 * Copyright (c) 2014-2018 Google, Inc.  All rights reserved.
 * Copyright (c) 2011 Massachusetts Institute of Technology  All rights
 * reserved. Copyright (c) 2008 VMware, Inc.  All rights reserved. 2018
 * University of Regensburg, Germany (QPACE 4, SFB TRR-55)
 *******************************************************************************/

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
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
 * Counter for NEON/SIMD-instructions on AARCH64, based on the inscount.cpp
 * sample
 */

#include "dr_api.h"
#include "dr_tools.h"
#include "drmgr.h"

#include "string.h"
#include <stdio.h>

// ARM-Architecture Reference Book p. 185ff
// Arithmetic instructions:
int OP_arithmetic[] = {
    OP_add,    OP_adds,     OP_sub,     OP_subs,   OP_adc,    OP_adcs,   OP_sbc,
    OP_sbcs,   OP_madd,     OP_msub,    OP_mul,    OP_smaddl, OP_smsubl, OP_smulh,
    OP_umaddl, OP_umsubl,   OP_umulh,   OP_sdiv,   OP_udiv,   OP_fmadd,  OP_fmsub,
    OP_fnmadd, OP_fnmsub,   OP_fabs,    OP_fneg,   OP_fsqrt,  OP_fadd,   OP_fdiv,
    OP_fmul,   OP_fnmul,    OP_fsub,    OP_fmax,   OP_fmaxnm, OP_fmin,   OP_fminnm,
    OP_fabd,   OP_fmla,     OP_fmlal,   OP_fmlal2, OP_fmls,   OP_fmlsl,  OP_fmlsl2,
    OP_fmulx,  OP_frecps,   OP_frsqrts, OP_mla,    OP_mls,    OP_pmul,   OP_saba,
    OP_sabd,   OP_shadd,    OP_shsub,   OP_smax,   OP_smin,   OP_sqadd,  OP_sqdmulh,
    OP_sqrshl, OP_sqrdmulh, OP_sqshl,   OP_sqsub,  OP_srhadd, OP_srshl,  OP_sshl,
    OP_uaba,   OP_uabd,     OP_uhadd,   OP_uhsub,  OP_umax,   OP_umin,   OP_uqadd,
    OP_uqrshl, OP_uqshl,    OP_uqsub,   OP_urhadd, OP_urshl,  OP_ushl,   OP_cls,
    OP_clz,
};

struct counts_t {
    uint64 all;
    uint64 arith;
    uint64 neon;
    uint64 neon_arith;
    uint64 neon_load;
    uint64 neon_store;
    uint64 branching;
    uint64 taken_branches;
    uint64 load;
    uint64 load_linear;
    uint64 load_structured;
    uint64 store;
    uint64 store_linear;
    uint64 store_structured;
};

// Save counts
struct counts_t global_counts = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

// If this file is not empty all new data is added at the end
FILE *result_file;

// Count only instructions in the application itself, ignoring instructions in
// shared libraries.
static bool app_only = false;
// application module
static app_pc exe_start;

// saves the name of the executed app
static const char *executable;

// use this function for clean calls
static void
inscount(uint num_instrs, uint num_arith, uint num_simd, uint num_simd_arith,
         uint num_simd_load, uint num_simd_store, uint num_branching, uint num_load,
         uint num_load_linear, uint num_load_structured, uint num_store,
         uint num_store_linear, uint num_store_structured)
{
    global_counts.all += (uint64)num_instrs;
    global_counts.arith += (uint64)num_arith;
    global_counts.neon += (uint64)num_simd;
    global_counts.neon_arith += (uint64)num_simd_arith;
    global_counts.neon_load += (uint64)num_simd_load;
    global_counts.neon_store += (uint64)num_simd_store;
    global_counts.branching += (uint64)num_branching;
    global_counts.load += (uint64)num_load;
    global_counts.load_linear += (uint64)num_load_linear;
    global_counts.load_structured += (uint64)num_load_structured;
    global_counts.store += (uint64)num_store;
    global_counts.store_linear += (uint64)num_store_linear;
    global_counts.store_structured += (uint64)num_store_structured;
}

/*
 * check for NEON Instructions
 * SIMD-Registers: Q0-Q31, D0-D31, S0-S31, H0-H31, B0-B31 (dr_ir_opnd.h); SVE:
 * Z0-Z31 returns true if ins is a NEON instruction
 */
bool
instr_is_neon(instr_t *ins, bool is_load, bool is_store)
{
    bool is_neon = false;
    reg_id_t reg;
    int op_code = instr_get_opcode(ins);

    /* NEON and FP use the same registers
     * arithmetic NEON Instr. store their width in an additional src-slot
     * this variable holds the number of src slots that acutally point to a
     * register
     */
    int num_src = 0;

    if (is_load || is_store) {
        num_src = instr_num_srcs(ins);
    } else {
        switch (op_code) {
        case OP_fnmadd:
        case OP_fnmsub:
        case OP_fmadd:
        case OP_fmsub:
        case OP_fmla:
        case OP_mla:
            // these instructions have three sources when scalar
            if (!(instr_num_srcs(ins) == 4)) {
                return false;
            } else {
                num_src = 3;
            }
            break;
        case OP_fmlal:
        case OP_fmlal2:
        case OP_fmlsl:
        case OP_fmlsl2:
            // these are always vector instructions
            num_src = 3;
            break;
        case OP_shadd:
        case OP_sqadd:
        case OP_srhadd:
        case OP_shsub:
        case OP_sqsub:
        case OP_cmgt:
        case OP_cmge:
        case OP_sshl:
        case OP_sqshl:
        case OP_srshl:
        case OP_sqrshl:
        case OP_smax:
        case OP_smin:
        case OP_sabd:
        case OP_saba:
        case OP_add:
        case OP_cmtst:
        case OP_mul:
        case OP_smaxp:
        case OP_sminp:
        case OP_sqdmulh:
        case OP_addp:
        case OP_fmaxnm:
        case OP_fadd:
        case OP_fmulx:
        case OP_fcmeq:
        case OP_fmax:
        case OP_frecps:
        case OP_fminnm:
        case OP_fsub:
        case OP_fmin:
        case OP_frsqrts:
        case OP_uhadd:
        case OP_uqadd:
        case OP_urhadd:
        case OP_uhsub:
        case OP_uqsub:
        case OP_cmhi:
        case OP_cmhs:
        case OP_ushl:
        case OP_uqshl:
        case OP_urshl:
        case OP_uqrshl:
        case OP_umax:
        case OP_umin:
        case OP_uabd:
        case OP_uaba:
        case OP_sub:
        case OP_cmeq:
        case OP_pmul:
        case OP_umaxp:
        case OP_uminp:
        case OP_sqrdmulh:
        case OP_fmaxnmp:
        case OP_faddp:
        case OP_fmul:
        case OP_fcmge:
        case OP_facge:
        case OP_fmaxp:
        case OP_fdiv:
        case OP_fminnmp:
        case OP_fabd:
        case OP_fcmgt:
        case OP_facgt:
        case OP_fminp:
            // all other non-load/store instructions use two source registers when
            // scalar, the vector versions use three
            if (!(instr_num_srcs(ins) == 3)) {
                return false;
            } else {
                num_src = 2;
            }
            break;
        default:
            // instruction is not neon
            return false;
        }
    }

    // num_src contains the number of src slot that acutally hold a register
    for (int i = 0; i < num_src; ++i) {
        reg = opnd_get_reg(instr_get_src(ins, i));
        if (reg >= DR_REG_Q0 && reg <= DR_REG_B31) {
            is_neon = true;
            break;
        }
    }
    if (!is_neon) {
        for (int i = 0; i < instr_num_dsts(ins); ++i) {
            reg = opnd_get_reg(instr_get_dst(ins, i));
            if (reg >= DR_REG_Q0 && reg <= DR_REG_B31) {
                is_neon = true;
                break;
            }
        }
    }

    return is_neon;
}

/*
 * analyzes an instruction list
 */
void
analyze_instr(instr_t *instr, void **user_data)
{
    // save counts for all instr. types
    struct counts_t *counts = malloc(sizeof(struct counts_t));
    // set all counters to zero
    memset(counts, 0, sizeof(struct counts_t));

    int op_code;
    // is load/store instruction
    bool is_load = false;
    bool is_store = false;
    // is arithmetic instruction
    bool is_arith = false;

    instr_t *ins;

    for (ins = instr; ins != NULL; ins = instr_get_next(ins)) {
        // reset
        is_load = false;
        is_store = false;
        is_arith = false;

        // count all instructions
        ++counts->all;

        if (!instr_valid(ins) || instr_is_undefined(ins)) {
            continue;
        }

        op_code = instr_get_opcode(ins);

        // count taken branches
        if (instr_is_cbr(ins)) {
            // count all branching instructions
            ++counts->branching;
            // branching instructions are not simd or load/store instructions, so we
            // skip the rest
            continue;
        }

        // load instructions
        if (instr_reads_memory(ins)) {
            is_load = true;
            ++counts->load;
            if (op_code == OP_ld1 || op_code == OP_ld1r) {
                ++counts->load_linear;
            } else if (op_code == OP_ld2 || op_code == OP_ld3 || op_code == OP_ld4 ||
                       op_code == OP_ld2r || op_code == OP_ld3r || op_code == OP_ld4r) {
                ++counts->load_structured;
            }
        } else if (instr_writes_memory(ins)) { // store instructions
            is_store = true;
            ++counts->store;
            if (op_code == OP_st1) {
                ++counts->store_linear;
            } else if (op_code == OP_st2 || op_code == OP_st3 || op_code == OP_st4) {
                ++counts->store_structured;
            }
        } else
            for (int i = 0; i < 204; ++i) { // arithmetic instructions
                if (op_code == OP_arithmetic[i]) {
                    is_arith = true;
                    ++counts->arith;
                    break;
                }
            }

        // neon instructions
        if (instr_is_neon(ins, is_load, is_store)) {
            // all NEON instructions
            ++counts->neon;

            if (is_arith) {
                ++counts->neon_arith;
            } else if (is_load) {
                ++counts->neon_load;
            } else if (is_store) {
                ++counts->neon_store;
            }
        }
    }

    *user_data = (void *)counts;
}

/*
 * called for branches
 */
static void
at_cbr(app_pc inst_addr, app_pc targ_addr, app_pc fall_addr, int taken, void *bb_addr)
{
    // count taken branches
    if (taken != 0) {
        ++global_counts.taken_branches;
    }
}

/*
 * called for instructions
 */
static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                      bool for_trace, bool translating, void *user_data)
{
    drmgr_disable_auto_predication(drcontext, bb);

    // only count calls for in-app blocks
    if (user_data == NULL) {
        return DR_EMIT_DEFAULT;
    }
    if (!drmgr_is_first_instr(drcontext, instr)) {
        return DR_EMIT_DEFAULT;
    }

    struct counts_t *counts = (struct counts_t *)user_data;

    // increase counts per clean call
    dr_insert_clean_call(
        drcontext, bb, instrlist_first_app(bb), (void *)inscount, false, 13,
        OPND_CREATE_INT(counts->all), OPND_CREATE_INT(counts->arith),
        OPND_CREATE_INT(counts->neon), OPND_CREATE_INT(counts->neon_arith),
        OPND_CREATE_INT(counts->neon_load), OPND_CREATE_INT(counts->neon_store),
        OPND_CREATE_INT(counts->branching), OPND_CREATE_INT(counts->load),
        OPND_CREATE_INT(counts->load_linear), OPND_CREATE_INT(counts->load_structured),
        OPND_CREATE_INT(counts->store), OPND_CREATE_INT(counts->store_linear),
        OPND_CREATE_INT(counts->store_structured));

    // count taken branches
    if (instr_is_cbr(instr)) {
        dr_insert_cbr_instrumentation_ex(drcontext, bb, instr, (void *)at_cbr,
                                         OPND_CREATE_INTPTR(dr_fragment_app_pc(tag)));
    }

    free(counts);

    return DR_EMIT_DEFAULT;
}

/*
 * block analysis
 */
static dr_emit_flags_t
event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating, void **user_data)
{

    if (app_only) {
        // only blocks from the app itself, not shared libraries
        module_data_t *mod = dr_lookup_module(dr_fragment_app_pc(tag));
        if (mod != NULL) {
            bool from_exe = (mod->start == exe_start);
            dr_free_module_data(mod);
            if (!from_exe) {
                *user_data = NULL;
                return DR_EMIT_DEFAULT;
            }
        }
    }

    analyze_instr(instrlist_first(bb), user_data);

    return DR_EMIT_DEFAULT;
}

/*
 * called when the application quits
 */
static void
event_exit(void)
{
    fprintf(stderr,
            "\n=== RESULTS "
            "===========================================================\n");

    fprintf(stderr, "executable: %s\n\n", executable);

    fprintf(stderr, "  libraries that may have been used are:");
    if (app_only) {
        fprintf(stderr, " EXCLUDED \n");
    } else {
        fprintf(stderr, " INCLUDED \n");
    }

    fprintf(stderr, "  Number of ALL instructions:       %lu  \n", global_counts.all);
    fprintf(stderr,
            "__Instr. type____________Count / "
            "Ratio_________________________________\n");
    fprintf(stderr, "  NEON/SIMD              %lu / %Lf  \n", global_counts.neon,
            (global_counts.neon / (long double)global_counts.all));
    fprintf(stderr, "      |___ ARITHMETIC        %lu / %Lf  \n",
            global_counts.neon_arith,
            (global_counts.neon_arith / (long double)global_counts.neon));
    fprintf(stderr, "      |___ LOADING           %lu / %Lf  \n", global_counts.neon_load,
            (global_counts.neon_load / (long double)global_counts.neon));
    fprintf(stderr, "      |___ STORING           %lu / %Lf  \n",
            global_counts.neon_store,
            (global_counts.neon_store / (long double)global_counts.neon));
    fprintf(stderr, "  ARITHMETIC             %lu / %Lf  \n", global_counts.arith,
            (global_counts.arith / (long double)global_counts.all));
    fprintf(stderr, "  BRANCHING              %lu / %Lf  \n", global_counts.branching,
            (global_counts.branching / (long double)global_counts.all));
    fprintf(stderr, "      |___ TAKEN             %lu / %Lf  \n",
            global_counts.taken_branches,
            (global_counts.taken_branches / (long double)global_counts.branching));
    fprintf(stderr, "  LOADING                %lu / %Lf  \n", global_counts.load,
            (global_counts.load / (long double)global_counts.all));
    fprintf(stderr, "      |___ LINEAR            %lu / %Lf  \n",
            global_counts.load_linear,
            (global_counts.load_linear / (long double)global_counts.load));
    fprintf(stderr, "      |___ STRUCTURED        %lu / %Lf  \n",
            global_counts.load_structured,
            (global_counts.load_structured / (long double)global_counts.load));
    fprintf(stderr, "  STORING                %lu / %Lf  \n", global_counts.store,
            (global_counts.store / (long double)global_counts.all));
    fprintf(stderr, "      |___ LINEAR            %lu / %Lf  \n",
            global_counts.store_linear,
            (global_counts.store_linear / (long double)global_counts.store));
    fprintf(stderr, "      |___ STRUCTURED        %lu / %Lf  \n",
            global_counts.store_structured,
            (global_counts.store_structured / (long double)global_counts.store));
    fprintf(stderr, "  OTHER                  %lu / %Lf  \n",
            global_counts.all -
                (global_counts.arith + global_counts.load + global_counts.store +
                 global_counts.branching),
            ((global_counts.all -
              (global_counts.arith + global_counts.load + global_counts.store +
               global_counts.branching)) /
             (long double)global_counts.all));

    fprintf(stderr,
            "=========================================================== RESULTS "
            "===\n");

    // print results in file
    fprintf(result_file, "%s ", executable);
    fprintf(result_file, "%lu ", global_counts.all);
    fprintf(result_file, "%lu ", global_counts.neon);
    fprintf(result_file, "%lu ", global_counts.neon_arith);
    fprintf(result_file, "%lu ", global_counts.neon_load);
    fprintf(result_file, "%lu ", global_counts.neon_store);
    fprintf(result_file, "%lu ", global_counts.arith);
    fprintf(result_file, "%lu ", global_counts.branching);
    fprintf(result_file, "%lu ", global_counts.taken_branches);
    fprintf(result_file, "%lu ", global_counts.load);
    fprintf(result_file, "%lu ", global_counts.load_structured);
    fprintf(result_file, "%lu ", global_counts.load_linear);
    fprintf(result_file, "%lu ", global_counts.store);
    fprintf(result_file, "%lu ", global_counts.store_linear);
    fprintf(result_file, "%lu ", global_counts.store_structured);
    fprintf(result_file, "%lu\n",
            global_counts.all - global_counts.arith - global_counts.load -
                global_counts.store - global_counts.branching);

    fclose(result_file);

    // Unregister event
    if (!drmgr_unregister_bb_insertion_event(event_app_instruction)) {
        DR_ASSERT(false);
    }
    drmgr_exit();
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    // set client name
    dr_set_client_name("DynamoRIO Sample Client 'neoncounter'",
                       "http://dynamorio.org/issues");
    disassemble_set_syntax(DR_DISASM_ARM);

    // initialize
    if (!drmgr_init()) {
        DR_ASSERT(false);
    }

    // Remember the used library
    executable = dr_module_preferred_name(dr_get_main_module());
    // Open the output file at the default location. All new data is added at the end
    result_file = fopen("nc_output.txt", "a");

    // argv[0] is the name of the used library
    for (int args = 1; args < argc; ++args) {
        if (strcmp(argv[args], "--help") == 0) {
            // print help
            printf("\x1b[32m\nUsage:\tdrrun -c /path/to/libneoncounter.so [OPTIONS] "
                   "-- [APP Command]\n");
            printf("Options:\n\t--help         :\tdisplay "
                   "help\n\t-output /path/to/file\n\t-count-app-only:\tcount only "
                   "instructions "
                   "that are part of the\n\t\t                app itself, not those "
                   "of shared libraries etc.\x1b[0m\n\n");
        } else if (strncmp(argv[args], "-output", 7) == 0) {
            if (args + 1 < argc) {
                result_file = fopen(argv[args + 1], "a");
                ++args;
            } else {
                printf("Please enter a path to the output file. Choosing default path "
                       "./nc_output.txt\n");
            }
        } else if (strcmp(argv[args], "-count-app-only") == 0) {
            app_only = true;
            // get main module address
            module_data_t *exe = dr_get_main_module();
            if (exe != NULL) {
                exe_start = exe->start;
            }
            dr_free_module_data(exe);
        } else {
            printf("\n   Did not recognize this option \"%s\"  --  try \"--help\" "
                   "for help.\n",
                   argv[args]);
        }
    }

    // Test if file is empty and write a header into empty files
    fseek(result_file, 0, SEEK_END);
    if (ftell(result_file) == 0) {
        fprintf(result_file,
                "exe all-instr simd-instr simd-arith simd-load simd-store "
                " arith-instr branch-instr branch-taken load-instr load-struct "
                "load-lin store-instr store-lin store-struct other\n");
    }

    // register event:
    dr_register_exit_event(event_exit);
    if (!drmgr_register_bb_instrumentation_event(NULL, event_app_instruction, NULL)) {
        DR_ASSERT(false);
    }
    drmgr_register_bb_instrumentation_event(event_bb_analysis, event_app_instruction,
                                            NULL);

    // tell which client is running
    dr_log(NULL, DR_LOG_ALL, 1, "Client 'NEONCOUNTER' initializing\n");
}
