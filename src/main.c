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
#include "algo/procedure3.h"

// ============================================================================
// AFFICHAGE
// ============================================================================

/**
 * Affiche un bitset sous forme binaire lisible.
 *
 * Les bits sont affichés de gauche à droite du plus grand indice de clause
 * au plus petit (c_{m-1} ... c_0), permettant de visualiser quelles clauses
 * sont présentes dans un ensemble de projection satisfiable (PS-set).
 *
 * @param b            Le bitset à afficher.
 * @param num_clauses  Nombre total de clauses (nombre de bits significatifs).
 */
void print_binary(Bitset* b, int num_clauses) {
    // Affiche les bits de gauche (clause de plus grand indice) à droite (clause 0)
    for (int i = num_clauses - 1; i >= 0; i--) {
        int word_idx = i / 64;
        int bit_idx = i % 64;
        uint64_t bit = (b->words[word_idx] >> bit_idx) & 1ULL;
        printf("%llu", bit);
    }
}

/**
 * Affiche le contenu de PS'(Fv) pour un noeud donné.
 *
 * Pour chaque élément de l'ensemble PS'(Fv), affiche son identifiant
 * unique (ps_id attribué par le trie) et sa représentation binaire
 * indiquant quelles clauses de cla(F) sont satisfaites.
 *
 * @param node       Le noeud dont on affiche PS'(Fv).
 * @param f          La formule SAT (pour connaître num_clauses).
 * @param node_name  Nom du noeud pour l'en-tête d'affichage.
 */
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

/**
 * Affiche le contenu de PS'(F_v̄) pour un noeud donné.
 *
 * Même principe que print_node_ps_set, mais pour l'ensemble
 * PS'(F_v̄) calculé par la Procédure 2 (top-down).
 *
 * @param node       Le noeud dont on affiche PS'(F_v̄).
 * @param f          La formule SAT.
 * @param node_name  Nom du noeud pour l'en-tête d'affichage.
 */
void print_node_ps_barre_set(Node* node, SAT_Formula* f, const char* node_name) {
    if (!node || !node->ps_prime_v_barre) return;
    
    PS_Set* result = node->ps_prime_v_barre;
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
#define TREE_GREEDY 3
#define TREE_BENCHMARK 4

// ============================================================================
// FONCTION DE RESOLUTION UNIQUE
// Resout une formule avec un mode d'arbre donne.
// Si compact=true, affiche une seule ligne de resume (mode benchmark).
// ============================================================================
static int solve_formula(const char* filename, int execution_mode, bool compact);

// ============================================================================
// BENCHMARK : execute le solveur sur une batterie d'instances
// ============================================================================
/**
 * Exécute le solveur sur une batterie prédéfinie d'instances de test.
 *
 * Lance solve_formula en mode compact (une ligne par instance) sur des
 * formules de types 1, 2 et 3 (Section 6.1 du papier), avec différents
 * modes de décomposition (greedy, linear). Affiche un tableau récapitulatif
 * avec variables, clauses, ps-width, MaxSAT et temps d'exécution.
 */
static void run_benchmark(void) {
    const char* dir = "script/instances_test/";

    typedef struct { const char* file; int mode; } BenchEntry;

    BenchEntry entries[] = {
        // Type 3 t=3 s=2 (greedy) : psw attendue = 8 
        {"type3_n30_t3_s2.cnf",     TREE_GREEDY},
        {"type3_n60_t3_s2.cnf",     TREE_GREEDY},
        {"type3_n100_t3_s2.cnf",    TREE_GREEDY},
        {"type3_n200_t3_s2.cnf",    TREE_GREEDY},
        {"type3_n1000_t3_s2.cnf",   TREE_GREEDY},
        {"type3_n5000_t3_s2.cnf",   TREE_GREEDY},
        {"type3_n10000_t3_s2.cnf",  TREE_GREEDY},

        // Type 3 t=5 s=3 (greedy) : psw attendue ~ 60
        {"type3_n60_t5_s3.cnf",     TREE_GREEDY},
        {"type3_n90_t5_s3.cnf",     TREE_GREEDY},
        {"type3_n120_t5_s3.cnf",    TREE_GREEDY},
        {"type3_n150_t5_s3.cnf",    TREE_GREEDY},
        {"type3_n1000_t5_s3.cnf",   TREE_GREEDY},

        // Type 2 t=3 (greedy) : psw attendue ~ 32 
        {"type2_v25_c100_t3.cnf",   TREE_GREEDY},
        {"type2_v50_c200_t3.cnf",   TREE_GREEDY},
        {"type2_v100_c400_t3.cnf",  TREE_GREEDY},
        {"type2_v200_c800_t3.cnf",  TREE_GREEDY},
        {"type2_v500_c2000_t3.cnf", TREE_GREEDY},
        {"type2_v1000_c4000_t3.cnf",TREE_GREEDY},

        // Type 1 (greedy) 
        {"type1_v50_c60.cnf",       TREE_GREEDY},
        {"type1_v80_c100.cnf",      TREE_GREEDY},
        {"type1_v150_c180.cnf",     TREE_GREEDY},

        // Comparaison linear vs greedy sur une meme formule
        {"type2_v50_c200_t3.cnf",   TREE_LINEAR},
        {"type3_n60_t3_s2.cnf",     TREE_LINEAR},
        {"type3_n100_t3_s2.cnf",    TREE_LINEAR},

        // Random k-SAT (greedy) 
        {"random_k3_v8_c20.cnf",    TREE_GREEDY},
        {"random_k3_v10_c50.cnf",   TREE_GREEDY},
        {"random_k3_v12_c50.cnf",   TREE_GREEDY},
    };

    int num_entries = sizeof(entries) / sizeof(entries[0]);

    printf("================================================================================\n");
    printf("  BENCHMARK COMPLET : %d instances\n", num_entries);
    printf("================================================================================\n");
    printf("%-40s %6s %7s %7s %10s %12s %12s %10s\n",
           "Fichier", "Mode", "Vars", "Cls", "ps-width", "MaxSAT", "#SAT", "Temps(s)");
    printf("--------------------------------------------------------------------------------\n");

    char path[512];
    for (int i = 0; i < num_entries; i++) {
        snprintf(path, sizeof(path), "%s%s", dir, entries[i].file);
        // Verifier que le fichier existe
        FILE* test = fopen(path, "r");
        if (!test) {
            printf("%-40s  [FICHIER MANQUANT]\n", entries[i].file);
            continue;
        }
        fclose(test);
        solve_formula(path, entries[i].mode, true);
    }

    printf("================================================================================\n");
}

int main(int argc, char *argv[]) {
    // Mode benchmark special
    if (argc == 2 && strcmp(argv[1], "benchmark") == 0) {
        run_benchmark();
        return 0;
    }

    // vérif du nombre d'arguments
    if (argc != 3) {
        printf("Erreur : Mauvais nombre d'arguments.\n\n");
        printf("Utilisation : %s <chemin_vers_fichier.cnf> <mode_arbre>\n", argv[0]);
        printf("       ou   : %s benchmark\n", argv[0]);
        printf("Modes disponibles :\n");
        printf("  manual : Arbre manuel (Figure 2, page 67 du papier)\n");
        printf("  random : Arbre aleatoire pur\n");
        printf("  linear : Linear Branch Decomposition (ordre par numero de variable)\n");
        printf("  greedy : GreedyOrder (heuristique du papier, Section 6, page 76)\n");
        printf("  benchmark : Execute toutes les instances de test\n\n");
        printf("Exemple : %s script/instances_test/type2_v50_c200_t3.cnf greedy\n", argv[0]);
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
    } else if (strcmp(mode_str, "greedy") == 0) {
        execution_mode = TREE_GREEDY;
    } else {
        printf("Erreur : Mode '%s' inconnu.\n", mode_str);
        printf("Veuillez choisir parmi : manual, random, linear, greedy, benchmark\n");
        return 1;
    }

    return solve_formula(filename, execution_mode, false);
}

/**
 * Résout une formule CNF avec un mode d'arbre de décomposition donné.
 *
 * Enchaîne les trois phases de l'algorithme du papier :
 *   1. Construction de l'arbre de décomposition (T, delta)
 *   2. Procédure 1 (bottom-up) : calcul de PS'(Fv) pour chaque noeud v
 *   3. Procédure 2 (top-down) : calcul de PS'(F_v_barre) pour chaque noeud v
 *   4. Procédure 3 : programmation dynamique pour résoudre #SAT et MaxSAT
 *      (Théorème 2 : O(k^3.m.(m+n)))
 *
 * @param filename        Chemin vers le fichier DIMACS CNF.
 * @param execution_mode  Mode de construction de l'arbre (TREE_MANUAL,
 *                        TREE_RANDOM, TREE_LINEAR, TREE_GREEDY).
 * @param compact         Si true, affiche une seule ligne résumé (mode benchmark).
 * @return                0 en cas de succès, 1 en cas d'erreur.
 */
static int solve_formula(const char* filename, int execution_mode, bool compact) {
    // lecture de la formule dynamique
    if (!compact) printf("Lecture de la formule depuis le fichier CNF : %s\n", filename);
    SAT_Formula* f_ptr = parse_cnf(filename);
    if (!f_ptr) {
        if (compact) printf("%-40s  [ERREUR PARSING]\n", filename);
        else printf("Erreur critique lors de l'ouverture ou du parsing du fichier.\n");
        return 1;
    }

    SAT_Formula f = *f_ptr;

    if (!compact) {
        printf("=== CARACTERISTIQUES DE LA FORMULE ===\n");
        printf(" Variables : %d\n", f.num_vars);
        printf(" Clauses   : %d\n", f.num_clauses);
        printf("======================================\n");
    }

    Bitset* all_clauses_mask = create_bitset(f.num_clauses);
    for (int i = 0; i < f.num_clauses; i++) {
        set_bit(all_clauses_mask, i);
    }

    // Si m est petit, on alloue au moins 100 000 pour etre large
    int initial_capacity = (f.num_clauses * f.num_clauses > 100000) ?
                       (f.num_clauses * f.num_clauses * 2) : 100000;
    BinaryTrie* trie = create_trie(initial_capacity);
    Node* root = NULL;

    // variables temps
    clock_t start_time, end_time;
    double time_used_tree = 0.0, time_used_proc1 = 0.0, time_used_proc2 = 0.0, time_used_proc3 = 0.0;

    // CONSTRUCTION DE L'ARBRE
    if (execution_mode == TREE_MANUAL) {
        if (!compact) printf("\n>>> EXECUTION : ARBRE MANUEL (Figure 2, page 67) <<<\n");

        if (f.num_vars != 5 || f.num_clauses != 4) {
            if (!compact) {
                printf("ERREUR : L'arbre manuel est concu pour la formule de la Figure 2 ");
                printf("(5 variables, 4 clauses). Formule actuelle : %d var, %d clauses.\n",
                       f.num_vars, f.num_clauses);
            }
            free_formula(f_ptr);
            free_trie(trie);
            free_bitset(all_clauses_mask);
            return 1;
        }

        start_time = clock();

        Node* leaf_x1 = create_leaf_node(NODE_LEAF_VAR, 1, f.num_clauses);
        Node* leaf_x2 = create_leaf_node(NODE_LEAF_VAR, 2, f.num_clauses);
        Node* A = create_internal_node(leaf_x1, leaf_x2, f.num_clauses);

        Node* leaf_c1 = create_leaf_node(NODE_LEAF_CLAUSE, 0, f.num_clauses);
        Node* leaf_c3 = create_leaf_node(NODE_LEAF_CLAUSE, 2, f.num_clauses);
        Node* B = create_internal_node(leaf_c1, leaf_c3, f.num_clauses);

        Node* v = create_internal_node(A, B, f.num_clauses);

        Node* leaf_x3 = create_leaf_node(NODE_LEAF_VAR, 3, f.num_clauses);
        Node* leaf_c4 = create_leaf_node(NODE_LEAF_CLAUSE, 3, f.num_clauses);
        Node* C = create_internal_node(leaf_x3, leaf_c4, f.num_clauses);

        Node* leaf_x5 = create_leaf_node(NODE_LEAF_VAR, 5, f.num_clauses);
        Node* leaf_c2 = create_leaf_node(NODE_LEAF_CLAUSE, 1, f.num_clauses);
        Node* D = create_internal_node(leaf_x5, leaf_c2, f.num_clauses);

        Node* E = create_internal_node(C, D, f.num_clauses);

        Node* leaf_x4 = create_leaf_node(NODE_LEAF_VAR, 4, f.num_clauses);
        Node* F_node = create_internal_node(E, leaf_x4, f.num_clauses);

        root = create_internal_node(v, F_node, f.num_clauses);

        end_time = clock();
        time_used_tree = ((double) (end_time - start_time)) / CLOCKS_PER_SEC;
        if (!compact) printf("[Temps] Generation de l'arbre manuel : %f secondes\n\n", time_used_tree);
    }
    else if (execution_mode == TREE_RANDOM) {
        if (!compact) printf("\n>>> EXECUTION : ARBRE ALEATOIRE <<<\n");

        start_time = clock();
        root = generate_random_tree(&f);
        end_time = clock();

        time_used_tree = ((double) (end_time - start_time)) / CLOCKS_PER_SEC;
        if (!compact) printf("[Temps] Generation de l'arbre aleatoire : %f secondes\n\n", time_used_tree);
    }
    else if (execution_mode == TREE_LINEAR) {
        if (!compact) printf("\n>>> EXECUTION : ARBRE LINEAIRE (Linear Branch Decomposition) <<<\n");

        start_time = clock();
        root = generate_linear_tree(&f);
        end_time = clock();

        time_used_tree = ((double) (end_time - start_time)) / CLOCKS_PER_SEC;
        if (!compact) printf("[Temps] Generation de l'arbre lineaire : %f secondes\n\n", time_used_tree);
    }
    else if (execution_mode == TREE_GREEDY) {
        if (!compact) printf("\n>>> EXECUTION : GREEDY ORDER (Heuristique du papier, Section 6) <<<\n");

        start_time = clock();
        root = generate_greedy_linear_tree(&f);
        end_time = clock();

        time_used_tree = ((double) (end_time - start_time)) / CLOCKS_PER_SEC;
        if (!compact) printf("[Temps] Generation de l'arbre GreedyOrder : %f secondes\n\n", time_used_tree);
    }

    // PROCEDURE 1 (BOTTOM-UP) : Calcul de PS'(Fv) 
    if (!compact) printf(">>> EXECUTION : PROCEDURE 1 (Bottom-Up PS'(Fv)) <<<\n");
    start_time = clock();
    compute_ps_prime_bottom_up(root, &f, all_clauses_mask, trie);
    end_time = clock();
    time_used_proc1 = ((double) (end_time - start_time)) / CLOCKS_PER_SEC;
    if (!compact) {
        printf("[Proc 1] Termine. PS-sets uniques dans le trie : %d\n", trie->num_ps_sets);
        printf("[Temps] Execution Procedure 1 : %f secondes\n\n", time_used_proc1);
    }

    // PROCEDURES 2 (TOP-DOWN) : Calcul de PS'(Fv_barre) 
    if (!compact) printf(">>> EXECUTION : PROCEDURES 2 (Top-Down PS'(Fv_barre) <<<\n");
    start_time = clock();
    compute_ps_bar_top_down(root, &f, trie);
    end_time = clock();
    time_used_proc2 = ((double) (end_time - start_time)) / CLOCKS_PER_SEC;
    if (!compact) {
        printf("[Proc 2] Termine. PS-sets uniques dans le trie : %d\n", trie->num_ps_sets);
        printf("[Temps] Execution Procedure 2 : %f secondes\n\n", time_used_proc2);
    }

    // PROCEDURE 3 (DP) : Resolution de #SAT et MaxSAT 
    if (!compact) printf(">>> EXECUTION : PROCEDURE 3 (Programmation Dynamique #SAT & MaxSAT) <<<\n");
    start_time = clock();
    DPResult dp_result = solve_dp(root, &f, all_clauses_mask, trie);
    end_time = clock();
    time_used_proc3 = ((double) (end_time - start_time)) / CLOCKS_PER_SEC;
    if (!compact) {
        printf("[Proc 3] Termine.\n");
        printf("[Temps] Execution Procedure 3 : %f secondes\n\n", time_used_proc3);
    }

    // RESUME
    int ps_width = calculate_tree_max_ps_width(root);
    double total_time = time_used_tree + time_used_proc1 + time_used_proc2 + time_used_proc3;

    if (compact) {
        // Mode benchmark : une seule ligne par instance
        const char* mode_str = (execution_mode == TREE_GREEDY) ? "greedy" :
                               (execution_mode == TREE_LINEAR) ? "linear" :
                               (execution_mode == TREE_RANDOM) ? "random" : "manual";
        // Extraire le nom de fichier sans le chemin
        const char* basename = strrchr(filename, '/');
        basename = basename ? basename + 1 : filename;
        printf("%-40s %6s %7d %7d %10d %12lld %12lld %10.3f\n",
               basename, mode_str, f.num_vars, f.num_clauses,
               ps_width, dp_result.maxsat_value, dp_result.sharpsat_count, total_time);
    } else {
        int max_p_prime = calculate_tree_ps_width(root);
        int max_p_prime_barre = calculate_tree_ps_prime_barre_width(root);

        printf("\n=== RESUME DE L'EXECUTION ===\n");
        printf("Taille max de PS'(F_v) (Proc 1, bottom-up) : %d\n", max_p_prime);
        printf("Taille max de PS'(F_v_barre) (Proc 2, top-down): %d\n", max_p_prime_barre);
        printf("Borne superieure ps-width de la formule F : <= %d\n", ps_width);
        printf("Noeuds uniques generes dans le Trie (RAM) : %d\n", trie->num_ps_sets);
        printf("\n--- Resultats Procedure 3 ---\n");
        printf("MaxSAT  : %lld clauses satisfaisables\n", dp_result.maxsat_value);
        printf("#SAT    : %lld affectations satisfaisant toutes les clauses\n", dp_result.sharpsat_count);
        printf("\nTemps total de resolution : %f secondes\n", total_time);

        if (execution_mode == TREE_MANUAL) {
            print_node_ps_set(root, &f, "racine");
            if (root->left) {
                print_node_ps_set(root->left, &f, "v");
                print_node_ps_barre_set(root->left, &f, "v");
            }
        }
    }

    // NETTOYAGE
    free_formula(f_ptr);
    free_trie(trie);
    free_tree(root);
    free_bitset(all_clauses_mask);

    return 0;
}