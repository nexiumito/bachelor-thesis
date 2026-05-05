#include "dnnf_transform.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "bitset.h"

// Forward declaration -- definition dans la Section 1 (portees + smoothing).
static void free_scope_table(DNNFPool* pool);


// ============================================================================
// SECTION 1 : PORTEES + SMOOTHING
// ============================================================================
//
// Justification : Lemme A.1, Darwiche & Marquis 2002 (page 244) -- toute
// d-DNNF se traduit en sd-DNNF en polytime via insertion de (~v ou v) pour
// les variables manquantes des enfants d'un OR.
//
// La table des portees est stockee dans pool->scope_by_id, allouee lazy.
// PIEGE : la table est INVALIDEE par toute transformation qui cree de nouveaux
// noeuds (smooth, condition). dnnf_smooth la libere en sortie.
// ============================================================================

static int scope_test(Bitset* bs, int v) {
    return (int)((bs->words[v / 64] >> (v % 64)) & 1ULL);
}

static int scope_is_zero(Bitset* bs) {
    for (int w = 0; w < bs->num_words; w++) {
        if (bs->words[w] != 0ULL) return 0;
    }
    return 1;
}

static void scope_or_in_place(Bitset* dest, Bitset* src) {
    for (int w = 0; w < dest->num_words; w++) {
        dest->words[w] |= src->words[w];
    }
}

static void scope_diff(Bitset* dest, Bitset* src1, Bitset* src2) {
    for (int w = 0; w < dest->num_words; w++) {
        dest->words[w] = src1->words[w] & ~src2->words[w];
    }
}

static void free_scope_table(DNNFPool* pool) {
    if (!pool || !pool->scope_by_id) return;
    for (int k = 0; k < pool->scope_capacity; k++) {
        if (pool->scope_by_id[k]) free_bitset(pool->scope_by_id[k]);
    }
    free(pool->scope_by_id);
    pool->scope_by_id = NULL;
    pool->scope_capacity = 0;
}

static void compute_scopes_rec(DNNFNode* n, DNNFPool* pool, char* visited) {
    if (visited[n->id]) return;
    visited[n->id] = 1;

    Bitset* my = pool->scope_by_id[n->id];

    switch (n->type) {
        case DNNF_TRUE:
        case DNNF_FALSE:
            break;
        case DNNF_LIT_POS:
        case DNNF_LIT_NEG:
            set_bit(my, n->var_index);
            break;
        case DNNF_AND:
        case DNNF_OR:
            for (int k = 0; k < n->num_children; k++) {
                compute_scopes_rec(n->children[k], pool, visited);
                scope_or_in_place(my, pool->scope_by_id[n->children[k]->id]);
            }
            break;
    }
}

// (Re)calcule la table pool->scope_by_id : un Bitset des variables mentionnees
// par le sous-DAG de chaque noeud. Helper interne de dnnf_smooth.
static void dnnf_compute_scopes(DNNFPool* pool, int num_vars) {
    if (!pool || num_vars < 0) return;

    free_scope_table(pool);

    pool->num_vars = num_vars;
    pool->scope_capacity = pool->num_nodes;
    pool->scope_by_id = calloc(pool->num_nodes, sizeof(Bitset*));
    for (int k = 0; k < pool->num_nodes; k++) {
        pool->scope_by_id[k] = create_bitset(num_vars + 1);
    }

    char* visited = calloc(pool->num_nodes, sizeof(char));
    for (int k = 0; k < pool->num_nodes; k++) {
        compute_scopes_rec(pool->nodes[k], pool, visited);
    }
    free(visited);
}

// Cache : un seul (v ou ~v) par variable v dans la sortie smoothed.
static DNNFNode* smooth_get_decision(DNNFPool* pool, DNNFNode** decision_or,
                                     int v) {
    if (decision_or[v]) return decision_or[v];
    DNNFNode* d = dnnf_make_or(pool, 2);
    dnnf_or_add_child(d, dnnf_make_literal(pool, v, 1));
    dnnf_or_add_child(d, dnnf_make_literal(pool, v, 0));
    decision_or[v] = d;
    return d;
}

// Construit AND_{v in missing} (v ou ~v) en fold left. Suppose missing != 0.
static DNNFNode* smooth_build_witness(DNNFPool* pool, DNNFNode** decision_or,
                                      Bitset* missing, int num_vars) {
    DNNFNode* witness = NULL;
    for (int v = 1; v <= num_vars; v++) {
        if (!scope_test(missing, v)) continue;
        DNNFNode* dv = smooth_get_decision(pool, decision_or, v);
        if (!witness) {
            witness = dv;
        } else {
            witness = dnnf_make_and(pool, witness, dv);
        }
    }
    return witness;
}

static DNNFNode* smooth_rec(DNNFNode* n, DNNFPool* pool, int num_vars,
                            DNNFNode** decision_or, DNNFNode** rewritten,
                            Bitset* missing_buf, int initial_n_nodes);

static DNNFNode* smooth_wrap_for_or(DNNFNode* child_smoothed,
                                    Bitset* parent_scope,
                                    Bitset* original_child_scope,
                                    DNNFPool* pool, DNNFNode** decision_or,
                                    Bitset* missing_buf, int num_vars,
                                    int* out_changed) {
    scope_diff(missing_buf, parent_scope, original_child_scope);
    if (scope_is_zero(missing_buf)) {
        return child_smoothed;
    }
    DNNFNode* witness = smooth_build_witness(pool, decision_or,
                                             missing_buf, num_vars);
    if (out_changed) *out_changed = 1;
    return dnnf_make_and(pool, child_smoothed, witness);
}

static DNNFNode* smooth_rec(DNNFNode* n, DNNFPool* pool, int num_vars,
                            DNNFNode** decision_or, DNNFNode** rewritten,
                            Bitset* missing_buf, int initial_n_nodes) {
    if (n->id >= initial_n_nodes) return n;
    if (rewritten[n->id]) return rewritten[n->id];

    DNNFNode* result;
    switch (n->type) {
        case DNNF_TRUE:
        case DNNF_FALSE:
        case DNNF_LIT_POS:
        case DNNF_LIT_NEG:
            result = n;
            break;

        case DNNF_AND: {
            int changed = 0;
            DNNFNode** new_children = malloc((size_t)n->num_children
                                              * sizeof(DNNFNode*));
            for (int k = 0; k < n->num_children; k++) {
                new_children[k] = smooth_rec(n->children[k], pool, num_vars,
                                              decision_or, rewritten,
                                              missing_buf, initial_n_nodes);
                if (new_children[k] != n->children[k]) changed = 1;
            }
            if (!changed) {
                result = n;
            } else {
                result = new_children[0];
                for (int k = 1; k < n->num_children; k++) {
                    result = dnnf_make_and(pool, result, new_children[k]);
                }
            }
            free(new_children);
            break;
        }

        case DNNF_OR: {
            Bitset* scope_or = pool->scope_by_id[n->id];
            int any_change = 0;
            DNNFNode** new_children = malloc((size_t)n->num_children
                                              * sizeof(DNNFNode*));
            for (int k = 0; k < n->num_children; k++) {
                DNNFNode* child = n->children[k];
                DNNFNode* csm = smooth_rec(child, pool, num_vars,
                                            decision_or, rewritten,
                                            missing_buf, initial_n_nodes);
                if (csm != child) any_change = 1;
                Bitset* scope_child = pool->scope_by_id[child->id];
                new_children[k] = smooth_wrap_for_or(csm, scope_or, scope_child,
                                                     pool, decision_or,
                                                     missing_buf, num_vars,
                                                     &any_change);
            }

            if (!any_change) {
                result = n;
            } else {
                DNNFNode* new_or = dnnf_make_or(pool, n->num_children);
                for (int k = 0; k < n->num_children; k++) {
                    dnnf_or_add_child(new_or, new_children[k]);
                }
                result = new_or;
            }
            free(new_children);
            break;
        }

        default:
            assert(0 && "smooth_rec: type inconnu");
            result = n;
            break;
    }

    rewritten[n->id] = result;
    return result;
}

DNNFNode* dnnf_smooth(DNNFNode* root, DNNFPool* pool, int num_vars) {
    if (!root || !pool || num_vars < 0) return root;

    dnnf_compute_scopes(pool, num_vars);

    int initial_n_nodes = pool->num_nodes;
    DNNFNode** decision_or = calloc((size_t)num_vars + 1, sizeof(DNNFNode*));
    DNNFNode** rewritten = calloc(initial_n_nodes, sizeof(DNNFNode*));
    Bitset* missing_buf = create_bitset(num_vars + 1);

    DNNFNode* result = smooth_rec(root, pool, num_vars, decision_or,
                                   rewritten, missing_buf, initial_n_nodes);

    free_bitset(missing_buf);
    free(rewritten);
    free(decision_or);

    free_scope_table(pool);

    return result;
}


// ============================================================================
// SECTION 2 : CONDITIONING (CD)
// ============================================================================
//
// Conditioning = restriction sur la valeur d'une variable. Definition formelle :
// Definition 5.4 de Darwiche & Marquis 2002. Tractabilite : Table 7 du meme
// paper, d-DNNF satisfait CD.
//
// PIEGE : ne pas simplifier OR(TRUE, X) en TRUE. La simplification est
// semantiquement valide mais perdrait le scope de X dont dnnf_smooth ulterieur
// a besoin pour inserer (v ou ~v) (Lemme A.1 Darwiche-Marquis 2002).
// ============================================================================

static DNNFNode* condition_rec(DNNFNode* n, DNNFPool* pool,
                                int var, int value,
                                DNNFNode** cache, int initial_n_nodes) {
    if (n->id >= initial_n_nodes) return n;
    if (cache[n->id]) return cache[n->id];

    DNNFNode* result;
    switch (n->type) {
        case DNNF_TRUE:
        case DNNF_FALSE:
            result = n;
            break;

        case DNNF_LIT_POS:
            if (n->var_index == var) {
                result = value ? pool->node_true : pool->node_false;
            } else {
                result = n;
            }
            break;

        case DNNF_LIT_NEG:
            if (n->var_index == var) {
                result = value ? pool->node_false : pool->node_true;
            } else {
                result = n;
            }
            break;

        case DNNF_AND: {
            DNNFNode** kept = malloc((size_t)n->num_children
                                      * sizeof(DNNFNode*));
            int n_kept = 0;
            int has_false = 0;
            int any_change = 0;

            for (int k = 0; k < n->num_children; k++) {
                DNNFNode* c = condition_rec(n->children[k], pool, var, value,
                                             cache, initial_n_nodes);
                if (c != n->children[k]) any_change = 1;
                if (c == pool->node_false) {
                    has_false = 1;
                    any_change = 1;
                    break;
                }
                if (c == pool->node_true) {
                    any_change = 1;
                    continue;
                }
                kept[n_kept++] = c;
            }

            if (has_false) {
                result = pool->node_false;
            } else if (n_kept == 0) {
                result = pool->node_true;
            } else if (n_kept == 1) {
                result = kept[0];
            } else if (!any_change && n_kept == n->num_children) {
                result = n;
            } else {
                result = kept[0];
                for (int k = 1; k < n_kept; k++) {
                    result = dnnf_make_and(pool, result, kept[k]);
                }
            }
            free(kept);
            break;
        }

        case DNNF_OR: {
            DNNFNode** kept = malloc((size_t)n->num_children
                                      * sizeof(DNNFNode*));
            int n_kept = 0;
            int any_change = 0;

            for (int k = 0; k < n->num_children; k++) {
                DNNFNode* c = condition_rec(n->children[k], pool, var, value,
                                             cache, initial_n_nodes);
                if (c != n->children[k]) any_change = 1;
                if (c == pool->node_false) {
                    any_change = 1;
                    continue;
                }
                kept[n_kept++] = c;
                // Note : un TRUE est conserve, on ne short-circuite PAS l'OR.
            }

            if (n_kept == 0) {
                result = pool->node_false;
            } else if (n_kept == 1) {
                result = kept[0];
            } else if (!any_change && n_kept == n->num_children) {
                result = n;
            } else {
                DNNFNode* new_or = dnnf_make_or(pool, n_kept);
                for (int k = 0; k < n_kept; k++) {
                    dnnf_or_add_child(new_or, kept[k]);
                }
                result = new_or;
            }
            free(kept);
            break;
        }

        default:
            assert(0 && "condition_rec: type inconnu");
            result = n;
            break;
    }

    cache[n->id] = result;
    return result;
}

DNNFNode* dnnf_condition(DNNFNode* root, DNNFPool* pool,
                          int var, int value) {
    if (!root || !pool || var < 1) return root;
    if (value != 0 && value != 1) return root;

    int initial_n_nodes = pool->num_nodes;
    DNNFNode** cache = calloc(initial_n_nodes, sizeof(DNNFNode*));
    DNNFNode* result = condition_rec(root, pool, var, value, cache,
                                      initial_n_nodes);
    free(cache);

    free_scope_table(pool);

    return result;
}


// ============================================================================
// CLEANUP : libere les structures auxiliaires installees par smooth
// ============================================================================

void dnnf_transform_free_pool_extras(DNNFPool* pool) {
    if (!pool) return;
    free_scope_table(pool);
}
