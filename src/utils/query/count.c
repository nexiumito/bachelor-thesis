#include "count.h"

#include <stdlib.h>

/**
 * DFS post-ordre : calcule le nombre de modeles memoise dans counts[id].
 * counted[id] != 0 indique que counts[id] est valide.
 */
static long long count_rec(DNNFNode* n, long long* counts, char* counted) {
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
                v *= count_rec(n->children[k], counts, counted);
            }
            break;
        }
        case DNNF_OR: {                   // v = 0 puis on additionne (déterminisme)
            v = 0;
            for (int k = 0; k < n->num_children; k++) {
                v += count_rec(n->children[k], counts, counted);
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
    count_rec(root, counts, counted);
    free(counted);
    return counts;
}

long long dnnf_count(DNNFNode* root, DNNFPool* pool) {
    if (!root || !pool) return 0;
    long long* counts = dnnf_count_table(root, pool);
    long long result = counts[root->id];
    free(counts);
    return result;
}
