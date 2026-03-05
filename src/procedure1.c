#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "procedure1.h"

// ============================================================================
// UTILITAIRES BITSET
// ============================================================================

Bitset* create_bitset(int num_clauses) {
    Bitset* b = malloc(sizeof(Bitset));
    b->num_words = (num_clauses + 63) / 64; // arrondi supérieur
    b->words = calloc(b->num_words, sizeof(uint64_t)); // met tout à 0
    return b;
}


void free_bitset(Bitset* b) {
    if (b) {
        if (b->words) free(b->words);
        free(b);
    }
}


Bitset* bitset_copy(Bitset* src, int num_clauses) {
    Bitset* dest = create_bitset(num_clauses);
    memcpy(dest->words, src->words, src->num_words * sizeof(uint64_t));
    return dest;
}


// dest = (b1 U b2) ∩ filter_mask
void bitset_union_and_filter(Bitset* dest, Bitset* b1, Bitset* b2, Bitset* filter_mask) {
    for (int i = 0; i < dest->num_words; i++) {
        uint64_t un = b1->words[i] | b2->words[i];
        dest->words[i] = un & filter_mask->words[i];
    }
}



// ============================================================================
// GESTION DU PS_SET LOCAL (Pour un noeud de l'arbre)
// ============================================================================

PS_Set* create_ps_set(int initial_capacity) {
    PS_Set* set = malloc(sizeof(PS_Set));
    set->capacity = initial_capacity;
    set->size = 0;
    set->sets = malloc(set->capacity * sizeof(Bitset*));
    set->ps_ids = malloc(set->capacity * sizeof(int));
    return set;
}


// Ajoute un Bitset au PS_Set du noeud SEULEMENT s'il n'y est pas déjà
void add_to_node_ps_set(PS_Set* set, Bitset* new_mask, int ps_id) {
    // Élimination locale des doublons grâce à l'identifiant entier
    for (int i = 0; i < set->size; i++) {
        if (set->ps_ids[i] == ps_id) {
            free_bitset(new_mask); // libérer la mémoire temporaire
            return;
        }
    }
    
    // ajout si unique
    if (set->size == set->capacity) {
        set->capacity *= 2;
        set->sets = realloc(set->sets, set->capacity * sizeof(Bitset*));
        set->ps_ids = realloc(set->ps_ids, set->capacity * sizeof(int));
    }
    set->sets[set->size] = new_mask;
    set->ps_ids[set->size] = ps_id;
    set->size++;
}



// ============================================================================
// PROCEDURE 1 : GENERATION DE PS'(F_v)
// ============================================================================

PS_Set* compute_leaf_ps_prime(Node* leaf, SAT_Formula* f, Bitset* mask_Fv, BinaryTrie* trie) {
    PS_Set* ps_v = create_ps_set(2);

    if (leaf->type == NODE_LEAF_VAR) {
        int x = leaf->index;

        // Cas 1 : variable x = Vrai (mask_pos[x] & mask_Fv)
        Bitset* C_true = create_bitset(f->num_clauses);
        for(int i = 0; i < C_true->num_words; i++) {
            C_true->words[i] = f->mask_pos[x]->words[i] & mask_Fv->words[i];
        }
        int id_true = insert_or_get_ps_set(trie, C_true, f->num_clauses);
        add_to_node_ps_set(ps_v, C_true, id_true);

        // Cas 2 : variable x = Faux (mask_neg[x] & mask_Fv)
        Bitset* C_false = create_bitset(f->num_clauses);
        for(int i = 0; i < C_false->num_words; i++) {
            C_false->words[i] = f->mask_neg[x]->words[i] & mask_Fv->words[i];
        }
        int id_false = insert_or_get_ps_set(trie, C_false, f->num_clauses);
        add_to_node_ps_set(ps_v, C_false, id_false);

    } else if (leaf->type == NODE_LEAF_CLAUSE) {
        // feuille clause = ensemble vide
        Bitset* C_empty = create_bitset(f->num_clauses);
        int id_empty = insert_or_get_ps_set(trie, C_empty, f->num_clauses);
        add_to_node_ps_set(ps_v, C_empty, id_empty);
    }

    return ps_v;
}

PS_Set* compute_ps_prime_bottom_up(Node* node, SAT_Formula* f, Bitset* all_clauses_mask, BinaryTrie* trie) {
    // mask_Fv = cla(F) \ delta(v)
    Bitset* mask_Fv = create_bitset(f->num_clauses);
    for(int i = 0; i < mask_Fv->num_words; i++) {
        mask_Fv->words[i] = all_clauses_mask->words[i] & ~(node->delta_mask->words[i]);
    }

    // condition d'arrêt (feuille)
    if (node->type == NODE_LEAF_VAR || node->type == NODE_LEAF_CLAUSE) {
        PS_Set* leaf_set = compute_leaf_ps_prime(node, f, mask_Fv, trie);
        free_bitset(mask_Fv);
        
        node->ps_prime_v = leaf_set; 
        return leaf_set;
    }

    PS_Set* ps_c1 = compute_ps_prime_bottom_up(node->left, f, all_clauses_mask, trie);
    PS_Set* ps_c2 = compute_ps_prime_bottom_up(node->right, f, all_clauses_mask, trie);

    // ensemble L pour le noeud courant
    PS_Set* ps_v = create_ps_set(16);

    // (C1 U C2) ∩ cla(Fv)
    for (int i = 0; i < ps_c1->size; i++) {
        for (int j = 0; j < ps_c2->size; j++) {
            Bitset* C1 = ps_c1->sets[i];
            Bitset* C2 = ps_c2->sets[j];
            
            Bitset* C_filtered = create_bitset(f->num_clauses);
            bitset_union_and_filter(C_filtered, C1, C2, mask_Fv);
            
            // vérifie dans le binary trie pour récupérer un entier unique
            int id = insert_or_get_ps_set(trie, C_filtered, f->num_clauses);
            
            // tente de l'ajouter à l'ensemble du noeud
            add_to_node_ps_set(ps_v, C_filtered, id);
        }
    }

    free_bitset(mask_Fv);
    
    node->ps_prime_v = ps_v; 
    return ps_v;
}