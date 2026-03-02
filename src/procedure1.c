#include <stdio.h>
#include <stdlib.h>

#include "procedure1.h"

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

// Fonction cas de base
PS_Set* compute_leaf_ps_prime(Node* leaf, SAT_Formula* f, uint64_t all_clauses_mask) {
    // 1. Initialisation de l'ensemble
    PS_Set* ps_v = malloc(sizeof(PS_Set));
    ps_v->capacity = 2; // Il y a au maximum 2 affectations (Vrai ou Faux)
    ps_v->size = 0;
    ps_v->masks = malloc(ps_v->capacity * sizeof(uint64_t));

    // 2. Identifier la variable de cette feuille
    int x = leaf->var_index;

    // 3. Calcul du masque de filtrage : cla(F_v) = cla(F) \ delta(v)
    uint64_t mask_Fv = all_clauses_mask & ~(leaf->delta_clauses_mask);

    // 4. Cas 1 : Si on affecte x = Vrai (1)
    // On prend les clauses satisfaites par x=Vrai, et on applique le filtre
    uint64_t C_true = f->mask_pos[x] & mask_Fv;
    add_to_ps_set(ps_v, C_true);

    // 5. Cas 2 : Si on affecte x = Faux (0)
    // On prend les clauses satisfaites par x=Faux, et on applique le filtre
    uint64_t C_false = f->mask_neg[x] & mask_Fv;
    add_to_ps_set(ps_v, C_false);

    return ps_v;
}


// Implémentation de la Procédure 1
PS_Set* compute_ps_prime_bottom_up(Node* node, SAT_Formula* f, uint64_t all_clauses_mask){
    if (node->is_leaf) {
        node->ps_prime_v = compute_leaf_ps_prime(node, f, all_clauses_mask);
        return node->ps_prime_v;
    }

    // 1. Appels récursifs (Bottom-Up)
    PS_Set* ps_c1 = compute_ps_prime_bottom_up(node->left, f, all_clauses_mask);
    PS_Set* ps_c2 = compute_ps_prime_bottom_up(node->right, f, all_clauses_mask);

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