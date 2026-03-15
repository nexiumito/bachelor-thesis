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

// Affiche joliment le contenu du PS_Set d'un noeud donné
void print_node_ps_set(Node* node, SAT_Formula* f, const char* node_name) {
    if (!node || !node->ps_prime_v) return;
    
    PS_Set* result = node->ps_prime_v;
    printf("\n=== RESULTATS AU NOEUD %s ===\n", node_name);
    printf("Taille de PS'(F_v) trouvee : %d\n", result->size);
    printf("Masques retenus (Bits: c%d ... c1) :\n", f->num_clauses);
    
    for (int i = 0; i < result->size; i++) {
        printf("  - Ensemble ID %d : ", result->ps_ids[i]);
        print_binary(result->sets[i], f->num_clauses);
        printf("\n");
    }
}

// Parcourt l'arbre pour trouver et afficher le(s) noeud(s) qui cause(nt) la ps-width maximale
void print_max_ps_nodes(Node* node, SAT_Formula* f, int max_width) {
    if (!node) return;
    if (node->ps_prime_v && node->ps_prime_v->size == max_width) {
        printf("\n[!] Noeud interne creant la ps-width maximale de %d trouve :\n", max_width);
        print_node_ps_set(node, f, "MAX_WIDTH_NODE");
    }
    print_max_ps_nodes(node->left, f, max_width);
    print_max_ps_nodes(node->right, f, max_width);
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

    // Si m est petit, on alloue au moins 100 000 pour être tranquille
    // Si m est grand, on alloue O(m^2) pour les bons arbres
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
        
        start_time = clock();
        
        Node* leaf_x1 = create_leaf_node(NODE_LEAF_VAR, 1, f.num_clauses);
        Node* leaf_x2 = create_leaf_node(NODE_LEAF_VAR, 2, f.num_clauses);
        Node* u1 = create_internal_node(leaf_x1, leaf_x2, f.num_clauses);

        Node* leaf_c1 = create_leaf_node(NODE_LEAF_CLAUSE, 0, f.num_clauses);
        Node* leaf_c3 = create_leaf_node(NODE_LEAF_CLAUSE, 2, f.num_clauses);
        Node* u2 = create_internal_node(leaf_c1, leaf_c3, f.num_clauses);

        root = create_internal_node(u1, u2, f.num_clauses); 
        end_time = clock();

        time_used_tree = ((double) (end_time - start_time)) / CLOCKS_PER_SEC;
        printf("[Temps] Generation de l'arbre manuel : %f secondes\n", time_used_tree);
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

    // --- PROCEDURE 1 (BOTTOM-UP) ---
    printf(">>> EXECUTION : PROCEDURE 1 (Bottom-Up) <<<\n");
    start_time = clock();
    compute_ps_prime_bottom_up(root, &f, all_clauses_mask, trie);
    end_time = clock();
    time_used_proc1 = ((double) (end_time - start_time)) / CLOCKS_PER_SEC;
    printf("[Temps] Execution Procedure 1 : %f secondes\n\n", time_used_proc1);

    // --- PROCEDURE 2 (TOP-DOWN) ---
    printf(">>> EXECUTION : PROCEDURE 2 (Top-Down) <<<\n");
    start_time = clock();
    
    // Le print SAT/UNSAT se fait dans cette fonction (sans saut de ligne au début)
    compute_ps_double_prime_top_down(root, &f, trie);
    
    end_time = clock();
    time_used_proc2 = ((double) (end_time - start_time)) / CLOCKS_PER_SEC;
    printf("[Temps] Execution Procedure 2 : %f secondes\n", time_used_proc2);

    // --- RESUME ---
    int max_p_prime = calculate_tree_ps_width(root);
    int max_p_double_prime = calculate_tree_ps_double_prime_width(root);

    printf("\n=== RESUME DE L'EXECUTION ===\n");
    printf("Taille max de P'(v)  (Etats generes par Proc 1) : %d\n", max_p_prime);
    printf("Taille max de P''(v) (Etats utiles apres Proc 2): %d\n", max_p_double_prime);
    printf("Borne superieure ps-width de la formule F       : <= %d\n", max_p_prime);
    printf("Noeuds uniques generes dans le Trie (RAM)       : %d\n", trie->num_ps_sets);
    printf("Temps total de resolution                       : %f secondes\n", 
           time_used_tree + time_used_proc1 + time_used_proc2);

    if (execution_mode == TREE_MANUAL) {
        print_node_ps_set(root, &f, "v (Figure 2)");
    }

    // --- NETTOYAGE ---
    free_formula(f_ptr);
    free_trie(trie);
    free_tree(root);
    free_bitset(all_clauses_mask);

    return 0;
}