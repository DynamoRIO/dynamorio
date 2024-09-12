#ifndef ABSTRACT_BACKEND_H
#define ABSTRACT_BACKEND_H

#include "mir_insn.h"

class AbstractBackend {
public:
    virtual void replay(mir_insn_list_t *insn_list) = 0;
};

#endif // ABSTRACT_BACKEND_H