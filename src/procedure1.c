#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// Structure pour représenter un ensemble PS' dynamique
typedef struct {
    uint64_t* masks;
    int size;
    int capacity;
} PS_Set;

// Structure pour un noeud de l'arbre de décomposition
typedef struct Node {
    bool is_leaf;
    uint64_t delta_clauses_mask; // Masque des clauses dans delta(v)
    struct Node* left;
    struct Node* right;
    
    PS_Set* ps_prime_v;          // L'ensemble PS'(F_v) que l'on va calculer
} Node;

// Fonction utilitaire pour ajouter un masque sans doublon (remplace le set() Python ou le Trie)
void add_to_ps_set(PS_Set* set, uint64_t mask) {
    // Vérification des doublons en O(|cla(F)|) simulé ici par O(k)
    for (int i = 0; i < set->size; i++) {
        if (set->masks[i] == mask) return; // Doublon ignoré
    }
    
    // Ajout dynamique
    if (set->size == set->capacity) {
        set->capacity *= 2;
        set->masks = realloc(set->masks, set->capacity * sizeof(uint64_t));
    }
    set->masks[set->size++] = mask;
}

// Fonction utilitaire (à implémenter) pour le cas de base
PS_Set* compute_leaf_ps_prime(Node* leaf);

// ---------------------------------------------------------
// L'implémentation de la Procédure 1
// ---------------------------------------------------------
PS_Set* compute_ps_prime_bottom_up(Node* node, uint64_t all_clauses_mask) {
    if (node->is_leaf) {
        node->ps_prime_v = compute_leaf_ps_prime(node);
        return node->ps_prime_v;
    }

    // 1. Appels récursifs (Bottom-Up)
    PS_Set* ps_c1 = compute_ps_prime_bottom_up(node->left, all_clauses_mask);
    PS_Set* ps_c2 = compute_ps_prime_bottom_up(node->right, all_clauses_mask);

    // 2. Initialisation de l'ensemble vide (L dans le papier)
    PS_Set* ps_v = malloc(sizeof(PS_Set));
    ps_v->capacity = 16;
    ps_v->size = 0;
    ps_v->masks = malloc(ps_v->capacity * sizeof(uint64_t));

    // 3. Calcul du masque de filtrage : cla(F_v) = cla(F) \ delta(v)
    uint64_t mask_Fv = all_clauses_mask & ~(node->delta_clauses_mask);

    // 4. Produit cartésien et filtrage
    for (int i = 0; i < ps_c1->size; i++) {
        for (int j = 0; j < ps_c2->size; j++) {
            
            uint64_t C1 = ps_c1->masks[i];
            uint64_t C2 = ps_c2->masks[j];
            
            // Ligne 3 de la Procédure 1: add (C1 U C2) cap cla(F_v) to L
            uint64_t C_union = C1 | C2;
            uint64_t C_filtered = C_union & mask_Fv;
            
            // Ajout avec vérification des doublons
            add_to_ps_set(ps_v, C_filtered);
        }
    }

    node->ps_prime_v = ps_v;
    return ps_v;
}