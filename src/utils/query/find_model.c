#include "find_model.h"
#include "count.h"

#include <assert.h>
#include <stdlib.h>

/**
 * Descente top-down dans le DAG, ecrit dans model_out la valeur de chaque
 * variable rencontree sur le chemin choisi.
 *
 * Precondition : counts[root->id] > 0 (sinon il n'y a aucun modele a trouver,
 * et le caller doit avoir teste avant l'appel).
 */
static void find_model_descend(DNNFNode* n, DNNFPool* pool,
                               long long* counts, int* model_out) {
    (void)pool;  // borne implicite : counts est dimensionnee a pool->num_nodes

    switch (n->type) {
        case DNNF_TRUE:
            return;
        case DNNF_FALSE:
            assert(0 && "find_model_descend: FALSE atteint depuis un AND");
            return;
        case DNNF_LIT_POS:
            model_out[n->var_index] = 1;
            return;
        case DNNF_LIT_NEG:
            model_out[n->var_index] = 0;
            return;
        case DNNF_AND:
            for (int k = 0; k < n->num_children; k++) {
                find_model_descend(n->children[k], pool, counts, model_out);
            }
            return;
        case DNNF_OR: {
            // Premier enfant dont counts > 0 : contient au moins un modele.
            for (int k = 0; k < n->num_children; k++) {
                if (counts[n->children[k]->id] > 0) {
                    find_model_descend(n->children[k], pool,
                                       counts, model_out);
                    return;
                }
            }
            assert(0 && "find_model_descend: OR sans enfant satisfiable");
            return;
        }
    }
}

int dnnf_find_model(DNNFNode* root, DNNFPool* pool,
                    int num_vars, int* model_out) {
    if (!root || !pool || num_vars < 0 || !model_out) return -1;

    long long* counts = dnnf_count_table(root, pool);
    if (counts[root->id] == 0) {
        free(counts);
        return 0;  // UNSAT
    }

    // Variables libres (DAG potentiellement non lisse) - 0 par defaut.
    // model_out[0] n'est PAS touche (convention DIMACS, index 0 inutilise).
    for (int v = 1; v <= num_vars; v++) {
        model_out[v] = 0;
    }

    find_model_descend(root, pool, counts, model_out);
    free(counts);
    return 1;
}
