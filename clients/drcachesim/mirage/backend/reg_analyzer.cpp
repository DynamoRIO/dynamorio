#include "reg_analyzer.h"

RegAnalyzer::RegAnalyzer() {
    for (int i = 0; i < DR_NUM_GPR_REGS; i++) {
        gp_reg_file[i] = 1 << i;
    }
}

RegAnalyzer::~RegAnalyzer() {

}

void RegAnalyzer::replay(mir_insn_list_t *insn_list) {
    struct list_elem* e;
    for (e = list_begin(insn_list); e != list_end(insn_list); e = list_next(e)) {
        mir_insn_t* insn = list_entry(e, mir_insn_t, elem);
        step(insn);
    }
}

void RegAnalyzer::report() {
    printf("--> Memory Access Report -->\n");
    for (auto it = mem_access_pattern.begin(); it != mem_access_pattern.end(); it++) {
        if (it->first == 0) {
            printf("constant memory access: %lu\n", it->second);
        } else {
            printf("variable memory access for 0x%lx : %lu times\n", it->first, it->second);
        }
    }
    printf("--> Register State Report -->\n");
    for (int i = 0; i < DR_NUM_GPR_REGS; i++) {
        printf("register %d: %lx\n", i, gp_reg_file[i]);
    }
}

uint64_t RegAnalyzer::read_mem(mir_insn_t *insn) {
    access_mem(insn);
    return 0x80000000; // signed bit marks the contamination of register
}

void RegAnalyzer::write_mem(mir_insn_t *insn) {
    access_mem(insn);
}

void RegAnalyzer::access_mem(mir_insn_t *insn) {
    if (is_const_addr(insn)) {
        inc_mem_access_count(0);
    } else {
        inc_mem_access_count(get_val_from_opnd(insn->opnd0) | get_val_from_opnd(insn->opnd1));
    }
}

bool RegAnalyzer::is_const_addr(mir_insn_t *insn) {
    return (insn->opnd0.type == MIR_OPND_IMM || 
           (insn->opnd0.type == MIR_OPND_REG && insn->opnd0.value.reg == REG_NULL)) && 
           (insn->opnd1.type == MIR_OPND_IMM || 
           (insn->opnd1.type == MIR_OPND_REG && insn->opnd1.value.reg == REG_NULL));
}

void RegAnalyzer::step(mir_insn_t *insn) {
    uint64_t src0_val = get_val_from_opnd(insn->opnd0);
    uint64_t src1_val = get_val_from_opnd(insn->opnd1);
    uint64_t dst_val;
    switch (insn->op) {
        case MIR_OP_NULL:
            break;
        case MIR_OP_MOV:
            set_val_to_opnd(insn->dst, src0_val);
        // all arithmetic operations are treated the same
        case MIR_OP_ADD:
        case MIR_OP_SUB:
        case MIR_OP_MUL:
        case MIR_OP_DIV:
        case MIR_OP_REM:
        case MIR_OP_AND:
        case MIR_OP_OR:
        case MIR_OP_XOR:
        case MIR_OP_SHL:
        case MIR_OP_SHR:
            dst_val = src0_val | src1_val;
            set_val_to_opnd(insn->dst, dst_val);
            break;
        case MIR_OP_LD8:
        case MIR_OP_LD16:
        case MIR_OP_LD32:
        case MIR_OP_LD64:
            dst_val = read_mem(insn);
            // set_val_to_opnd(insn->dst, dst_val);
            break;
        case MIR_OP_ST8:
        case MIR_OP_ST16:
        case MIR_OP_ST32:
        case MIR_OP_ST64:
            write_mem(insn);
            break;
        default:
            break;
    }
}

uint64_t RegAnalyzer::get_reg_val(reg_id_t reg) {
    if (reg == REG_NULL) {
        return 0;
    } else if (IS_DR_REG_GPR(reg)) {
        return gp_reg_file[GET_DR_REG_GPR_NUM(reg)];
    } else if (IS_DR_REG_64(reg)) {
        return gp_reg_file[GET_DR_REG_64_NUM(reg)];
    } else if (IS_DR_REG_32(reg)) {
        return (uint32_t)gp_reg_file[GET_DR_REG_32_NUM(reg)];
    } else if (IS_DR_REG_16(reg)) {
        return (uint16_t)gp_reg_file[GET_DR_REG_16_NUM(reg)];
    } else if (IS_DR_REG_8(reg)) {
        return (uint8_t)gp_reg_file[GET_DR_REG_8_NUM(reg)];
    } else if (IS_TMP_REG(reg)) {
        return tmp_reg_file[GET_TMP_REG_NUM(reg)];
    } else {
        return 0;
    }
}

uint64_t RegAnalyzer::get_val_from_opnd(mir_opnd_t opnd) {
    if (opnd.type == MIR_OPND_REG) {
        return get_reg_val(opnd.value.reg);
    } else if (opnd.type == MIR_OPND_IMM) {
        return 0; // constants do not contribute
    }
    return 0;
}

void RegAnalyzer::set_val_to_opnd(mir_opnd_t opnd, uint64_t value) {
    if (opnd.type == MIR_OPND_REG) {
        //debug
        // if (opnd.value.reg == REG_XSP && value != 0x10) {
        //     printf("setting reg xsp to %lx\n", value);
        //     volatile uint64_t xsp = get_reg_val(REG_XSP);
        //     printf("xsp: %lx\n", xsp);
        // }
        if (opnd.value.reg == REG_NULL) {
            return;
        } else if (IS_DR_REG_GPR(opnd.value.reg)) {
            gp_reg_file[GET_DR_REG_GPR_NUM(opnd.value.reg)] = value;
        } else if (IS_DR_REG_64(opnd.value.reg)) {
            gp_reg_file[GET_DR_REG_64_NUM(opnd.value.reg)] = value;
        } else if (IS_DR_REG_32(opnd.value.reg)) {
            gp_reg_file[GET_DR_REG_32_NUM(opnd.value.reg)] = (uint32_t)value;
        } else if (IS_DR_REG_16(opnd.value.reg)) {
            gp_reg_file[GET_DR_REG_16_NUM(opnd.value.reg)] = (uint16_t)value;
        } else if (IS_DR_REG_8(opnd.value.reg)) {
            gp_reg_file[GET_DR_REG_8_NUM(opnd.value.reg)] = (uint8_t)value;
        } else if (IS_TMP_REG(opnd.value.reg)) {
            tmp_reg_file[GET_TMP_REG_NUM(opnd.value.reg)] = value;
        }
    } else if (opnd.type == MIR_OPND_IMM) {
        assert(false && "attempted to set imm value");
    }
}

void RegAnalyzer::inc_mem_access_count(uint64_t addr) {
    if (mem_access_pattern.find(addr) == mem_access_pattern.end()) {
        mem_access_pattern[addr] = 1;
    } else {
        mem_access_pattern[addr]++;
    }
}
