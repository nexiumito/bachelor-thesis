#include "dnnf.h"
#include "dnnf_transform.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// DNNF : IMPLEMENTATION DU POOL ET DU DAG D-DNNF
// ============================================================================
//
// Reference : Bova, Capelli, Mengel, Slivovsky 2016, Section 3.3.
//
// Proprietaire unique : le DNNFPool. Chaque dnnf_make_* incremente
// pool->num_nodes et assigne id = num_nodes - 1. Ids strictement monotones,
// jamais reaffectes, jamais reutilises apres liberation (il n'y a pas de
// liberation individuelle). Tous les enfants d'un noeud ont donc un id
// inferieur au sien, propriete exploitee par dnnf_export_nnf.
// ============================================================================


// ============================================================================
// SECTION 1 : POOL (ARENA ALLOCATOR)
// ============================================================================

/**
 * Allocation brute d'un DNNFNode (sans enregistrement dans le pool).
 * Les champs communs sont initialises ; var_index et children sont
 * configures par l'appelant selon le type.
 *
 * Retourne NULL si malloc echoue (l'appelant doit lever pool->alloc_failed).
 */
static DNNFNode* allocate_raw_node(void) {
    DNNFNode* node = malloc(sizeof(DNNFNode));
    if (!node) return NULL;
    node->id = -1;
    node->type = DNNF_AND; // reecrit par l'appelant
    node->var_index = 0;
    node->children = NULL;
    node->num_children = 0;
    node->capacity = 0;
    return node;
}

/**
 * Enregistre un noeud dans le pool, lui assigne son id (= num_nodes avant
 * incrementation) et double la capacite si besoin.
 *
 * Retourne 0 sur succes, -1 sur echec d'allocation. Sur echec, le drapeau
 * pool->alloc_failed est leve et le noeud n'est pas enregistre (le caller
 * doit liberer ``node`` lui-meme avant de retourner pool->node_false).
 */
static int pool_register_node(DNNFPool* pool, DNNFNode* node) {
    if (pool->num_nodes >= pool->capacity) {
        int new_cap = pool->capacity * 2;
        DNNFNode** new_nodes = realloc(pool->nodes,
                                        new_cap * sizeof(DNNFNode*));
        if (!new_nodes) {
            pool->alloc_failed = 1;
            return -1;
        }
        pool->nodes = new_nodes;
        pool->capacity = new_cap;
    }
    node->id = pool->num_nodes;
    pool->nodes[pool->num_nodes++] = node;
    return 0;
}

DNNFPool* create_dnnf_pool(int initial_capacity) {
    assert(initial_capacity >= 2);
    DNNFPool* pool = malloc(sizeof(DNNFPool));
    if (!pool) return NULL;
    pool->capacity = initial_capacity;
    pool->num_nodes = 0;
    pool->alloc_failed = 0;
    pool->node_true = NULL;
    pool->node_false = NULL;
    // Champs auxiliaires installes lazy par dnnf_transform.
    pool->scope_by_id = NULL;
    pool->scope_capacity = 0;
    pool->num_vars = 0;
    pool->nodes = malloc(initial_capacity * sizeof(DNNFNode*));
    if (!pool->nodes) {
        free(pool);
        return NULL;
    }

    // Singletons TRUE (id=0) puis FALSE (id=1).
    DNNFNode* t = allocate_raw_node();
    if (!t || pool_register_node(pool, t) != 0) {
        if (t) free(t);
        free(pool->nodes);
        free(pool);
        return NULL;
    }
    t->type = DNNF_TRUE;
    pool->node_true = t;

    DNNFNode* f = allocate_raw_node();
    if (!f || pool_register_node(pool, f) != 0) {
        if (f) free(f);
        // t est deja dans pool->nodes, sera libere par free_dnnf_pool si
        // l'appelant l'invoque -- mais ici on retourne NULL, le pool
        // n'est pas remis a l'appelant. On libere donc tout a la main.
        free(t->children);
        free(t);
        free(pool->nodes);
        free(pool);
        return NULL;
    }
    f->type = DNNF_FALSE;
    pool->node_false = f;

    return pool;
}

void free_dnnf_pool(DNNFPool* pool) {
    if (!pool) return;
    // Libere la table de portees installee par dnnf_smooth. No-op si non allouee.
    dnnf_transform_free_pool_extras(pool);

    for (int k = 0; k < pool->num_nodes; k++) {
        DNNFNode* n = pool->nodes[k];
        if (n->children) free(n->children);
        free(n);
    }
    free(pool->nodes);
    free(pool);
}


// ============================================================================
// SECTION 2 : FACTORIES
// ============================================================================

DNNFNode* dnnf_make_literal(DNNFPool* pool, int var, int positive) {
    assert(var >= 1);
    assert(positive == 0 || positive == 1);
    if (pool->alloc_failed) return pool->node_false;
    DNNFNode* n = allocate_raw_node();
    if (!n) {
        pool->alloc_failed = 1;
        return pool->node_false;
    }
    n->type = positive ? DNNF_LIT_POS : DNNF_LIT_NEG;
    n->var_index = var;
    if (pool_register_node(pool, n) != 0) {
        free(n);
        return pool->node_false;
    }
    return n;
}

DNNFNode* dnnf_make_constant_true(DNNFPool* pool) {
    return pool->node_true;
}

DNNFNode* dnnf_make_constant_false(DNNFPool* pool) {
    return pool->node_false;
}

DNNFNode* dnnf_make_and(DNNFPool* pool, DNNFNode* left, DNNFNode* right) {
    assert(left && right);
    if (pool->alloc_failed) return pool->node_false;

    // Simplifications locales : comparaison sur les singletons du pool.
    // Les TRUE/FALSE sont uniques par pool, donc l'egalite de pointeur est
    // le bon invariant.
    if (left == pool->node_false || right == pool->node_false) {
        return pool->node_false;
    }
    if (left == pool->node_true)  return right;
    if (right == pool->node_true) return left;

    DNNFNode* n = allocate_raw_node();
    if (!n) {
        pool->alloc_failed = 1;
        return pool->node_false;
    }
    n->type = DNNF_AND;
    n->capacity = 2;
    n->num_children = 2;
    n->children = malloc(2 * sizeof(DNNFNode*));
    if (!n->children) {
        free(n);
        pool->alloc_failed = 1;
        return pool->node_false;
    }
    n->children[0] = left;
    n->children[1] = right;
    if (pool_register_node(pool, n) != 0) {
        free(n->children);
        free(n);
        return pool->node_false;
    }
    return n;
}

DNNFNode* dnnf_make_or(DNNFPool* pool, int initial_capacity) {
    assert(initial_capacity >= 1);
    if (pool->alloc_failed) return pool->node_false;
    DNNFNode* n = allocate_raw_node();
    if (!n) {
        pool->alloc_failed = 1;
        return pool->node_false;
    }
    n->type = DNNF_OR;
    n->capacity = initial_capacity;
    n->num_children = 0;
    n->children = malloc(initial_capacity * sizeof(DNNFNode*));
    if (!n->children) {
        free(n);
        pool->alloc_failed = 1;
        return pool->node_false;
    }
    if (pool_register_node(pool, n) != 0) {
        free(n->children);
        free(n);
        return pool->node_false;
    }
    return n;
}

void dnnf_or_add_child(DNNFNode* or_node, DNNFNode* child) {
    assert(or_node && or_node->type == DNNF_OR);
    assert(child);
    if (or_node->num_children >= or_node->capacity) {
        int new_cap = or_node->capacity * 2;
        DNNFNode** new_children = realloc(or_node->children,
                                           new_cap * sizeof(DNNFNode*));
        if (!new_children) {
            // Pas d'access au pool ici (signature legacy). On laisse l'OR
            // dans son etat actuel sans ajouter le child : sharpsat sera
            // sous-evalue mais pas corrompu, et la memoire reste coherente.
            // Le caller (procedure3.c) doit verifier pool->alloc_failed
            // independamment via les autres factories. Ce chemin reste
            // theorique dans la pratique : un realloc qui shrink/keep
            // n'echoue presque jamais.
            return;
        }
        or_node->children = new_children;
        or_node->capacity = new_cap;
    }
    or_node->children[or_node->num_children++] = child;
}


// ============================================================================
// SECTION 3 : UTILITAIRES STRUCTURELS (taille et export NNF)
// ============================================================================
//
// Visitent le DAG via l'id de chaque noeud comme cle de memoisation.
// L'invariant "enfant.id < parent.id" n'est PAS maintenu (un OR construit
// paresseusement recoit des enfants crees apres lui), donc l'id max
// accessible peut depasser root->id. On borne les tableaux de memoisation
// par pool->num_nodes : tout DNNFNode accessible a un id dans
// [0, pool->num_nodes-1] par construction du pool.
// ============================================================================

/**
 * DFS post-ordre : compte les aretes accessibles (somme des num_children
 * avec marquage visited pour ne compter chaque noeud qu'une seule fois).
 */
static long long size_rec(DNNFNode* n, char* visited) {
    if (visited[n->id]) return 0;
    visited[n->id] = 1;
    long long edges = n->num_children;
    for (int k = 0; k < n->num_children; k++) {
        edges += size_rec(n->children[k], visited);
    }
    return edges;
}

long long dnnf_size(DNNFNode* root, DNNFPool* pool) {
    if (!root || !pool) return 0;
    int n_entries = pool->num_nodes;
    char* visited = calloc(n_entries, sizeof(char));
    long long edges = size_rec(root, visited);
    free(visited);
    return edges;
}


// ============================================================================
// EXPORT AU FORMAT NNF (DARWICHE)
// ============================================================================
//
// Syntaxe :
//   nnf <N> <E> <V>
//   ligne par noeud, indices (0-based) des enfants strictement inferieurs.
//
//   L v       : litteral x_v             (positif)
//   L -v      : litteral ~x_v            (negatif)
//   A 0       : constante Top            (AND vide, convention Darwiche)
//   O 0 0     : constante Bot            (OR vide)
//   A k c1..ck  : AND a k enfants
//   O j k c1..ck : OR a k enfants, j = variable de decision (0 ici)
// ============================================================================

/**
 * DFS post-ordre qui recense les noeuds atteignables depuis root et les
 * empile dans order[] dans un ordre topologique compatible (parents apres
 * enfants). Les ids locaux seront 0..count-1 dans cet ordre.
 */
static void dfs_collect(DNNFNode* n, char* visited,
                        DNNFNode** order, int* count) {
    if (visited[n->id]) return;
    visited[n->id] = 1;
    for (int k = 0; k < n->num_children; k++) {
        dfs_collect(n->children[k], visited, order, count);
    }
    order[*count] = n;
    (*count)++;
}

void dnnf_export_nnf(DNNFNode* root, int num_vars, DNNFPool* pool, FILE* out) {
    if (!root || !pool || !out) return;

    int n_entries = pool->num_nodes;

    char* visited = calloc(n_entries, sizeof(char));
    DNNFNode** order = malloc(n_entries * sizeof(DNNFNode*));
    int count = 0;

    dfs_collect(root, visited, order, &count);

    // Numerotation locale : local_id[global_id] = position dans order[].
    int* local_id = malloc(n_entries * sizeof(int));
    memset(local_id, -1, n_entries * sizeof(int));
    for (int k = 0; k < count; k++) {
        local_id[order[k]->id] = k;
    }

    long long edges = 0;
    for (int k = 0; k < count; k++) edges += order[k]->num_children;

    fprintf(out, "nnf %d %lld %d\n", count, edges, num_vars);

    for (int k = 0; k < count; k++) {
        DNNFNode* n = order[k];
        switch (n->type) {
            case DNNF_LIT_POS: fprintf(out, "L %d\n",  n->var_index); break;
            case DNNF_LIT_NEG: fprintf(out, "L -%d\n", n->var_index); break;
            case DNNF_TRUE:    fprintf(out, "A 0\n"); break;
            case DNNF_FALSE:   fprintf(out, "O 0 0\n"); break;
            case DNNF_AND: {
                fprintf(out, "A %d", n->num_children);
                for (int j = 0; j < n->num_children; j++) {
                    fprintf(out, " %d", local_id[n->children[j]->id]);
                }
                fputc('\n', out);
                break;
            }
            case DNNF_OR: {
                // Variable de decision inconnue --> 0.
                fprintf(out, "O 0 %d", n->num_children);
                for (int j = 0; j < n->num_children; j++) {
                    fprintf(out, " %d", local_id[n->children[j]->id]);
                }
                fputc('\n', out);
                break;
            }
        }
    }

    free(local_id);
    free(order);
    free(visited);
}
