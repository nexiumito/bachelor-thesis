#include "procedure2.h"
#include "../utils/ps_set.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>


// ============================================================================
// PROCEDURE 2 : GENERATION DE PS'(F_v_barre)
// ============================================================================

/**
 * Calcule récursivement PS'(F_v_barre) pour les enfants d'un noeud interne.
 *
 * Implémentation de la Procédure 2 du papier (page 69) :
 *   Pour un noeud v avec frère s et parent p :
 *      PS'(F_v_barre) = { (Cs OU Cp) ET delta(v) : Cs appartient à PS'(Fs), Cp appartient à PS'(F_p_barre) }
 *
 * Ici, pour l'enfant gauche (left) : le frère est right, et vice versa.
 * PS'(Fs) = ps_prime_v du frère (calculé par Procédure 1).
 * PS'(F_p_barre) = ps_prime_v_barre du parent (calculé à l'étape précédente).
 *
 * @param parent  Noeud interne dont on traite les enfants.
 * @param f       La formule SAT.
 * @param trie    Le trie binaire pour la déduplication.
 */
static void compute_ps_bar_recursive(Node* parent, SAT_Formula* f, BinaryTrie* trie) {
    if (!parent || parent->type != NODE_INTERNAL) return;
    
    Node* left = parent->left;
    Node* right = parent->right;
    if (!left || !right) return;
    
    // Calcul de PS'(F_left_barre)
    {
        PS_Set* ps_sibling = right->ps_prime_v; // PS'(F_right) = PS'(F_sibling_de_left) (calculé procédure 1)
        PS_Set* ps_bar_parent = parent->ps_prime_v_barre; // PS'(F_parent_barre) (calculer dans l'étape précédente top-down)
        
        left->ps_prime_v_barre = create_ps_set(ps_sibling->size * ps_bar_parent->size + 1);
        
        // prdduit cartésien (Cs OU Cp) ET cla(Fv_barre)
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
    
    // Calcul de PS'(F_right_barre)
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


/**
 * Point d'entrée de la Procédure 2 : calcul top-down de PS'(F_v_barre).
 *
 * Initialise le cas de base à la racine r : PS'(F_r_barre) = {vide}
 * (car F_r_barre n'a aucune variable, page 72 du papier).
 * Puis propage récursivement vers les feuilles via compute_ps_bar_recursive.
 *
 * Réinitialise d'abord le seen_array du trie (pollué par la Procédure 1)
 * pour que la déduplication fonctionne correctement avec les nouveaux node_id.
 *
 * Le résultat est stocké dans node->ps_prime_v_barre pour chaque noeud.
 *
 * @param root  Racine de l'arbre de décomposition.
 * @param f     La formule SAT.
 * @param trie  Le trie binaire.
 */
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