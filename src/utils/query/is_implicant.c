#include "is_implicant.h"
#include "count.h"
#include "../dnnf_transform.h"

#include <stdlib.h>

// 2^n en long long ; -1 si n trop grand pour tenir.
static long long pow2_safe(int n) {
    if (n < 0) return 0;
    if (n >= 63) return -1;
    return 1LL << n;
}

int dnnf_is_implicant(DNNFNode* root, DNNFPool* pool, int num_vars,
                      const int* literals, int num_literals,
                      long long* count_out, long long* target_out,
                      int* k_distinct_out) {
    if (count_out) *count_out = 0;
    if (target_out) *target_out = 0;
    if (k_distinct_out) *k_distinct_out = 0;

    if (!root) return DNNF_IS_IMPLICANT_UNSAT;

    // Compte le nombre de variables distinctes dans gamma.
    int* seen = calloc((size_t)num_vars + 2, sizeof(int));
    int k_distinct = 0;
    for (int i = 0; i < num_literals; i++) {
        int v = (literals[i] > 0) ? literals[i] : -literals[i];
        if (v < 1 || v > num_vars) {
            free(seen);
            return DNNF_IS_IMPLICANT_BAD_VAR;
        }
        if (!seen[v]) {
            seen[v] = 1;
            k_distinct++;
        }
    }
    free(seen);
    if (k_distinct_out) *k_distinct_out = k_distinct;

    long long target = pow2_safe(num_vars - k_distinct);
    if (target_out) *target_out = target;
    if (target < 0) return DNNF_IS_IMPLICANT_OVERFLOW;

    // Conditionner sur gamma = (l1 ^ ... ^ lk).
    DNNFNode* current = root;
    for (int i = 0; i < num_literals; i++) {
        int lit = literals[i];
        int var = (lit > 0) ? lit : -lit;
        int value = (lit > 0) ? 1 : 0;
        current = dnnf_condition(current, pool, var, value);
    }

    // Le conditionnement peut casser la lissite ; lisser avant de compter.
    DNNFNode* smoothed = dnnf_smooth(current, pool, num_vars);

    // Comptage avec detection sticky d'overflow.
    int local_overflow = 0;
    long long c = dnnf_count(smoothed, pool, &local_overflow);
    if (count_out) *count_out = c;

    if (local_overflow) {
        // Compte sature : si c == target par hasard, ce serait un faux YES.
        // On signale l'incertitude au caller.
        return DNNF_IS_IMPLICANT_OVERFLOW;
    }
    return (c == target) ? DNNF_IS_IMPLICANT_YES : DNNF_IS_IMPLICANT_NO;
}
