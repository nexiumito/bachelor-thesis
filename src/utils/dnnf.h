#ifndef DNNF_H
#define DNNF_H

#include <stdio.h>

// ============================================================================
// DNNF : CONSTRUCTION D'UN DAG D-DNNF PENDANT LA PROCEDURE 3
// ============================================================================
//
// Implementation de la Section 3.3 (Lemmes 4 a 7) de Bova, Capelli, Mengel,
// Slivovsky 2016, "On Compiling CNFs into Structured Deterministic DNNFs".
//
// Chaque cellule Tab_v(C, C') de la Procedure 3 se voit associer un pointeur
// DNNFNode* note phi_v(C, C') (Lemme 4). Parcourir le DAG construit en
// remplacant "ou --> +" et "et --> x" retourne exactement sharpsat_count.
//
// Le DAG est possede par un DNNFPool unique (arena allocator). Aucun autre
// conteneur (DPTable, DPResult) n'appelle free() sur les DNNFNode pointes.
//
// REQUETES (CO, VA, CT, ME) : voir utils/query/.
// ============================================================================

/**
 * Type de noeud du DAG d-DNNF.
 *
 * Feuilles :
 *   - DNNF_LIT_POS / DNNF_LIT_NEG : litteraux x_i / ~x_i (var_index in 1..n).
 *   - DNNF_TRUE / DNNF_FALSE       : constantes, partagees comme singletons.
 * Internes :
 *   - DNNF_AND : conjonction decomposable (portees d'enfants disjointes).
 *   - DNNF_OR  : disjonction deterministe (ensembles de modeles disjoints).
 */
typedef enum {
    DNNF_AND,       // et-gate, decomposable
    DNNF_OR,        // ou-gate, deterministe
    DNNF_LIT_POS,   // litteral positif  x_i
    DNNF_LIT_NEG,   // litteral negatif ~x_i
    DNNF_TRUE,      // constante Top
    DNNF_FALSE      // constante Bot
} dnnf_node_type;

/**
 * Noeud du DAG d-DNNF.
 *
 * Invariants :
 *   1. id >= 0, assigne a la creation, jamais reecrit.
 *   2. Feuilles (LIT_*, TRUE, FALSE) : children == NULL, num_children == 0.
 *   3. AND : portees d'enfants disjointes (garantie structurellement par la
 *      branch decomposition, pas verifiee dynamiquement).
 *   4. TRUE / FALSE sont singletons par pool.
 */
typedef struct dnnf_node {
    int                 id;             // identifiant global, strictement croissant
    dnnf_node_type      type;
    int                 var_index;      // 1..n pour LIT_POS/LIT_NEG, 0 sinon
    struct dnnf_node**  children;       // NULL pour feuilles/constantes
    int                 num_children;
    int                 capacity;       // capacite allouee, doublee en cas de besoin
} DNNFNode;

/**
 * Pool (arena allocator) proprietaire de tous les DNNFNode alloues.
 *
 * Invariants :
 *   1. nodes[k]->id == k pour tout k < num_nodes.
 *   2. node_true et node_false sont dans nodes[] (ids 0 et 1).
 *   3. Tous les DNNFNode* retournes par l'API pointent dans ce pool.
 */
typedef struct {
    DNNFNode**      nodes;          // pool : tous les DNNFNode alloues
    int             num_nodes;
    int             capacity;
    DNNFNode*       node_true;      // singleton Top
    DNNFNode*       node_false;     // singleton Bot
    // Drapeau sticky : passe a 1 des qu'une allocation interne (malloc d'un
    // DNNFNode, realloc du tableau pool->nodes, ou realloc de la liste
    // d'enfants d'un OR) a echoue. Une fois leve, les factories deviennent
    // des no-op qui retournent ``node_false`` (semantique conservatrice :
    // un sous-arbre absent compte 0 modele). Le caller (solve_dp / main)
    // doit verifier ce drapeau pour router vers print_json_error.
    int             alloc_failed;
} DNNFPool;

// ============================================================================
// construction du DAG
// ============================================================================

/**
 * Cree un pool initialise avec les deux singletons TRUE (id=0) et FALSE (id=1).
 *
 * @param initial_capacity  Capacite initiale du tableau de pointeurs (>= 2).
 * @return                  Pool alloue, ou NULL si une allocation initiale
 *                          echoue (le caller doit emettre print_json_error).
 */
DNNFPool* create_dnnf_pool(int initial_capacity);

/**
 * Libere tous les DNNFNode du pool, leurs tableaux children, puis le pool.
 * Accepte NULL (no-op).
 */
void free_dnnf_pool(DNNFPool* pool);

/**
 * Cree une feuille litterale x_var (positive si positive==1, negative sinon).
 * Precondition : var >= 1, positive in {0,1}.
 */
DNNFNode* dnnf_make_literal(DNNFPool* pool, int var, int positive);

/** Retourne le singleton TRUE du pool (O(1), pas d'allocation). */
DNNFNode* dnnf_make_constant_true(DNNFPool* pool);

/** Retourne le singleton FALSE du pool (O(1), pas d'allocation). */
DNNFNode* dnnf_make_constant_false(DNNFPool* pool);

/**
 * Construit un AND a deux enfants avec simplifications locales :
 *   - left ou right == FALSE --> retourne FALSE (singleton).
 *   - left == TRUE --> retourne right (symetrique pour right).
 * Sinon alloue un nouveau noeud DNNF_AND. Precondition : left, right non-NULL.
 *
 * Decomposabilite non verifiee : garantie par le site d'appel (procedure3.c).
 */
DNNFNode* dnnf_make_and(DNNFPool* pool, DNNFNode* left, DNNFNode* right);

/**
 * Cree un OR vide (num_children=0). L'appelant l'alimente via
 * dnnf_or_add_child puis decide s'il le conserve (un OR a zero enfant
 * NE DOIT PAS etre stocke : cela casserait l'invariant de comptage).
 *
 * @param initial_capacity  Capacite initiale du tableau children (>= 1).
 */
DNNFNode* dnnf_make_or(DNNFPool* pool, int initial_capacity);

/**
 * Ajoute un enfant a un noeud OR existant. Double la capacite si plein.
 * Precondition : or_node->type == DNNF_OR, child non-NULL.
 */
void dnnf_or_add_child(DNNFNode* or_node, DNNFNode* child);

// ============================================================================
// Utilitaires structurels
// ============================================================================

/**
 * Taille |D| = nombre d'aretes accessibles depuis root (somme des
 * num_children avec marquage). Complexite O(|D|).
 */
long long dnnf_size(DNNFNode* root, DNNFPool* pool);

/**
 * Ecrit le DAG au format NNF Darwiche.
 * Premiere ligne : "nnf N E V" ou N=nombre de noeuds, E=nombre d'aretes,
 * V=num_vars. Une ligne par noeud en ordre topologique (id local croissant).
 */
void dnnf_export_nnf(DNNFNode* root, int num_vars, DNNFPool* pool, FILE* out);

#endif
