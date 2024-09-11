#include "replayer.h"

Replayer::Replayer(InitStrategy init_strategy) {
    this->init_strategy = init_strategy;
    if (init_strategy == INIT_STRATEGY_ZERO) {
        for (int i = 0; i < DR_NUM_GPR_REGS; i++) {
            gp_reg_file[i] = 0;
        }
    } else if (init_strategy == INIT_STRATEGY_RANDOM) {
        for (int i = 0; i < DR_NUM_GPR_REGS; i++) {
            gp_reg_file[i] = rand() & 0xffffffff;
        }
    }
    log_file = fopen("replayer.log", "w");
}

Replayer::~Replayer() {
    shadow_mem.clear();
}

void Replayer::replay(mir_insn_list_t *insn_list) {
    struct list_elem* e;
    for (e = list_begin(insn_list); e != list_end(insn_list); e = list_next(e)) {
        mir_insn_t* insn = list_entry(e, mir_insn_t, elem);
        step(insn);
    }
}

uint64_t Replayer::read_mem(uintptr_t addr, size_t size) {
    uint64_t value = 0;
    for (size_t i = 0; i < size; i++) {
        auto it = shadow_mem.find(addr + i);
        if (it != shadow_mem.end()) {
            value |= (uint64_t(it->second) << (i * 8));
        } else {
            if (this->init_strategy == INIT_STRATEGY_RANDOM) {
                value |= (uint64_t(rand() & 0xff) << (i * 8));
            } else if (this->init_strategy == INIT_STRATEGY_ZERO) {
                // do nothing
            }
        }
    }
    fprintf(log_file, "read: addr = %lx, size = %lx\n", addr, size);
    return value;
}

void Replayer::write_mem(uintptr_t addr, uint64_t value, size_t size) {
    for (size_t i = 0; i < size; i++) {
        shadow_mem[addr + i] = (value >> (i * 8)) & 0xff;
    }
    fprintf(log_file, "write: addr = %lx, size = %lx\n", addr, size);
}

void Replayer::step(mir_insn_t *insn) {
    uint64_t src0_val = get_val_from_opnd(insn->opnd0);
    uint64_t src1_val = get_val_from_opnd(insn->opnd1);
    uint64_t dst_val;
    switch (insn->op) {
        case MIR_OP_NULL:
            break;
        case MIR_OP_MOV:
            // ensure src1 is unvalued
            assert(src1_val == 0);
            set_val_to_opnd(insn->dst, src0_val);
            break;
        case MIR_OP_ADD:
            dst_val = src0_val + src1_val;
            set_val_to_opnd(insn->dst, dst_val);
            break;
        case MIR_OP_SUB:
            dst_val = src0_val - src1_val;
            set_val_to_opnd(insn->dst, dst_val);
            break;
        case MIR_OP_MUL:
            dst_val = src0_val * src1_val;
            set_val_to_opnd(insn->dst, dst_val);
            break;
        case MIR_OP_DIV:
            dst_val = src0_val / src1_val;
            set_val_to_opnd(insn->dst, dst_val);
            break;
        case MIR_OP_REM:
            dst_val = src0_val % src1_val;
            set_val_to_opnd(insn->dst, dst_val);
            break;
        case MIR_OP_OR:
            dst_val = src0_val | src1_val;
            set_val_to_opnd(insn->dst, dst_val);
            break;
        case MIR_OP_XOR:
            dst_val = src0_val ^ src1_val;
            set_val_to_opnd(insn->dst, dst_val);
            break;
        case MIR_OP_SHL:
            dst_val = src0_val << src1_val;
            set_val_to_opnd(insn->dst, dst_val);
            break;
        case MIR_OP_SHR:
            dst_val = src0_val >> src1_val;
            set_val_to_opnd(insn->dst, dst_val);
            break;
        case MIR_OP_LD8:
            dst_val = read_mem(src0_val + src1_val, 1);
            set_val_to_opnd(insn->dst, dst_val);
            break;
        case MIR_OP_LD16:
            dst_val = read_mem(src0_val + src1_val, 2);
            set_val_to_opnd(insn->dst, dst_val);
            break;
        case MIR_OP_LD32:
            dst_val = read_mem(src0_val + src1_val, 4);
            set_val_to_opnd(insn->dst, dst_val);
            break;
        case MIR_OP_LD64:
            dst_val = read_mem(src0_val + src1_val, 8);
            set_val_to_opnd(insn->dst, dst_val);
            break;
        case MIR_OP_ST8:
            dst_val = get_val_from_opnd(insn->dst);
            write_mem(src0_val + src1_val, dst_val, 1);
            break;
        case MIR_OP_ST16:
            dst_val = get_val_from_opnd(insn->dst);
            write_mem(src0_val + src1_val, dst_val, 2);
            break;
        case MIR_OP_ST32:
            dst_val = get_val_from_opnd(insn->dst);
            write_mem(src0_val + src1_val, dst_val, 4);
            break;
        case MIR_OP_ST64:
            dst_val = get_val_from_opnd(insn->dst);
            write_mem(src0_val + src1_val, dst_val, 8);
            break;
        case MIR_OP_W_FLAG:
            // assert src1 is unvalued
            assert(src1_val == 0);
            // set_flag_from_value(src0_val);
            set_flag_hard();
            break;
        default:
            break;
    }
}

uint64_t Replayer::get_reg_val(reg_id_t reg) {
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
    } else if (IS_FLAG_REG(reg)) {
        return flag_reg_file[GET_FLAG_REG_NUM(reg)];
    } else {
        assert(false && "Invalid register encoding");
    }
}

uint64_t Replayer::get_val_from_opnd(mir_opnd_t opnd) {
    if (opnd.type == MIR_OPND_REG) {
        return get_reg_val(opnd.value.reg);
    } else if (opnd.type == MIR_OPND_IMM) {
        return opnd.value.imm;
    }
    return 0;
}

void Replayer::set_val_to_opnd(mir_opnd_t opnd, uint64_t value) {
    if (opnd.type == MIR_OPND_REG) {
        if (IS_DR_REG_GPR(opnd.value.reg)) {
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
        // flags cannot be set directly
    } else if (opnd.type == MIR_OPND_IMM) {
        assert(false && "attempted to set imm value");
    }
}

// void Replayer::set_flag_from_value(uint64_t value) {
    // flag_reg_file[GET_FLAG_REG_NUM(FLAG_REG_CF)] = ?;
    // flag_reg_file[GET_FLAG_REG_NUM(FLAG_REG_PF)] = value & 0x01;
    // flag_reg_file[GET_FLAG_REG_NUM(FLAG_REG_AF)] = (value & 0x15) == 0x15;
    // flag_reg_file[GET_FLAG_REG_NUM(FLAG_REG_ZF)] = value == 0;
    // flag_reg_file[GET_FLAG_REG_NUM(FLAG_REG_SF)] = (value >> 63) & 1;
    // flag_reg_file[GET_FLAG_REG_NUM(FLAG_REG_OF)] = (value >> 31) & 1;
// }

// marks a flag as being set
void Replayer::set_flag_hard() {
    flag_reg_file[GET_FLAG_REG_NUM(FLAG_REG_CF)] = 2;
    flag_reg_file[GET_FLAG_REG_NUM(FLAG_REG_PF)] = 2;
    flag_reg_file[GET_FLAG_REG_NUM(FLAG_REG_AF)] = 2;
    flag_reg_file[GET_FLAG_REG_NUM(FLAG_REG_ZF)] = 2;
    flag_reg_file[GET_FLAG_REG_NUM(FLAG_REG_SF)] = 2;
    flag_reg_file[GET_FLAG_REG_NUM(FLAG_REG_OF)] = 2;
}

// marks a flag as being unset
void Replayer::unset_flag_hard() {
    flag_reg_file[GET_FLAG_REG_NUM(FLAG_REG_CF)] = 0;
    flag_reg_file[GET_FLAG_REG_NUM(FLAG_REG_PF)] = 0;
    flag_reg_file[GET_FLAG_REG_NUM(FLAG_REG_AF)] = 0;
    flag_reg_file[GET_FLAG_REG_NUM(FLAG_REG_ZF)] = 0;
    flag_reg_file[GET_FLAG_REG_NUM(FLAG_REG_SF)] = 0;
    flag_reg_file[GET_FLAG_REG_NUM(FLAG_REG_OF)] = 0;
}
