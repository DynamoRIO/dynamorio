#ifndef REPLAYER_H
#define REPLAYER_H

#include <unordered_map>
using std::unordered_map;

#include "dr_api.h"
#include "abstract_backend.h"
#include "translate_context.h"
#include "mir_insn.h"

enum InitStrategy {
    INIT_STRATEGY_ZERO,
    INIT_STRATEGY_RANDOM
};

class Replayer : public AbstractBackend {
public:
    Replayer(InitStrategy init_strategy);
    ~Replayer();
    void replay(mir_insn_list_t *insn_list) override;
    // this is public for testing purposes
    uint64_t get_reg_val(reg_id_t reg);
    void report();
private:
    FILE* log_file;
    InitStrategy init_strategy;
    // register file
    uint64_t gp_reg_file[DR_NUM_GPR_REGS];
    // tmp register file 
    uint64_t tmp_reg_file[NUM_TMP_REGS];
    // flag register file
    uint64_t flag_reg_file[NUM_FLAG_REGS];

    // memory - using unordered_map since we're using vaddr
    unordered_map<uintptr_t, uint8_t> shadow_mem;
    // both addr and size are byte-addressable
    uint64_t read_mem(uintptr_t addr, size_t size);
    void write_mem(uintptr_t addr, uint64_t value, size_t size);

    void step(mir_insn_t *insn);
    uint64_t get_val_from_opnd(mir_opnd_t opnd);
    void set_val_to_opnd(mir_opnd_t opnd, uint64_t value);
    // void set_flag_from_value(uint64_t value);
    void set_flag_hard();
    void unset_flag_hard();
};

#endif // REPLAYER_H
