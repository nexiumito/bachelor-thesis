#include "dnnf_transform.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "bitset.h"

// Helpers internes exposes par dnnf.c (declares dans dnnf.h, section "API
// INTERNE"). Leurs signatures respectent la discipline alloc_failed
// (retour NULL / int) ; on s'appuie sur le prototype de dnnf.h.

// Forward declarations -- definitions plus bas.
static void hashcons_free(DNNFHashTable* tab);
static void free_scope_table(DNNFPool* pool);


// ============================================================================
// SECTION 1 : HASH-CONSING POST-DP (compress)
// ============================================================================
//
// Reference : Bova et al. 2016 + deepsearch §B1.2 (hash-consing standard de
// l'ecosysteme d-DNNF C/C++).
//
// La table est interne a dnnf_transform.c -- aucune autre partie du code
// ne l'expose. Strategie : passe post-DP separee, declenchee par
// dnnf_compress(). Ne touche PAS les factories existantes (Phase 1 verrouillee).
//
// Cle canonique :
//   - LIT_POS / LIT_NEG : (type, var_index)
//   - AND / OR          : (type, ids des enfants tries croissant)
//   - TRUE / FALSE      : singletons, jamais hash-consees
//
// Pour OR, les enfants canoniques tries sont DEDUPLIQUES (un OR avec doublons
// post-canonicalisation aurait des modeles non-disjoints, cassant le
// determinisme -- on simplifie en l'unique enfant si la liste collapse a 1).
//
// PIEGE : l'invariant "enfant.id < parent.id" ne tient pas. On indexe les
// tableaux de memoisation par pool->num_nodes, jamais par root->id+1.
// ============================================================================

typedef struct dnnf_hash_entry {
    uint64_t                hash;
    dnnf_node_type          type;
    int                     var_index;     // pertinent pour LIT_*, ignore sinon
    int                     num_ids;
    int*                    sorted_ids;    // possede ; NULL si num_ids == 0
    DNNFNode*               canonical;     // pointeur emprunte au pool
    struct dnnf_hash_entry* next;
} DNNFHashEntry;

struct dnnf_hash_table {
    DNNFHashEntry**  buckets;
    int              num_buckets;
    int              num_entries;
};

// SplitMix64 -- avalanche correct pour des entiers 64 bits.
static uint64_t splitmix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

static uint64_t hashcons_compute_hash(dnnf_node_type type, int var_index,
                                      const int* sorted_ids, int num_ids) {
    uint64_t h = splitmix64((uint64_t)type * 0x9E3779B97F4A7C15ULL);
    h ^= splitmix64((uint64_t)(unsigned int)var_index + 1ULL);
    h ^= splitmix64((uint64_t)num_ids + 0x100ULL);
    for (int k = 0; k < num_ids; k++) {
        h ^= splitmix64((uint64_t)(unsigned int)sorted_ids[k]
                        + (uint64_t)k * 0x9E3779B97F4A7C15ULL);
    }
    return h;
}

static int hashcons_keys_equal(const DNNFHashEntry* e,
                                dnnf_node_type type, int var_index,
                                const int* sorted_ids, int num_ids) {
    if (e->type != type) return 0;
    if (e->var_index != var_index) return 0;
    if (e->num_ids != num_ids) return 0;
    for (int k = 0; k < num_ids; k++) {
        if (e->sorted_ids[k] != sorted_ids[k]) return 0;
    }
    return 1;
}

static DNNFHashTable* hashcons_create(int initial_buckets) {
    DNNFHashTable* tab = malloc(sizeof(DNNFHashTable));
    tab->num_buckets = initial_buckets;
    tab->num_entries = 0;
    tab->buckets = calloc(initial_buckets, sizeof(DNNFHashEntry*));
    return tab;
}

static void hashcons_free(DNNFHashTable* tab) {
    if (!tab) return;
    for (int b = 0; b < tab->num_buckets; b++) {
        DNNFHashEntry* e = tab->buckets[b];
        while (e) {
            DNNFHashEntry* next = e->next;
            if (e->sorted_ids) free(e->sorted_ids);
            free(e);
            e = next;
        }
    }
    free(tab->buckets);
    free(tab);
}

// Double la capacite si num_entries depasse 0.75 * num_buckets.
static void hashcons_grow_if_needed(DNNFHashTable* tab) {
    if ((tab->num_entries + 1) * 4 < tab->num_buckets * 3) return;

    int new_buckets = tab->num_buckets * 2;
    DNNFHashEntry** new_table = calloc(new_buckets, sizeof(DNNFHashEntry*));

    for (int b = 0; b < tab->num_buckets; b++) {
        DNNFHashEntry* e = tab->buckets[b];
        while (e) {
            DNNFHashEntry* next = e->next;
            int idx = (int)(e->hash % (uint64_t)new_buckets);
            e->next = new_table[idx];
            new_table[idx] = e;
            e = next;
        }
    }

    free(tab->buckets);
    tab->buckets = new_table;
    tab->num_buckets = new_buckets;
}

static DNNFNode* hashcons_lookup(DNNFHashTable* tab,
                                 dnnf_node_type type, int var_index,
                                 const int* sorted_ids, int num_ids,
                                 uint64_t* out_hash) {
    uint64_t h = hashcons_compute_hash(type, var_index, sorted_ids, num_ids);
    if (out_hash) *out_hash = h;
    int idx = (int)(h % (uint64_t)tab->num_buckets);
    for (DNNFHashEntry* e = tab->buckets[idx]; e; e = e->next) {
        if (e->hash == h && hashcons_keys_equal(e, type, var_index,
                                                sorted_ids, num_ids)) {
            return e->canonical;
        }
    }
    return NULL;
}

static void hashcons_insert(DNNFHashTable* tab,
                            dnnf_node_type type, int var_index,
                            const int* sorted_ids, int num_ids,
                            uint64_t hash, DNNFNode* canonical) {
    hashcons_grow_if_needed(tab);

    DNNFHashEntry* e = malloc(sizeof(DNNFHashEntry));
    e->hash = hash;
    e->type = type;
    e->var_index = var_index;
    e->num_ids = num_ids;
    if (num_ids > 0) {
        e->sorted_ids = malloc(num_ids * sizeof(int));
        memcpy(e->sorted_ids, sorted_ids, num_ids * sizeof(int));
    } else {
        e->sorted_ids = NULL;
    }
    e->canonical = canonical;

    int idx = (int)(hash % (uint64_t)tab->num_buckets);
    e->next = tab->buckets[idx];
    tab->buckets[idx] = e;
    tab->num_entries++;
}

/**
 * Factory interne au compress : alloue un nouveau noeud du type donne SANS
 * appliquer de simplifications locales (les simplifications ont deja eu lieu
 * en Phase 1). Les factories publiques dnnf_make_and / make_or ne conviennent
 * pas ici car elles simplifient et ne supportent pas un nombre arbitraire
 * d'enfants.
 */
static DNNFNode* compress_alloc_internal(DNNFPool* pool,
                                          dnnf_node_type type, int var_index,
                                          DNNFNode** children, int num_children) {
    // Discipline alloc_failed : early-bail si le pool est deja en etat
    // d'echec, pour eviter d'agraver la corruption.
    if (pool->alloc_failed) return pool->node_false;

    DNNFNode* n = allocate_raw_node();
    if (!n) {
        pool->alloc_failed = 1;
        return pool->node_false;
    }
    n->type = type;
    n->var_index = var_index;
    n->capacity = num_children;
    n->num_children = num_children;
    n->children = NULL;
    if (num_children > 0) {
        n->children = malloc(num_children * sizeof(DNNFNode*));
        if (!n->children) {
            free(n);
            pool->alloc_failed = 1;
            return pool->node_false;
        }
        for (int k = 0; k < num_children; k++) {
            n->children[k] = children[k];
        }
    }
    if (pool_register_node(pool, n) != 0) {
        free(n->children);
        free(n);
        return pool->node_false;
    }
    return n;
}

static void compress_sort_children(int* ids, DNNFNode** ptrs, int n) {
    for (int i = 1; i < n; i++) {
        int v_id = ids[i];
        DNNFNode* v_ptr = ptrs[i];
        int j = i - 1;
        while (j >= 0 && ids[j] > v_id) {
            ids[j+1] = ids[j];
            ptrs[j+1] = ptrs[j];
            j--;
        }
        ids[j+1] = v_id;
        ptrs[j+1] = v_ptr;
    }
}

static int compress_dedup_sorted(int* ids, DNNFNode** ptrs, int n) {
    if (n <= 1) return n;
    int w = 1;
    for (int i = 1; i < n; i++) {
        if (ids[i] != ids[w-1]) {
            ids[w] = ids[i];
            ptrs[w] = ptrs[i];
            w++;
        }
    }
    return w;
}

static DNNFNode* compress_rec(DNNFNode* n, DNNFPool* pool, DNNFNode** canon) {
    if (!n) return NULL;
    if (canon[n->id]) return canon[n->id];

    if (n->num_children == 0) {
        switch (n->type) {
            case DNNF_TRUE:  canon[n->id] = pool->node_true;  return pool->node_true;
            case DNNF_FALSE: canon[n->id] = pool->node_false; return pool->node_false;
            case DNNF_LIT_POS:
            case DNNF_LIT_NEG: {
                uint64_t h;
                DNNFNode* found = hashcons_lookup(pool->hashcons,
                                                  n->type, n->var_index,
                                                  NULL, 0, &h);
                if (found) {
                    canon[n->id] = found;
                    return found;
                }
                hashcons_insert(pool->hashcons, n->type, n->var_index,
                                NULL, 0, h, n);
                canon[n->id] = n;
                return n;
            }
            default:
                assert(0 && "compress_rec: feuille de type inconnu");
                return n;
        }
    }

    int k = n->num_children;
    DNNFNode** child_canon = malloc(k * sizeof(DNNFNode*));
    int* child_ids = malloc(k * sizeof(int));
    for (int i = 0; i < k; i++) {
        child_canon[i] = compress_rec(n->children[i], pool, canon);
        child_ids[i] = child_canon[i]->id;
    }

    compress_sort_children(child_ids, child_canon, k);

    int new_k = k;
    if (n->type == DNNF_OR) {
        new_k = compress_dedup_sorted(child_ids, child_canon, k);
        if (new_k == 1) {
            DNNFNode* res = child_canon[0];
            free(child_canon);
            free(child_ids);
            canon[n->id] = res;
            return res;
        }
    }

    uint64_t h;
    DNNFNode* found = hashcons_lookup(pool->hashcons,
                                      n->type, 0, child_ids, new_k, &h);
    if (found) {
        free(child_canon);
        free(child_ids);
        canon[n->id] = found;
        return found;
    }

    int reuse = (k == new_k);
    if (reuse) {
        for (int i = 0; i < k; i++) {
            if (n->children[i] != child_canon[i]) { reuse = 0; break; }
        }
    }

    DNNFNode* result;
    if (reuse) {
        result = n;
    } else {
        result = compress_alloc_internal(pool, n->type, 0, child_canon, new_k);
    }

    hashcons_insert(pool->hashcons, n->type, 0,
                    child_ids, new_k, h, result);
    canon[n->id] = result;
    free(child_canon);
    free(child_ids);
    return result;
}

DNNFNode* dnnf_compress(DNNFNode* root, DNNFPool* pool) {
    if (!root || !pool) return NULL;

    if (!pool->hashcons) {
        pool->hashcons = hashcons_create(1024);
    }

    int n_entries = pool->num_nodes;
    DNNFNode** canon = calloc(n_entries, sizeof(DNNFNode*));
    DNNFNode* result = compress_rec(root, pool, canon);
    free(canon);

    // Invalidation de la table de portees : la canonicalisation peut
    // faire pointer la racine vers un noeud dont l'id ne correspond plus.
    free_scope_table(pool);

    return result;
}


// ============================================================================
// SECTION 2 : PORTEES + SMOOTHING
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

void dnnf_compute_scopes(DNNFPool* pool, int num_vars) {
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
// SECTION 3 : CONDITIONING (CD)
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
// CLEANUP : libere les structures auxiliaires installees par compress/smooth
// ============================================================================

void dnnf_transform_free_pool_extras(DNNFPool* pool) {
    if (!pool) return;
    if (pool->hashcons) {
        hashcons_free(pool->hashcons);
        pool->hashcons = NULL;
    }
    free_scope_table(pool);
}
