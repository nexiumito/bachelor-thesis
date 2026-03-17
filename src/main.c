#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>

#include "core/formula.h"
#include "core/parser.h"
#include "core/decomposition_tree.h"
#include "utils/bitset.h"
#include "utils/ps_set.h"
#include "utils/trie.h"
#include "algo/procedure1.h"
#include "algo/procedure2.h"

// ============================================================================
// AFFICHAGE
// ============================================================================

void print_binary(Bitset* b, int num_clauses) {
    // Affiche les bits de gauche (clause de plus grand indice) à droite (clause 0)
    for (int i = num_clauses - 1; i >= 0; i--) {
        int word_idx = i / 64;
        int bit_idx = i % 64;
        uint64_t bit = (b->words[word_idx] >> bit_idx) & 1ULL;
        printf("%llu", bit);
    }
}

// Affiche le contenu du PS'(Fv) d'un noeud 
void print_node_ps_set(Node* node, SAT_Formula* f, const char* node_name) {
    if (!node || !node->ps_prime_v) return;
    
    PS_Set* result = node->ps_prime_v;
    printf("\n=== PS'(F_%s_barre) ===\n", node_name);
    printf("Taille : %d\n", result->size);
    printf("Masques (Bits: c%d ... c1) :\n", f->num_clauses);
    
    for (int i = 0; i < result->size; i++) {
        printf("  - Ensemble ID %d : ", result->ps_ids[i]);
        print_binary(result->sets[i], f->num_clauses);
        printf("\n");
    }
}

// Affiche le contenu du PS'(Fv_barre) d'un noeud
void print_node_ps_barre_set(Node* node, SAT_Formula* f, const char* node_name) {
    if (!node || !node->ps_double_prime_v) return;
    
    PS_Set* result = node->ps_double_prime_v;
    printf("\n=== PS'(F_%s_barre) ===\n", node_name);
    printf("Taille : %d\n", result->size);
    printf("Masques (Bits: c%d ... c1) :\n", f->num_clauses);
    
    for (int i = 0; i < result->size; i++) {
        printf("  - Ensemble ID %d : ", result->ps_ids[i]);
        print_binary(result->sets[i], f->num_clauses);
        printf("\n");
    }
}

// ============================================================================
// MAIN
// ============================================================================

#define TREE_MANUAL 0
#define TREE_RANDOM 1
#define TREE_LINEAR 2

int main(int argc, char *argv[]) {
    // vérif du nombre d'arguments
    if (argc != 3) {
        printf("Erreur : Mauvais nombre d'arguments.\n\n");
        printf("Utilisation : %s <chemin_vers_fichier.cnf> <mode_arbre>\n", argv[0]);
        printf("Modes disponibles :\n");
        printf("  manual : Arbre manuel (Figure 2, page 67 du papier)\n");
        printf("  random : Arbre aleatoire pur\n");
        printf("  linear : Linear Branch Decomposition (optimise pour les structures locales)\n\n");
        printf("Exemple : %s script/instances_test/type3_k3_v40_c100_b20.cnf linear\n", argv[0]);
        return 1;
    }

    // recup des arguments
    char* filename = argv[1];
    char* mode_str = argv[2];
    int execution_mode = -1;

    // décodage du mode
    if (strcmp(mode_str, "manual") == 0) {
        execution_mode = TREE_MANUAL;
    } else if (strcmp(mode_str, "random") == 0) {
        execution_mode = TREE_RANDOM;
    } else if (strcmp(mode_str, "linear") == 0) {
        execution_mode = TREE_LINEAR;
    } else {
        printf("Erreur : Mode '%s' inconnu.\n", mode_str);
        printf("Veuillez choisir parmi : manual, random, linear\n");
        return 1;
    }

    // lecture de la formule dynamique
    printf("Lecture de la formule depuis le fichier CNF : %s\n", filename);
    SAT_Formula* f_ptr = parse_cnf(filename);
    if (!f_ptr) {
        printf("Erreur critique lors de l'ouverture ou du parsing du fichier.\n");
        return 1;
    }

    SAT_Formula f = *f_ptr;

    printf("=== CARACTERISTIQUES DE LA FORMULE ===\n");
    printf(" Variables : %d\n", f.num_vars);
    printf(" Clauses   : %d\n", f.num_clauses);
    printf("======================================\n");

    Bitset* all_clauses_mask = create_bitset(f.num_clauses);
    for (int i = 0; i < f.num_clauses; i++) {
        set_bit(all_clauses_mask, i);
    }

    // Si m est petit, on alloue au moins 100 000 pour être large
    int initial_capacity = (f.num_clauses * f.num_clauses > 100000) ? 
                       (f.num_clauses * f.num_clauses * 2) : 100000;
    BinaryTrie* trie = create_trie(initial_capacity);
    Node* root = NULL;

    // variables temps
    clock_t start_time, end_time;
    double time_used_tree = 0.0, time_used_proc1 = 0.0, time_used_proc2 = 0.0;

    // --- CONSTRUCTION DE L'ARBRE ---
    if (execution_mode == TREE_MANUAL) {
        printf("\n>>> EXECUTION : ARBRE MANUEL (Figure 2, page 67) <<<\n");
        
        // ================================================================
        // ARBRE COMPLET de la Figure 2 (page 67 du papier)
        // ================================================================
        // La formule a 5 variables (x1..x5) et 4 clauses (c1..c4).
        // L'arbre a 9 feuilles = 5 variables + 4 clauses.
        //
        // Structure lue depuis la figure :
        //
        //                    racine
        //                   /      \
        //                  v        F
        //                / \       / \
        //               A   B     E   x4
        //              / \ / \   / \
        //            x1 x2 c1 c3 C   D
        //                       / \ / \
        //                     x3 c4 x5 c2
        //
        // Correspondance indices :
        //   Variables : x1=1, x2=2, x3=3, x4=4, x5=5
        //   Clauses :   c1=0, c2=1, c3=2, c4=3
        //     c1 = {x1, x2}         -> index 0
        //     c2 = {x1, -x2, x3}    -> index 1
        //     c3 = {x2, -x4, x5}    -> index 2
        //     c4 = {x2, x3, x5}     -> index 3
        // ================================================================
        
        if (f.num_vars != 5 || f.num_clauses != 4) {
            printf("ERREUR : L'arbre manuel est concu pour la formule de la Figure 2 ");
            printf("(5 variables, 4 clauses). Formule actuelle : %d var, %d clauses.\n",
                   f.num_vars, f.num_clauses);
            free_formula(f_ptr);
            free_trie(trie);
            free_bitset(all_clauses_mask);
            return 1;
        }
        
        start_time = clock();
        
        // Sous-arbre gauche de la racine (noeud "v" du papier)
        Node* leaf_x1 = create_leaf_node(NODE_LEAF_VAR, 1, f.num_clauses);
        Node* leaf_x2 = create_leaf_node(NODE_LEAF_VAR, 2, f.num_clauses);
        Node* A = create_internal_node(leaf_x1, leaf_x2, f.num_clauses);  // (x1, x2)

        Node* leaf_c1 = create_leaf_node(NODE_LEAF_CLAUSE, 0, f.num_clauses);  // c1 = index 0
        Node* leaf_c3 = create_leaf_node(NODE_LEAF_CLAUSE, 2, f.num_clauses);  // c3 = index 2
        Node* B = create_internal_node(leaf_c1, leaf_c3, f.num_clauses);  // (c1, c3)

        Node* v = create_internal_node(A, B, f.num_clauses);  // noeud "v" de la Figure 2

        // Sous-arbre droit de la racine
        Node* leaf_x3 = create_leaf_node(NODE_LEAF_VAR, 3, f.num_clauses);
        Node* leaf_c4 = create_leaf_node(NODE_LEAF_CLAUSE, 3, f.num_clauses);  // c4 = index 3
        Node* C = create_internal_node(leaf_x3, leaf_c4, f.num_clauses);  // (x3, c4)

        Node* leaf_x5 = create_leaf_node(NODE_LEAF_VAR, 5, f.num_clauses);
        Node* leaf_c2 = create_leaf_node(NODE_LEAF_CLAUSE, 1, f.num_clauses);  // c2 = index 1
        Node* D = create_internal_node(leaf_x5, leaf_c2, f.num_clauses);  // (x5, c2)

        Node* E = create_internal_node(C, D, f.num_clauses);  // (x3, c4, x5, c2)
        
        Node* leaf_x4 = create_leaf_node(NODE_LEAF_VAR, 4, f.num_clauses);
        Node* F_node = create_internal_node(E, leaf_x4, f.num_clauses);  // (x3, c4, x5, c2, x4)

        root = create_internal_node(v, F_node, f.num_clauses);  // racine

        end_time = clock();
        time_used_tree = ((double) (end_time - start_time)) / CLOCKS_PER_SEC;
        printf("[Temps] Generation de l'arbre manuel : %f secondes\n\n", time_used_tree);
    } 
    else if (execution_mode == TREE_RANDOM) {
        printf("\n>>> EXECUTION : ARBRE ALEATOIRE <<<\n");
        
        start_time = clock();
        root = generate_random_tree(&f);
        end_time = clock();
        
        time_used_tree = ((double) (end_time - start_time)) / CLOCKS_PER_SEC;
        printf("[Temps] Generation de l'arbre aleatoire : %f secondes\n\n", time_used_tree);
    }
    else if (execution_mode == TREE_LINEAR) {
        printf("\n>>> EXECUTION : ARBRE LINEAIRE (Linear Branch Decomposition) <<<\n");

        start_time = clock();
        root = generate_linear_tree(&f);
        end_time = clock();

        time_used_tree = ((double) (end_time - start_time)) / CLOCKS_PER_SEC;
        printf("[Temps] Generation de l'arbre lineaire : %f secondes\n\n", time_used_tree);
    }

    // --- PROCEDURE 1 (BOTTOM-UP) : Calcul de PS'(Fv) ---
    printf(">>> EXECUTION : PROCEDURE 1 (Bottom-Up PS'(Fv)) <<<\n");
    start_time = clock();
    compute_ps_prime_bottom_up(root, &f, all_clauses_mask, trie);
    end_time = clock();
    time_used_proc1 = ((double) (end_time - start_time)) / CLOCKS_PER_SEC;
    printf("[Proc 1] Termine. PS-sets uniques dans le trie : %d\n", trie->num_ps_sets);
    printf("[Temps] Execution Procedure 1 : %f secondes\n\n", time_used_proc1);

    // --- PROCEDURES 2 (TOP-DOWN) : Calcul de PS'(Fv_barre) ---
    printf(">>> EXECUTION : PROCEDURES 2 (Top-Down PS'(Fv_barre) <<<\n");
    start_time = clock();
    compute_ps_bar_top_down(root, &f, trie);
    end_time = clock();
    time_used_proc2 = ((double) (end_time - start_time)) / CLOCKS_PER_SEC;
    printf("[Proc 2] Termine. PS-sets uniques dans le trie : %d\n", trie->num_ps_sets);
    printf("[Temps] Execution Procedure 2 : %f secondes\n\n", time_used_proc2);
    

    // --- RESUME ---
    int max_p_prime = calculate_tree_ps_width(root);
    int max_p_prime_barre = calculate_tree_ps_prime_barre_width(root);
    int ps_width = calculate_tree_max_ps_width(root);

    printf("\n=== RESUME DE L'EXECUTION ===\n");
    printf("Taille max de PS'(F_v) (Proc 1, bottom-up) : %d\n", max_p_prime);
    printf("Taille max de PS'(F_v_barre) (Proc 2, top-down): %d\n", max_p_prime_barre);
    printf("Borne superieure ps-width de la formule F : <= %d\n", ps_width);
    printf("Noeuds uniques generes dans le Trie (RAM) : %d\n", trie->num_ps_sets);
    printf("Temps total de resolution : %f secondes\n", 
           time_used_tree + time_used_proc1 + time_used_proc2);

    if (execution_mode == TREE_MANUAL) {
        print_node_ps_set(root, &f, "racine");
        // Afficher aussi le noeud "v" (enfant gauche de la racine)
        if (root->left) {
            print_node_ps_set(root->left, &f, "v (Figure 2)");
            print_node_ps_barre_set(root->left, &f, "v");
        }
    }

    // --- NETTOYAGE ---
    free_formula(f_ptr);
    free_trie(trie);
    free_tree(root);
    free_bitset(all_clauses_mask);

    return 0;
}
