#include "validity.h"
#include "count.h"

// 2^n en long long ; -1 si n trop grand pour tenir.
static long long pow2_safe(int n) {
    if (n < 0) return 0;
    if (n >= 63) return -1;
    return 1LL << n;
}

int dnnf_validity(DNNFNode* root, DNNFPool* pool, int num_vars,
                  long long* count_out, long long* max_models_out) {
    long long max_models = pow2_safe(num_vars);
    if (max_models_out) *max_models_out = max_models;

    if (!root) {
        if (count_out) *count_out = 0;
        return DNNF_VALIDITY_UNSAT;
    }
    if (max_models < 0) {
        if (count_out) *count_out = dnnf_count(root, pool);
        return DNNF_VALIDITY_OVERFLOW;
    }

    long long c = dnnf_count(root, pool);
    if (count_out) *count_out = c;

    return (c == max_models) ? DNNF_VALIDITY_VALID
                             : DNNF_VALIDITY_NOT_VALID;
}
