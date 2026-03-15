#include "procedure2.h"
#include "../utils/ps_set.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

void process_node_proc2(Node* u, BinaryTrie* trie, SAT_Formula* f) {
    if (!u || u->type != NODE_INTERNAL) return;

    Node* left = u->left;
    Node* right = u->right;

    if (!left || !right || !left->ps_prime_v || !right->ps_prime_v) return;

    // vérifier l'appartenance à P''(u)
    int cap = trie->seen_capacity;
    bool* valid_F = calloc(cap, sizeof(bool));
    for (int i = 0; i < u->ps_double_prime_v->size; i++) {
        int ps_id = u->ps_double_prime_v->ps_ids[i];
        if (ps_id < cap) valid_F[ps_id] = true;
    }

    // tableaux pour marquer quels masques de P'(left) et P'(right) survivent
    bool* keep_left = calloc(left->ps_prime_v->size, sizeof(bool));
    bool* keep_right = calloc(right->ps_prime_v->size, sizeof(bool));

    // masque temporaire réutilisé pour éviter les allocations en boucle
    Bitset* f_cut = create_bitset(f->num_clauses);

    // produit cartésien 
    for (int i = 0; i < left->ps_prime_v->size; i++) {
        Bitset* mask_left = left->ps_prime_v->sets[i];
        
        for (int j = 0; j < right->ps_prime_v->size; j++) {
            Bitset* mask_right = right->ps_prime_v->sets[j];

            // opération inline : F' = (F_1 \cup F_2) \cap \delta(u)
            for (int w = 0; w < f_cut->num_words; w++) {
                f_cut->words[w] = (mask_left->words[w] | mask_right->words[w]) & u->delta_mask->words[w];
            }

            // récupère l'ID
            int f_prime_id = insert_or_get_ps_set(trie, f_cut, f->num_clauses);

            // si ce masque fait partie de P''(u), alors F_1 et F_2 sont valides
            if (f_prime_id < cap && valid_F[f_prime_id]) {
                keep_left[i] = true;
                keep_right[j] = true;
            }
        }
    }
    
    free_bitset(f_cut);
    free(valid_F);

    // construction de P''(left)
    left->ps_double_prime_v = create_ps_set(left->ps_prime_v->size);
    for (int i = 0; i < left->ps_prime_v->size; i++) {
        if (keep_left[i]) {
            int idx = left->ps_double_prime_v->size;
            left->ps_double_prime_v->sets[idx] = bitset_copy(left->ps_prime_v->sets[i], f->num_clauses);
            left->ps_double_prime_v->ps_ids[idx] = left->ps_prime_v->ps_ids[i];
            left->ps_double_prime_v->size++;
        }
    }

    // construction de P''(right)
    right->ps_double_prime_v = create_ps_set(right->ps_prime_v->size);
    for (int j = 0; j < right->ps_prime_v->size; j++) {
        if (keep_right[j]) {
            int idx = right->ps_double_prime_v->size;
            right->ps_double_prime_v->sets[idx] = bitset_copy(right->ps_prime_v->sets[j], f->num_clauses);
            right->ps_double_prime_v->ps_ids[idx] = right->ps_prime_v->ps_ids[j];
            right->ps_double_prime_v->size++;
        }
    }

    free(keep_left);
    free(keep_right);

    // top-down (on descend)
    process_node_proc2(left, trie, f);
    process_node_proc2(right, trie, f);
}

void compute_ps_double_prime_top_down(Node* root, SAT_Formula* f, BinaryTrie* trie) {
    if (!root || !root->ps_prime_v) return;

    // à la racine l'ensemble \delta(root) est vide.
    root->ps_double_prime_v = create_ps_set(1);

    Bitset* empty_set = create_bitset(f->num_clauses);
    int empty_id = insert_or_get_ps_set(trie, empty_set, f->num_clauses);

    bool is_sat = false;
    for (int i = 0; i < root->ps_prime_v->size; i++) {
        if (root->ps_prime_v->ps_ids[i] == empty_id) {
            is_sat = true;
            break;
        }
    }

    if (is_sat) {
        printf("[RESULTAT] La formule est SATISFIABLE ! \n");
        printf("           (L'ensemble vide a atteint la racine)\n");
        
        // initialiser P''(root) avec l'ensemble vide
        root->ps_double_prime_v->sets[0] = bitset_copy(empty_set, f->num_clauses);
        root->ps_double_prime_v->ps_ids[0] = empty_id;
        root->ps_double_prime_v->size = 1;
    } else {
        printf("[RESULTAT] La formule est INSATISFIABLE ! \n");
        printf("           (Aucune affectation valide n'atteint la racine)\n");
        free_bitset(empty_set);
        return;  // rien à filtrer 
    }

    free_bitset(empty_set);

    // descente Top-Down
    process_node_proc2(root, trie, f);
}