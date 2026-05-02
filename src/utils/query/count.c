#include "count.h"

#include <limits.h>
#include <stdlib.h>

/**
 * DFS post-ordre : calcule le nombre de modeles memoise dans counts[id].
 * counted[id] != 0 indique que counts[id] est valide.
 *
 * Detection sticky d'overflow : si une multiplication ou une addition
 * long long deborde, *overflow est mis a 1 et la valeur courante est
 * saturee a LLONG_MAX. Le caller (dnnf_count) propage *overflow vers
 * overflow_out. Une fois leve, le drapeau reste leve.
 */
static long long count_rec(DNNFNode* n, long long* counts, char* counted,
                            int* overflow) {
    if (counted[n->id]) return counts[n->id]; // deja compté
    long long v;
    switch (n->type) {
        case DNNF_TRUE:     v = 1; break; // 1 modèle
        case DNNF_FALSE:    v = 0; break; // 0 modèle
        case DNNF_LIT_POS:                // 1 modèle
        case DNNF_LIT_NEG:  v = 1; break; // 1 modèle
        case DNNF_AND: {                  // v = 1 puis on multiplie par chaque enfant (décomposabilité)
            v = 1;
            for (int k = 0; k < n->num_children; k++) {
                long long child = count_rec(n->children[k], counts, counted,
                                              overflow);
                long long prod;
                if (__builtin_mul_overflow(v, child, &prod)) {
                    *overflow = 1;
                    v = LLONG_MAX;
                } else {
                    v = prod;
                }
            }
            break;
        }
        case DNNF_OR: {                   // v = 0 puis on additionne (déterminisme)
            v = 0;
            for (int k = 0; k < n->num_children; k++) {
                long long child = count_rec(n->children[k], counts, counted,
                                              overflow);
                long long sum;
                if (__builtin_add_overflow(v, child, &sum)) {
                    *overflow = 1;
                    v = LLONG_MAX;
                } else {
                    v = sum;
                }
            }
            break;
        }
        default: v = 0; break;
    }
    counts[n->id] = v;
    counted[n->id] = 1;
    return v;
}

long long* dnnf_count_table(DNNFNode* root, DNNFPool* pool) {
    int n_entries = pool->num_nodes;
    long long* counts = calloc(n_entries, sizeof(long long)); // compte du noeud id
    char* counted = calloc(n_entries, sizeof(char)); // booléean noeud deja calculé ?
    int local_overflow = 0;
    count_rec(root, counts, counted, &local_overflow);
    free(counted);
    // Note : pas de canal d'overflow expose ici. Les callers de
    // dnnf_count_table (find_model, validity) calculent des comptes locaux
    // pour des decisions de routage, l'overflow est une perte de precision
    // mais ne change pas la decision (sauf cas tres pathologiques) -- on
    // documente le risque dans les commentaires de count.h.
    return counts;
}

long long dnnf_count(DNNFNode* root, DNNFPool* pool, int* overflow_out) {
    if (!root || !pool) {
        return 0;
    }
    int n_entries = pool->num_nodes;
    long long* counts = calloc(n_entries, sizeof(long long));
    char* counted = calloc(n_entries, sizeof(char));
    int local_overflow = 0;
    long long result = count_rec(root, counts, counted, &local_overflow);
    free(counted);
    free(counts);
    if (overflow_out) {
        *overflow_out = local_overflow;
    }
    return result;
}
