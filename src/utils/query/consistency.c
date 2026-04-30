#include "consistency.h"
#include "count.h"

int dnnf_consistency(DNNFNode* root, DNNFPool* pool) {
    if (!root || !pool) return 0; // UNSAT
    return dnnf_count(root, pool) > 0 ? 1 : 0; // > 0 -> SAT, sinon UNSAT
}
