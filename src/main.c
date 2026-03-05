#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "formula.h"
#include "trie.h"
#include "procedure1.h"

// ============================================================================
// FONCTIONS UTILITAIRES POUR LE TEST MANUEL (création de l'arbre de l'exemple p. 67)
// ============================================================================

Node* create_leaf(NodeType type, int index, int num_clauses) {
    Node* n = malloc(sizeof(Node));
    n->type = type;
    n->index = index;
    n->left = NULL;
    n->right = NULL;
    n->delta_mask = create_bitset(num_clauses);
    
    // Si la feuille représente une clause, on active son bit dans la coupe locale
    if (type == NODE_LEAF_CLAUSE) {
        int word_idx = index / 64;
        int bit_idx = index % 64;
        n->delta_mask->words[word_idx] |= (1ULL << bit_idx);
    }
    return n;
}

Node* create_internal(Node* left, Node* right, int num_clauses) {
    Node* n = malloc(sizeof(Node));
    n->type = NODE_INTERNAL;
    n->index = 0;
    n->left = left;
    n->right = right;
    n->delta_mask = create_bitset(num_clauses);
    
    // Le masque (coupe) d'un parent est l'union logique des masques de ses enfants
    for(int i = 0; i < n->delta_mask->num_words; i++) {
        n->delta_mask->words[i] = left->delta_mask->words[i] | right->delta_mask->words[i];
    }
    return n;
}

// ============================================================================
// CONSTRUCTION DE LA FORMULE (Figure 2 du papier)
// ============================================================================

SAT_Formula create_figure2_formula() {
    SAT_Formula f;
    f.num_vars = 5;
    f.num_clauses = 4;

    // Allocation des tableaux de Bitsets (de l'indice 0 à num_vars)
    f.mask_pos = malloc((f.num_vars + 1) * sizeof(Bitset*));
    f.mask_neg = malloc((f.num_vars + 1) * sizeof(Bitset*));
    
    for(int i = 0; i <= f.num_vars; i++) {
        f.mask_pos[i] = create_bitset(f.num_clauses);
        f.mask_neg[i] = create_bitset(f.num_clauses);
    }

    // Clause c1 (index 0) : {x1, x2}
    f.mask_pos[1]->words[0] |= (1ULL << 0);
    f.mask_pos[2]->words[0] |= (1ULL << 0);

    // Clause c2 (index 1) : {x1, ¬x2, x3}
    f.mask_pos[1]->words[0] |= (1ULL << 1);
    f.mask_neg[2]->words[0] |= (1ULL << 1);
    f.mask_pos[3]->words[0] |= (1ULL << 1);

    // Clause c3 (index 2) : {x2, ¬x4, x5}
    f.mask_pos[2]->words[0] |= (1ULL << 2);
    f.mask_neg[4]->words[0] |= (1ULL << 2);
    f.mask_pos[5]->words[0] |= (1ULL << 2);

    // Clause c4 (index 3) : {x2, x3, x5}
    f.mask_pos[2]->words[0] |= (1ULL << 3);
    f.mask_pos[3]->words[0] |= (1ULL << 3);
    f.mask_pos[5]->words[0] |= (1ULL << 3);

    return f;
}

// ============================================================================
// AFFICHAGE
// ============================================================================

void print_binary(Bitset* b, int num_clauses) {
    // Affiche les bits de gauche (clause de plus grand indice) à droite (clause d'indice 0)
    for (int i = num_clauses - 1; i >= 0; i--) {
        int word_idx = i / 64;
        int bit_idx = i % 64;
        uint64_t bit = (b->words[word_idx] >> bit_idx) & 1ULL;
        printf("%llu", bit);
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    printf("Initialisation de la formule (Figure 2)...\n");
    SAT_Formula f = create_figure2_formula();

    // masque universel contenant toutes les clauses
    Bitset* all_clauses_mask = create_bitset(f.num_clauses);
    all_clauses_mask->words[0] = (1ULL << f.num_clauses) - 1; // 1111 en binaire

    BinaryTrie* trie = create_trie(1024);


    // Construction manuelle du sous-arbre pour le nœud 'v' de la Figure 2 page 67
    // Noeud v couvre les variables {x1, x2} et les clauses {c1, c3}.
    // Indexation de nos clauses : c1=0, c2=1, c3=2, c4=3.
    
    // Branche gauche (variables x1 et x2)
    Node* leaf_x1 = create_leaf(NODE_LEAF_VAR, 1, f.num_clauses);
    Node* leaf_x2 = create_leaf(NODE_LEAF_VAR, 2, f.num_clauses);
    Node* u1 = create_internal(leaf_x1, leaf_x2, f.num_clauses);

    // Branche droite (clauses c1 et c3)
    Node* leaf_c1 = create_leaf(NODE_LEAF_CLAUSE, 0, f.num_clauses);
    Node* leaf_c3 = create_leaf(NODE_LEAF_CLAUSE, 2, f.num_clauses);
    Node* u2 = create_internal(leaf_c1, leaf_c3, f.num_clauses);

    // noeud v (parent de u1 et u2)
    Node* v = create_internal(u1, u2, f.num_clauses);

    printf("Calcul bottom-up de PS'(F_v) via la Procedure 1...\n");
    PS_Set* result = compute_ps_prime_bottom_up(v, &f, all_clauses_mask, trie);


    printf("\n=== RESULTATS AU NOEUD v ===\n");
    printf("Taille de PS'(F_v) trouvee : %d\n", result->size);
    printf("Nombre total de noeuds alloues dans le Trie : %d\n", trie->next_free);
    printf("\nMasques retenus (Bits: c4 c3 c2 c1) :\n");
    
    for (int i = 0; i < result->size; i++) {
        printf("  - Ensemble ID %d : ", result->ps_ids[i]);
        print_binary(result->sets[i], f.num_clauses);
        printf("\n");
    }

    return 0;
}

// ============================================================================
// CALCUL DE LA PS-WIDTH DE L'ARBRE
// ============================================================================
// Parcourt l'arbre récursivement pour trouver la plus grande taille de PS'(F_v)
int calculate_tree_ps_width(Node* node) {
    if (!node || !node->ps_prime_v) return 0;
    
    int max_width = node->ps_prime_v->size;
    
    int left_width = calculate_tree_ps_width(node->left);
    int right_width = calculate_tree_ps_width(node->right);
    
    if (left_width > max_width) max_width = left_width;
    if (right_width > max_width) max_width = right_width;
    
    return max_width;
}

// ============================================================================
// Génération aléatoire de l'arbre
// ============================================================================
//int main() {
//    printf("Initialisation de la formule...\n");
//    SAT_Formula f = create_figure2_formula();
//
//    Bitset* all_clauses_mask = create_bitset(f.num_clauses);
//    all_clauses_mask->words[0] = (1ULL << f.num_clauses) - 1;
//
//    BinaryTrie* trie = create_trie(1024);
//
//    printf("Generation d'un arbre de decomposition aleatoire...\n");
//    Node* root = generate_random_tree(&f);
//
//    printf("Execution de la Procedure 1 (Bottom-Up)...\n");
//    compute_ps_prime_bottom_up(root, &f, all_clauses_mask, trie);
//
//    // Extraction de la ps-width (le maximum global de l'arbre)
//    int ps_width = calculate_tree_ps_width(root);
//
//    printf("\n=== RESULTATS DE L'ARBRE ALEATOIRE ===\n");
//    printf("PS-width de cette decomposition : %d\n", ps_width);
//    printf("Noeuds uniques generes dans le Trie : %d\n", trie->num_ps_sets);
//
//    // Libération mémoire
//    free_tree(root);
//    free_trie(trie);
//    free_bitset(all_clauses_mask);
//
//    return 0;
//}