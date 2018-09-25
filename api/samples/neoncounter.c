/* ******************************************************************************
 * Copyright (c) 2014-2018 Google, Inc.  All rights reserved.
 * Copyright (c) 2011 Massachusetts Institute of Technology  All rights reserved.
 * Copyright (c) 2008 VMware, Inc.  All rights reserved.
 * ******************************************************************************/
 
/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
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
  * Counter for NEON/SIMD-instructions on AARCH64, based on the inscount.cpp sample
  * @author Michael Nunhofer
  * University of Regensburg, Germany (QPACE 4, SFB TRR-55)
  */

#include "dr_api.h"
#include "drmgr.h"
#include "dr_tools.h"

#include <stdio.h>
#include "string.h"

#ifdef DEBUG
    #define IS_DEBUG true
#else
	#define IS_DEBUG false
#endif

//Remember the used command
const char *cmd;

//ARM-Architecture Reference Book p. 185ff
//Arithmetic instructions:
const char* OP_arithmetic[204] = {"add",
                                   "adds",
                                   "sub",
                                   "subs",
                                   "cmp",
                                   "cmn",
                                   "neg",
                                   "negs",
                                   "abs",
                                   "adc",
                                   "adcs",
                                   "sbc",
                                   "sbcs",
                                   "ngc",
                                   "ngcs",
                                   "madd",
                                   "msub",
                                   "mneg",
                                   "mul",
                                   "smaddl",
                                   "smsubl",
                                   "smnegl",
                                   "smull",
                                   "smulh",
                                   "umaddl",
                                   "umsubl",
                                   "umnegl",
                                   "umull",
                                   "umulh",
                                   "sdiv",
                                   "udiv",
                                   "fmadd",
                                   "fmsub",
                                   "fnmadd",
                                   "fnmsub",
                                   "fabs",
                                   "fneg",
                                   "fsqrt",
                                   "fadd",
                                   "fdiv",
                                   "fmul",
                                   "fnmul",
                                   "fsub",
                                   "fmax",
                                   "fmaxnm",
                                   "fmin",
                                   "fminnm",
                                   "fabd",
                                   "fmla",
                                   "fmlal",
                                   "fmlal2",
                                   "fmls",
                                   "fmlsl",
                                   "fmlsl2",
                                   "fmulx",
                                   "frecps",
                                   "frsqrts",
                                   "mla",
                                   "mls",
                                   "mov",
                                   "mvn",
                                   "tst",
                                   "pmul",
                                   "saba",
                                   "sabd",
                                   "shadd",
                                   "shsub",
                                   "smax",
                                   "smin",
                                   "sqadd",
                                   "sqdmulh",
                                   "sqrshl",
                                   "sqrdmlah",
                                   "sqrdmlsh",
                                   "sqrdmulh",
                                   "sqshl",
                                   "sqsub",
                                   "srhadd",
                                   "srshl",
                                   "sshl",
                                   "uaba",
                                   "uabd",
                                   "uhadd",
                                   "uhsub",
                                   "umax",
                                   "umin",
                                   "uqadd",
                                   "uqrshl",
                                   "uqshl",
                                   "uqsub",
                                   "urhadd",
                                   "urshl",
                                   "ushl",
                                   "addhn",
                                   "addhn2",
                                   "pmull",
                                   "pmull2",
                                   "raddhn",
                                   "raddhn2",
                                   "rsubhn",
                                   "rsubhn2",
                                   "sabal",
                                   "sabal2",
                                   "sabdl",
                                   "sabdl2",
                                   "saddl",
                                   "saddl2",
                                   "saddw",
                                   "saddw2",
                                   "smlal",
                                   "smlal2",
                                   "smlsl",
                                   "smlsl2",
                                   "smull",
                                   "smull2",
                                   "sqdmlal"
                                   "sqdmlal2",
                                   "sqdmlsl",
                                   "sqdmlsl2",
                                   "sqdmull",
                                   "sqdmull2",
                                   "ssubl",
                                   "ssubl2",
                                   "ssubw",
                                   "ssubw2",
                                   "subhn",
                                   "subhn2",
                                   "uabal",
                                   "uabal2",
                                   "uabdl",
                                   "uabdl2",
                                   "uaddl",
                                   "uaddl2",
                                   "uaddw",
                                   "uaddw2",
                                   "umlal",
                                   "umlal2",
                                   "umlsl",
                                   "umlsl2",
                                   "umull",
                                   "umull2",
                                   "usubl",
                                   "usubl2",
                                   "usubw",
                                   "usubw2",
                                   "cls",
                                   "clz",
                                   "cnt",
                                   "fcvtl",
                                   "fcvtl2",
                                   "fcvtn",
                                   "fcvtn2",
                                   "fcvtxn",
                                   "fcvtxn2",
                                   "frecpe",
                                   "frecpx",
                                   "frinta",
                                   "frinti",
                                   "frintm",
                                   "frintn",
                                   "frintp",
                                   "frintx",
                                   "frintz",
                                   "frsqrte",
                                   "mvn",
                                   "not",
                                   "rbit",
                                   "rev16",
                                   "rev32",
                                   "rev64",
                                   "sadalp",
                                   "saddlp",
                                   "sqabs",
                                   "sqneg",
                                   "sqxtn",
                                   "sqxtn2",
                                   "sqxtun",
                                   "sqxtun2",
                                   "suqadd",
                                   "sxtl",
                                   "sxtl2",
                                   "uadalp",
                                   "uaddlp",
                                   "uqxtn",
                                   "uqxtn2",
                                   "urecpe",
                                   "ursqrte",
                                   "usqadd",
                                   "uxtl",
                                   "uxtl2",
                                   "xtn",
                                   "xtn2",
                                   "faddp",
                                   "fmaxnmp",
                                   "fmaxp",
                                   "fminnmp",
                                   "fminp",
                                   "smaxp",
                                   "sminp",
                                   "umaxp",
                                   "uminp",
                                   "sdot",
                                   "udot",
                                   "fcadd",
                                   "fcmla"};


//ARM-Architecture Reference Book p. 170ff
//Load SIMD-Registers: ldr, ldur, ldp, ldnp
const char* OP_loading[45] = {"ldr",
                               "ldrb",
                               "ldrsb",
                               "ldrh",
                               "ldrsh",
                               "ldrsw",
                               "ldur",
                               "ldurb",
                               "ldursb",
                               "ldurh",
                               "ldursh",
                               "ldursw",
                               "ldp",
                               "ldpsw",
                               "ldnp",
                               "ldtr",
                               "ldtrb",
                               "ldtrsb",
                               "ldtrsh",
                               "ldtrsw",
                               "ldxr",
                               "ldxrb",
                               "ldxrh",
                               "ldxp",
                               "ldapr",
                               "ldaprb",
                               "ldaprh",
                               "ldar",
                               "ldarb",
                               "ldarh",
                               "ldaxr",
                               "ldaxrb",
                               "ldaxrh",
                               "ldaxp",
                               "ldlarb",
                               "ldlarh",
                               "ldlar",
                               "ld1", //linear   -> NEON
                               "ld2", //structured
                               "ld3",
                               "ld4",
                               "ld1r", //linear
                               "ld2r", //structured
                               "ld3r",
                               "ld4r" }; //---      NEON <-

//Store SIMD-Registers: str, stur, stp, stnp
const char* OP_storing[28] = {"str",
                               "strb",
                               "strh",
                               "stur",
                               "sturb",
                               "sturh",
                               "stp",
                               "stnp",
                               "sttr",
                               "sttrb",
                               "sttrh",
                               "stxr",
                               "stxrb",
                               "stxrh"
                               "stxp",
                               "stlr",
                               "stlrb",
                               "stlrh",
                               "stlxr",
                               "stlxrb",
                               "stlxrh",
                               "stlxp",
                               "stllrb",
                               "stllrh",
                               "stllr",
                               "st1", //linear  -> NEON
                               "st2", //structured
                               "st3",
                               "st4"}; //---       NEON <-


//Save counts
static long long unsigned int count_all = 0;
static long long unsigned int count_arith = 0;

static long long unsigned int count_simd = 0;
static long long unsigned int count_simd_arith = 0;
static long long unsigned int count_simd_load = 0;
static long long unsigned int count_simd_store = 0;
static long long unsigned int count_branching = 0;
static long long unsigned int count_taken_branches = 0;

static long long unsigned int count_load = 0;
static long long unsigned int count_load_linear = 0;
static long long unsigned int count_load_structured = 0;

static long long unsigned int count_store = 0;
static long long unsigned int count_store_linear = 0;
static long long unsigned int count_store_structured = 0;

static long long unsigned int count_sve = 0;
static long long unsigned int count_sve_arith = 0;
static long long unsigned int count_sve_load = 0;
static long long unsigned int count_sve_store = 0;


//use this function for clean calls
static void inscount(uint num_instrs, uint num_arith, uint num_simd, uint num_simd_arith,
                     uint num_simd_load, uint num_simd_store, uint num_branching,
                     uint num_load, uint num_load_linear, uint num_load_structured,
                     uint num_store, uint num_store_linear, uint num_store_structured,
                     uint num_sve, uint num_sve_arith, uint num_sve_load, uint num_sve_store) {

    count_all += num_instrs;
    count_arith += num_arith;
    count_simd += num_simd;
    count_simd_arith += num_simd_arith;
    count_simd_load += num_simd_load;
    count_simd_store += num_simd_store;
    count_branching += num_branching;
    count_load += num_load;
    count_load_linear += num_load_linear;
    count_load_structured += num_load_structured;
    count_store += num_store;
    count_store_linear += num_store_linear;
    count_store_structured += num_store_structured;
    count_sve += num_sve;
    count_sve_arith += num_sve_arith;
    count_sve_load += num_sve_load;
    count_sve_store += num_sve_store;
}


//in DEBUG mode all processed instructions are written in this file
FILE *debug;

//remembers if only the app or the app and all shared libraries should be analyzed
static bool app_only = false;
//application module
static app_pc exe_start;

//saves the name of the executed app
static const char* executable;


/*
 * analyzes an instruction list
 */
void analyze_instr(instr_t *instr, void **user_data) {
	//save counts for all instr., simd instr. and branching instr.
    uint *counts = malloc(sizeof(uint) * 17);
	//all instrs.
	counts[0] = 0;
    //all arithmethic count
	counts[1] = 0;

    //neon
	counts[2] = 0;
    //neon arithmetic
	counts[3] = 0;
    //neon load
	counts[4] = 0;
    //neon store
    counts[5] = 0;
    //branching
    counts[6] = 0;

    //load
    counts[7] = 0;
    //load linear
    counts[8] = 0;
    //load structured
    counts[9] = 0;

    //store
    counts[10] = 0;
    //store linear
    counts[11] = 0;
    //store structured
    counts[12] = 0;

    //sve
    counts[13] = 0;
    //sve arithmetic
    counts[14] = 0;
    //sve load
    counts[15] = 0;
    //sve store
    counts[16] = 0;
	
    const char* op_name;
    int op_code;
    //remember if instruction is load/store instruction
    bool is_load = false;
    bool is_store = false;
    //remember if instruction is arithmetic instruction
    bool is_ar = false;
    bool is_neon = false;
    bool is_sve = false;

    instr_t *ins;

    for (ins = instr; ins != NULL; ins = instr_get_next(ins)) {
        //reset
        is_load = false;
        is_store = false;
        is_ar = false;
        is_neon = false;

        if (drmgr_is_emulation_end(ins)) {
            is_sve = false;
            continue;
        }
        if (is_sve) {
            /* An SVE instruction is emulated with general AArch64
             * instructions, but we only want to count it once. */
            continue;
        }
        if (drmgr_is_emulation_start(ins)) {
            //
            is_sve = true;
            /* Data about the emulated instruction can be extracted from the
             * start label using drmgr_get_emulated_instr_data().
             */
            emulated_instr_t emulated;
            drmgr_get_emulated_instr_data(ins, &emulated);
            printf("SVE: %p\t", emulated.pc);
            int *sveinstr;
            sveinstr = ((int *)instr_get_raw_bits(emulated.instr));
            printf("0x%08x\n", *sveinstr);
        }


    	//count all instructions
    	counts[0]++;

        if(!instr_valid(ins) || instr_is_undefined(ins)) {
    		continue;
    	}

		//get the opcode
        op_code = instr_get_opcode(ins);
        op_name = decode_opcode_name(op_code);
		   
		if(IS_DEBUG) {
			fprintf(debug, "%s", op_name);
		}

        //assure that string is not null
        if(strlen(op_name) < 1) {
            continue;
        }
				
        //count taken branches
        if(instr_is_cbr(ins)) {
            //count all branching instructions
            counts[6]++;
            //branching instructions are no simd or load/store instructions, so we skip the rest
            if(IS_DEBUG) {
                fprintf(debug, "\n");
            }
            continue;
        }

        //loading instructions
        if(op_name[0] == 'l') {
			for(int i = 0; i < 45; i++) {
				if(strcmp(op_name, OP_loading[i]) == 0) {
                    is_load = true;
                    counts[7]++;
                    if(i == 37 || i == 41) { //ld1, ld1r
                        counts[8]++;
                    } else if((i > 37 && i < 41) || (i > 41 && i < 45)) { //ld2, ld3, ld4, ld2r, ld3r, ld4r
                        counts[9]++;
                    }
					break;
				}
			}
		}

        //storing instructions
        if(op_name[0] == 's') {
			for(int i = 0; i < 28; i++) {
				if(strcmp(op_name, OP_storing[i]) == 0) {
                    is_store = true;
                    counts[10]++;
                    if(i == 24) { //st1
                        counts[11]++;
                    } else if(i > 24) { //st2, st3, st4
                        counts[12]++;
                    }
					break;
				}
			}
		}

        //arithmetic instructions
        for(int i = 0; i < 204; i++) {
            if(strcmp(op_name, OP_arithmetic[i]) == 0) {
                is_ar = true;
                counts[1]++;
                break;
            }
        }


        //check for VECTOR Instructions --------------------------------------------------
        //SIMD-Registers: Q0-Q31, D0-D31, S0-S31, H0-H31, B0-B31 (dr_ir_opnd.h); SVE: Z0-Z31
        reg_id_t reg;

        if(is_sve) {
            counts[13]++;

            //arithmetic instructions
            if(is_ar) {
                counts[14]++;
            } else
                if(is_load) {  //load/store instructions
                    counts[15]++;
                } else
                    if(is_store) {
                        counts[16]++;
                    }
                    
            if(IS_DEBUG) {
                fprintf(debug, " <------------------------ SVE/SIMD");
            }
        } else {
            //NEON and FP use the same registers
            //arithmetic NEON Instr. store their width in an additional src-slot
            //this variable holds the number of src slots that acutally point to a register
            int num_src = 0;

            //potential neon load/store instructions should reference a memory block larger than four doubles (this is an estimate)
            if(is_load || is_store) {
                if(instr_memory_reference_size(ins) < sizeof(double) * 4) {
                    if(IS_DEBUG) {
                        fprintf(debug, "\n");
                    }
                    continue;
                } else {
                    num_src = instr_num_srcs(ins);
                }
            } else {
                switch (op_code) {
                    case OP_fnmadd:
                    case OP_fnmsub:
                    case OP_fmadd:
                    case OP_fmsub:
                    case OP_fmla:
                    case OP_mla:
                        //these instructions have three sources when scalar
                        if(!(instr_num_srcs(ins) == 4)) {
                            if(IS_DEBUG) {
                                fprintf(debug, "\n");
                            }
                            continue;
                        } else {
                            num_src = 3;
                        }
                        break;
                    case OP_fmlal:
                    case OP_fmlal2:
                    case OP_fmlsl:
                    case OP_fmlsl2:
                        //these are always vector instructions
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
                        //all other non-load/store instructions use two source registers when scalar, the vector versions use three
                        if(!(instr_num_srcs(ins) == 3)) {
                            if(IS_DEBUG) {
                                fprintf(debug, "\n");
                            }
                            continue;
                        } else {
                            num_src = 2;
                        }
                        break;
                    default:
                        //instruction is not neon
                        if(IS_DEBUG) {
                            fprintf(debug, "\n");
                        }
                        continue;
                }
            }

            //potential neon load/store instructions should reference a memory block larger than four doubles (this is an estimate)
            if((is_load || is_store) && instr_memory_reference_size(ins) < sizeof(double) * 4) {
                if(IS_DEBUG) {
                    fprintf(debug, "\n");
                }
                continue;
            } else {
                num_src = instr_num_srcs(ins);
            }

            //num_src contains the number of src slot that acutally hold a register
            for(int i = 0; i < num_src; i++) {
                reg = opnd_get_reg(instr_get_src(ins, i));
                if(reg >= DR_REG_Q0 && reg <= DR_REG_B31) {
                    is_neon = true;
                    break;
                }
            }
            if(!is_neon) {
                for(int i = 0; i < instr_num_dsts(ins); i++) {
                    reg = opnd_get_reg(instr_get_dst(ins, i));
                    if(reg >= DR_REG_Q0 && reg <= DR_REG_B31) {
                        is_neon = true;
                        break;
                    }
                }
            }

            if(is_neon) {
                //all NEON instructions
                counts[2]++;

                //arithmetic NEON instruction
                if(is_ar) {
                    counts[3]++;
                } else if(is_load) {
                    counts[4]++;
                } else if(is_store) {
                    counts[5]++;
                }

                if(IS_DEBUG) {
                    fprintf(debug, " <------------------------ NEON/SIMD");
                }
            }
        }

        if(IS_DEBUG) {
            fprintf(debug, "\n");
        }
    }

    *user_data = (void*) counts;
}


/*
 * called for branches
 */
static void at_cbr(app_pc inst_addr, app_pc targ_addr, app_pc fall_addr, int taken, void *bb_addr) {
    //count taken branches
    if(taken != 0) {
        count_taken_branches++;
    }
}


/*
 * called for instructions
 */
static dr_emit_flags_t event_app_instruction(void *drcontext,
											 void *tag,
											 instrlist_t *bb,
                                             instr_t *instr,
											 bool for_trace,
                                             bool translating,
											 void *user_data) {
    drmgr_disable_auto_predication(drcontext, bb);
	
	//only count calls for in-app blocks
	if (user_data == NULL) {
        return DR_EMIT_DEFAULT;
    }
    if (!drmgr_is_first_instr(drcontext, instr)) {
    	return DR_EMIT_DEFAULT;
    }

    uint *counts = (uint*) user_data;

    //increase counts per clean call
    dr_insert_clean_call(drcontext, bb, instrlist_first_app(bb), (void*)inscount, false, 17, OPND_CREATE_INT(counts[0]),
            OPND_CREATE_INT(counts[1]), OPND_CREATE_INT(counts[2]), OPND_CREATE_INT(counts[3]), OPND_CREATE_INT(counts[4]),
            OPND_CREATE_INT(counts[5]), OPND_CREATE_INT(counts[6]), OPND_CREATE_INT(counts[7]), OPND_CREATE_INT(counts[8]),
            OPND_CREATE_INT(counts[9]), OPND_CREATE_INT(counts[10]), OPND_CREATE_INT(counts[11]), OPND_CREATE_INT(counts[12]),
            OPND_CREATE_INT(counts[13]), OPND_CREATE_INT(counts[14]), OPND_CREATE_INT(counts[15]), OPND_CREATE_INT(counts[16]));

    //count taken branches
    if(instr_is_cbr(instr)) {
        dr_insert_cbr_instrumentation_ex(drcontext, bb, instr, (void *)at_cbr, OPND_CREATE_INTPTR(dr_fragment_app_pc(tag)));
    }

    return DR_EMIT_DEFAULT;
}

/*
 * block analysis
 */
static dr_emit_flags_t event_bb_analysis(void *drcontext,
										 void *tag,
										 instrlist_t *bb,
                  						 bool for_trace,
                  						 bool translating,
                  						 void **user_data) {
           
    if(app_only) {
		//only blocks from the app itself, not shared libraries
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
static void event_exit(void) {
    printf("\n=== RESULTS ===========================================================\n");

    printf("library: %s\n", cmd);
    printf("executable: %s\n\n", executable);

    printf("  libraries that may have been used are:");
    if(app_only) {
        printf(" EXCLUDED \n");
    } else {
        printf(" INCLUDED \n");
    }

    printf("  Number of ALL instructions:       %llu  \n", count_all);
    printf("__Instr. type____________Count / Ratio_________________________________\n");
    printf("  NEON/SIMD              %llu / %f  \n",
           count_simd, (count_simd/(float)count_all));
    printf("      |___ ARITHMETIC        %llu / %f  \n",
           count_simd_arith, (count_simd_arith/(float)count_simd));
    printf("      |___ LOADING           %llu / %f  \n",
           count_simd_load, (count_simd_load/(float)count_simd));
    printf("      |___ STORING           %llu / %f  \n",
           count_simd_store, (count_simd_store/(float)count_simd));
    printf("  SVE/SIMD               %llu / %f  \n",
           count_sve, (count_sve/(float)count_all));
    printf("      |___ ARITHMETIC        %llu / %f  \n",
           count_sve_arith, (count_sve_arith/(float)count_sve));
    printf("      |___ LOADING           %llu / %f  \n",
           count_sve_load, (count_sve_load/(float)count_sve));
    printf("      |___ STORING           %llu / %f  \n",
           count_sve_store, (count_sve_store/(float)count_sve));
    printf("  ARITHMETIC             %llu / %f  \n",
           count_arith, (count_arith/(float)count_all));
    printf("  BRANCHING              %llu / %f  \n",
           count_branching, (count_branching/(float)count_all));
    printf("      |___ TAKEN             %llu / %f  \n",
           count_taken_branches, (count_taken_branches/(float)count_branching));
    printf("  LOADING                %llu / %f  \n",
           count_load, (count_load/(float)count_all));
    printf("      |___ LINEAR            %llu / %f  \n",
           count_load_linear, (count_load_linear/(float)count_load));
    printf("      |___ STRUCTURED        %llu / %f  \n",
           count_load_structured, (count_load_structured/(float)count_load));
    printf("  STORING                %llu / %f  \n",
           count_store, (count_store/(float)count_all));
    printf("      |___ LINEAR            %llu / %f  \n",
           count_store_linear, (count_store_linear/(float)count_store));
    printf("      |___ STRUCTURED        %llu / %f  \n",
           count_store_structured, (count_store_structured/(float)count_store));
    printf("  OTHER                  %llu / %f  \n",
           count_all-count_arith-count_load-count_store-count_branching, ((count_all-count_arith-count_load-count_store-count_branching)/(float)count_all));


    printf("=========================================================== RESULTS ===\n");

	if(IS_DEBUG) {
		fclose(debug);
    } else {
        //print results in file
        fprintf(debug, "%s ", executable);
        fprintf(debug, "%llu ", count_all);
        fprintf(debug, "%llu ", count_simd);
        fprintf(debug, "%llu ", count_simd_arith);
        fprintf(debug, "%llu ", count_simd_load);
        fprintf(debug, "%llu ", count_simd_store);
        fprintf(debug, "%llu ", count_sve);
        fprintf(debug, "%llu ", count_sve_arith);
        fprintf(debug, "%llu ", count_sve_load);
        fprintf(debug, "%llu ", count_sve_store);
        fprintf(debug, "%llu ", count_arith);
        fprintf(debug, "%llu ", count_branching);
        fprintf(debug, "%llu ", count_taken_branches);
        fprintf(debug, "%llu ", count_load);
        fprintf(debug, "%llu ", count_load_structured);
        fprintf(debug, "%llu ", count_load_linear);
        fprintf(debug, "%llu ", count_store);
        fprintf(debug, "%llu ", count_store_linear);
        fprintf(debug, "%llu ", count_store_structured);
        fprintf(debug, "%llu\n", count_all - count_arith - count_load - count_store - count_branching);

        fclose(debug);
    }

	//Unregister event
	if (!drmgr_unregister_bb_insertion_event(event_app_instruction)) {
        DR_ASSERT(false);
	}
    drmgr_exit();
}


/*
 * main method
 */
DR_EXPORT void dr_client_main(client_id_t id, int argc, const char *argv[]) {
	//set client name
    dr_set_client_name("NEON/SIMD Instruction Counter", "Michael Nunhofer");
    disassemble_set_syntax(DR_DISASM_ARM);
	
	if(IS_DEBUG) {
		debug = fopen("debug.txt", "w");
    } else {
        debug = fopen("nc_output.txt", "a");
        //Test if file is empty and write a header into empty files
        fseek(debug, 0, SEEK_END);
        if(ftell(debug) == 0) {
            fprintf(debug, "exe all-instr simd-instr simd-arith simd-load simd-store sve-instr sve-arith sve-load sve-store"
                           " arith-instr branch-instr branch-taken load-instr load-struct load-lin store-instr store-lin store-struct other\n");
        }
    }
	
	//initialize
	if (!drmgr_init()) {
        DR_ASSERT(false);
    }

    //Remember the used command
    cmd = argv[0];
    executable = dr_module_preferred_name(dr_get_main_module());
    
    //argv[0] is the application name
    for(int args = 1; args < argc; args++) {
    	if(strcmp(argv[args], "--help") == 0) {
    		//print help
    		printf("\x1b[32m\nUsage:\tdrrun -c /path/to/libNeonCounter.so [OPTIONS] -- [APP Command]\n");
            printf("Options:\n\t--help         :\tdisplay help\n\t-count-app-only:\tcount only instructions "
                   "that are part of the\n\t\t                app itself, not those of shared libraries etc.\x1b[0m\n\n");
        } else
		if(strcmp(argv[args], "-count-app-only") == 0) {
			app_only = true;
			//get main module address
			module_data_t *exe = dr_get_main_module();
			if (exe != NULL) {
				exe_start = exe->start;
			}
			dr_free_module_data(exe);
        } else {
            printf("\n   Did not recognize this option \"%s\"  --  try \"--help\" for help.\n", argv[args]);
        }
	}

	//register Event:
	dr_register_exit_event(event_exit);
	if (!drmgr_register_bb_instrumentation_event(NULL, event_app_instruction, NULL)) {
        DR_ASSERT(false);
	}
	drmgr_register_bb_instrumentation_event(event_bb_analysis, event_app_instruction, NULL);

	//tell which client is running
    dr_log(NULL, __WALL, 1, "Client 'NEON/SIMD Analyzer' initializing\n");
}
