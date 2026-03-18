#include "procedure2.h"
#include "../utils/ps_set.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>


// ============================================================================
// PROCEDURE 2 : GENERATION DE PS'(F_v_barre)
// ============================================================================

static void compute_ps_bar_recursive(Node* parent, SAT_Formula* f, BinaryTrie* trie) {
    if (!parent || parent->type != NODE_INTERNAL) return;
    
    Node* left = parent->left;
    Node* right = parent->right;
    if (!left || !right) return;
    
    // --- Calcul de PS'(F_left_barre) ---
    {
        PS_Set* ps_sibling = right->ps_prime_v; // PS'(F_right) = PS'(F_sibling_de_left) (calculé procédure 1)
        PS_Set* ps_bar_parent = parent->ps_prime_v_barre; // PS'(F_parent_barre) (calculer dans l'étape précédente top-down)
        
        left->ps_prime_v_barre = create_ps_set(ps_sibling->size * ps_bar_parent->size + 1);
        
        for (int i = 0; i < ps_sibling->size; i++) {
            for (int j = 0; j < ps_bar_parent->size; j++) {
                Bitset* Cs = ps_sibling->sets[i];
                Bitset* Cp = ps_bar_parent->sets[j];
                
                Bitset* result = create_bitset(f->num_clauses);
                for (int w = 0; w < result->num_words; w++) {
                    result->words[w] = (Cs->words[w] | Cp->words[w]) & left->delta_mask->words[w];
                }
                
                int id = insert_or_get_ps_set(trie, result, f->num_clauses);
                add_to_node_ps_set(left->ps_prime_v_barre, result, id, left->id, trie);
            }
        }
    }
    
    // --- Calcul de PS'(F_right_barre) ---
    {
        PS_Set* ps_sibling = left->ps_prime_v; // PS'(F_left_barre) = PS'(F_sibling_de_right)
        PS_Set* ps_bar_parent = parent->ps_prime_v_barre; //même principe, tout est déjà pré-calculé
        
        right->ps_prime_v_barre = create_ps_set(ps_sibling->size * ps_bar_parent->size + 1);
        
        for (int i = 0; i < ps_sibling->size; i++) {
            for (int j = 0; j < ps_bar_parent->size; j++) {
                Bitset* Cs = ps_sibling->sets[i];
                Bitset* Cp = ps_bar_parent->sets[j];
                
                Bitset* result = create_bitset(f->num_clauses);
                for (int w = 0; w < result->num_words; w++) {
                    result->words[w] = (Cs->words[w] | Cp->words[w]) & right->delta_mask->words[w];
                }
                
                int id = insert_or_get_ps_set(trie, result, f->num_clauses);
                add_to_node_ps_set(right->ps_prime_v_barre, result, id, right->id, trie);
            }
        }
    }
    
    // descente récursive
    compute_ps_bar_recursive(left, f, trie);
    compute_ps_bar_recursive(right, f, trie);
}


void compute_ps_bar_top_down(Node* root, SAT_Formula* f, BinaryTrie* trie) {
    if (!root || !root->ps_prime_v) return;
    
    // réinitialiser le seen_array du trie (pollué par procédure1 avec les node_id utilisés pour dédupliquer PS'(Fv))
    memset(trie->seen_array, 0, trie->seen_capacity * sizeof(int)); // si on ne reset pas, add_to_node_ps_set rejette des PS-sets valides en croyant qu'ils sont des doublons (faux positif car même node_id, mais PS_Set différent).
    
    // cas de base : PS'(F_racine_barre) = {vide = 0000...000}
    root->ps_prime_v_barre = create_ps_set(1);
    Bitset* empty_set = create_bitset(f->num_clauses);
    int empty_id = insert_or_get_ps_set(trie, empty_set, f->num_clauses);
    add_to_node_ps_set(root->ps_prime_v_barre, empty_set, empty_id, root->id, trie);
    
    // propagation top-down
    compute_ps_bar_recursive(root, f, trie);
}

