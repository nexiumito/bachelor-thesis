#include "consistency.h"
#include "count.h"

int dnnf_consistency(DNNFNode* root, DNNFPool* pool) {
    if (!root || !pool) return 0; // UNSAT
    // Si overflow lors du recompte, c'est forcement > 0 -> SAT.
    int overflow = 0;
    long long c = dnnf_count(root, pool, &overflow);
    if (overflow) return 1;
    return c > 0 ? 1 : 0; // > 0 -> SAT, sinon UNSAT
}
