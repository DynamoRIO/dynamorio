#ifndef ABSTRACT_REPLAYER_H
#define ABSTRACT_REPLAYER_H

#include "mir_insn.h"

class AbstractReplayer {
public:
    virtual void step(mir_insn_list_t *insn_list) = 0;
};

#endif // ABSTRACT_REPLAYER_H