#ifndef REG_ANALYZER_H
#define REG_ANALYZER_H

#include <unordered_map>
using std::unordered_map;
#include "dr_api.h"
#include "abstract_backend.h"
#include "translate_context.h"
#include "mir_insn.h"

class RegAnalyzer : public AbstractBackend {
public:
    RegAnalyzer();
    ~RegAnalyzer();
    void replay(mir_insn_list_t *insn_list) override;
    void report();
private:
    // register file
    uint64_t gp_reg_file[DR_NUM_GPR_REGS];
    // tmp register file
    uint64_t tmp_reg_file[NUM_TMP_REGS];

    // assume all memory is chaotically volatile
    uint64_t read_mem(mir_insn_t *insn);
    void write_mem(mir_insn_t *insn);
    void access_mem(mir_insn_t *insn);
    bool is_const_addr(mir_insn_t *insn);

    void step(mir_insn_t *insn);
    uint64_t get_reg_val(reg_id_t reg);
    uint64_t get_val_from_opnd(mir_opnd_t opnd);
    void set_val_to_opnd(mir_opnd_t opnd, uint64_t value);

    // a dictionary to store the memory access pattern
    unordered_map<uint64_t, uint64_t> mem_access_pattern;
    void inc_mem_access_count(uint64_t addr);
};
#endif // REG_ANALYZER_H