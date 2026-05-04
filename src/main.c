#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <inttypes.h>

#include "core/formula.h"
#include "core/parser.h"
#include "core/decomposition_tree.h"
#include "utils/bitset.h"
#include "utils/dnnf.h"
#include "utils/ps_set.h"
#include "utils/trie.h"
#include "utils/query/count.h"
#include "utils/query/consistency.h"
#include "utils/query/validity.h"
#include "utils/query/find_model.h"
#include "utils/query/entails.h"
#include "utils/query/is_implicant.h"
#include "utils/query/enumerate.h"
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
        printf("%lu", bit);
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

// Contexte + callback pour enumerate ASCII (file scope pour rester C standard,
// pas de fonction nested GNU).
typedef struct {
    long long count;
    long long limit;
} enum_ascii_ctx;

static int enum_ascii_print_cb(const int* m, int n, void* u) {
    enum_ascii_ctx* ctx = (enum_ascii_ctx*)u;
    if (ctx->count >= ctx->limit) return 1;
    for (int v = 1; v <= n; v++) putchar(m[v] ? '1' : '0');
    putchar('\n');
    ctx->count++;
    return 0;
}

// Callback silencieux pour le bench enum_all : compte sans imprimer.
static int enum_silent_cb(const int* m, int n, void* u) {
    (void)m; (void)n;
    long long* counter = (long long*)u;
    (*counter)++;
    return 0;
}

// Callback silencieux qui stoppe immediatement au 1er modele (bench enum_first).
static int enum_silent_first_cb(const int* m, int n, void* u) {
    (void)m; (void)n; (void)u;
    return 1;  // stop apres le 1er modele.
}

// Description d'une requete optionnelle a executer apres la construction
// du DAG d-DNNF. Si name == NULL, aucune requete n'est lancee.
//
// Pour les requetes parametrees :
//   - entails L1 L2 ... Lk    : args = literaux DIMACS de la clause
//   - is_implicant L1 L2 ... Lk : args = literaux DIMACS du terme gamma
//   - enumerate [LIMIT]       : args = limite optionnelle (defaut 1e6)
typedef struct {
    const char* name;       // consistency, validity, find_model, entails, is_implicant, enumerate
    int          argc;       // nb d'arguments suivant le nom de requete (cf. main argv)
    char**       argv;       // arguments bruts (non-libere par solve_formula)
} QuerySpec;

// ============================================================================
// FONCTION DE RESOLUTION UNIQUE
// Resout une formule avec un mode d'arbre donne.
// Si compact=true, affiche une seule ligne de resume (mode benchmark).
// query peut etre NULL : aucune requete supplementaire dans ce cas.
// ============================================================================
static int solve_formula(const char* filename, int execution_mode,
                         bool compact, const QuerySpec* query);

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
    // Racine du repertoire de donnees (relative au cwd src/).
    const char* data_root = "../data/";

    typedef struct { const char* subdir; const char* file; int mode; } BenchEntry;

    BenchEntry entries[] = {
        // Type 3 t=3 s=2 (greedy) : psw attendue = 8
        {"type3/", "type3_n30_t3_s2.cnf",     TREE_GREEDY},
        {"type3/", "type3_n60_t3_s2.cnf",     TREE_GREEDY},
        {"type3/", "type3_n100_t3_s2.cnf",    TREE_GREEDY},
        {"type3/", "type3_n200_t3_s2.cnf",    TREE_GREEDY},
        {"type3/", "type3_n1000_t3_s2.cnf",   TREE_GREEDY},
        {"type3/", "type3_n5000_t3_s2.cnf",   TREE_GREEDY},
        {"type3/", "type3_n10000_t3_s2.cnf",  TREE_GREEDY},

        // Type 3 t=5 s=3 (greedy) : psw attendue ~ 60
        {"type3/", "type3_n60_t5_s3.cnf",     TREE_GREEDY},
        {"type3/", "type3_n90_t5_s3.cnf",     TREE_GREEDY},
        {"type3/", "type3_n120_t5_s3.cnf",    TREE_GREEDY},
        {"type3/", "type3_n150_t5_s3.cnf",    TREE_GREEDY},
        {"type3/", "type3_n1000_t5_s3.cnf",   TREE_GREEDY},

        // Type 2 t=3 (greedy) : psw attendue ~ 32
        {"type2/", "type2_v25_c100_t3.cnf",   TREE_GREEDY},
        {"type2/", "type2_v50_c200_t3.cnf",   TREE_GREEDY},
        {"type2/", "type2_v100_c400_t3.cnf",  TREE_GREEDY},
        {"type2/", "type2_v200_c800_t3.cnf",  TREE_GREEDY},
        {"type2/", "type2_v500_c2000_t3.cnf", TREE_GREEDY},
        {"type2/", "type2_v1000_c4000_t3.cnf",TREE_GREEDY},

        // Type 1 (greedy)
        {"type1/", "type1_v50_c60.cnf",       TREE_GREEDY},
        {"type1/", "type1_v80_c100.cnf",      TREE_GREEDY},
        {"type1/", "type1_v150_c180.cnf",     TREE_GREEDY},

        // Comparaison linear vs greedy sur une meme formule
        {"type2/", "type2_v50_c200_t3.cnf",   TREE_LINEAR},
        {"type3/", "type3_n60_t3_s2.cnf",     TREE_LINEAR},
        {"type3/", "type3_n100_t3_s2.cnf",    TREE_LINEAR},

        // Random k-SAT (greedy)
        {"random/", "random_k3_v8_c20.cnf",    TREE_GREEDY},
        {"random/", "random_k3_v10_c50.cnf",   TREE_GREEDY},
        {"random/", "random_k3_v12_c50.cnf",   TREE_GREEDY},

        // Formules difficiles : 3-SAT aleatoire sans structure d'interval bigraph
        // -> ps-width grande, DP inefficace : baseline de reference
        // Ratio critique ~ 4.26 (transition de phase)
        {"random/", "random_k3_v15_c64_difficile.cnf",   TREE_GREEDY},
        {"random/", "random_k3_v20_c85_difficile.cnf",   TREE_GREEDY},
        {"random/", "random_k3_v30_c128_difficile.cnf",  TREE_GREEDY},
        {"random/", "random_k3_v50_c213_difficile.cnf",  TREE_GREEDY},
        {"random/", "random_k3_v100_c426_difficile.cnf", TREE_GREEDY},
        // Ratio faible ~ 2.0 (SAT quasi-certain, pas de structure)
        {"random/", "random_k3_v30_c60_difficile.cnf",   TREE_GREEDY},
        {"random/", "random_k3_v50_c100_difficile.cnf",  TREE_GREEDY},
        // Ratio eleve ~ 8.0 (UNSAT quasi-certain, MaxSAT interessant)
        {"random/", "random_k3_v30_c240_difficile.cnf",  TREE_GREEDY},
        {"random/", "random_k3_v50_c400_difficile.cnf",  TREE_GREEDY},
    };

    int num_entries = sizeof(entries) / sizeof(entries[0]);

    printf("================================================================================\n");
    printf("  BENCHMARK COMPLET : %d instances\n", num_entries);
    printf("================================================================================\n");
    printf("%-40s %6s %7s %7s %10s %12s %12s %12s %10s\n",
           "Fichier", "Mode", "Vars", "Cls", "ps-width", "MaxSAT", "#SAT", "DAG", "Temps(s)");
    printf("--------------------------------------------------------------------------------\n");

    char path[512];
    for (int i = 0; i < num_entries; i++) {
        snprintf(path, sizeof(path), "%s%s%s",
                 data_root, entries[i].subdir, entries[i].file);
        // Verifier que le fichier existe
        FILE* test = fopen(path, "r");
        if (!test) {
            printf("%-40s  [FICHIER MANQUANT]\n", entries[i].file);
            continue;
        }
        fclose(test);
        solve_formula(path, entries[i].mode, true, NULL);
    }

    printf("================================================================================\n");
}

// ============================================================================
// SORTIE JSON (mode --json)
// Sérialisation manuelle, pas de dépendance externe.
// Une seule ligne sur stdout, terminée par \n, puis exit.
// ============================================================================

static int solve_formula_json_impl(const char* filename, int execution_mode,
                                   int with_queries);
static int solve_formula_json(const char* filename, int execution_mode) {
    return solve_formula_json_impl(filename, execution_mode, 0);
}
static int solve_formula_json_with_queries(const char* filename, int execution_mode) {
    return solve_formula_json_impl(filename, execution_mode, 1);
}

// Tri partiel de 5 doubles puis mediane (= troisieme element). Utilise par
// le bench query : on jette la 1ere mesure (warm-up) et on prend la mediane
// des 4 suivantes ; pour simplifier, on stocke 5 mesures et on lit le
// 3eme tri (apres avoir mis la 1ere a +inf pour qu'elle ne compte pas).
static double median_of_5_drop_first(double samples[5]) {
    double s[5];
    for (int i = 0; i < 5; i++) s[i] = samples[i];
    s[0] = 1e308;  // exclut la 1ere mesure (warm-up cold) du tri.
    for (int i = 1; i < 5; i++) {
        for (int j = i; j > 0 && s[j] < s[j-1]; j--) {
            double t = s[j]; s[j] = s[j-1]; s[j-1] = t;
        }
    }
    // Apres tri (avec 1e308 en s[4]) : mediane des 4 mesures restantes
    // = moyenne de s[1] et s[2] (les deux plus petites apres la 1ere).
    return (s[1] + s[2]) / 2.0;
}

// Extrait les literaux DIMACS de la 1ere clause de F (clause d'index 0).
// Ecrit dans `out_lits` (pre-alloue, taille >= num_vars) et `*out_num`.
// Si la 1ere clause est vide (clause vide DIMACS, indique F UNSAT), retourne 0.
static int extract_first_clause_literals(SAT_Formula* f, int* out_lits, int* out_num) {
    int n = 0;
    for (int v = 1; v <= f->num_vars; v++) {
        // Bit 0 de mask_pos[v] = "v apparait positivement dans clause 0".
        if ((f->mask_pos[v]->words[0] >> 0) & 1ULL) out_lits[n++] = v;
        if ((f->mask_neg[v]->words[0] >> 0) & 1ULL) out_lits[n++] = -v;
    }
    *out_num = n;
    return n;
}

/**
 * Écrit une ligne JSON minimale en cas d'erreur (avant ou après parsing).
 * Échappe les caractères spéciaux les plus courants dans le chemin et le
 * message (guillemets, backslashes) pour éviter de casser le parser Python.
 */
static void print_json_escaped(const char* s) {
    // Échappement minimaliste : \" et \\ uniquement. Suffisant pour des chemins
    // de fichiers standards et des messages d'erreur ASCII.
    if (!s) return;
    for (const char* p = s; *p; p++) {
        if (*p == '"' || *p == '\\') {
            putchar('\\');
            putchar(*p);
        } else if ((unsigned char)*p < 0x20) {
            // Caractère de contrôle : on le saute pour rester sur une seule ligne.
            continue;
        } else {
            putchar(*p);
        }
    }
}

static void print_json_error(const char* filename, const char* mode_str,
                             const char* status, const char* msg) {
    printf("{\"schema_version\":1,\"status\":\"%s\",\"error_message\":\"",
           status);
    print_json_escaped(msg ? msg : "");
    printf("\",\"file\":\"");
    print_json_escaped(filename ? filename : "");
    printf("\",\"mode\":\"");
    print_json_escaped(mode_str ? mode_str : "");
    printf("\"}\n");
    fflush(stdout);
}

/**
 * Helper local : durée écoulée en millisecondes entre deux timespec
 * obtenus via clock_gettime(CLOCK_MONOTONIC). Wall-time monotone.
 */
static inline double elapsed_ms_ts(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000.0
         + (end.tv_nsec - start.tv_nsec) / 1e6;
}

// Decode un mode d'arbre (manual / random / linear / greedy) ; retourne -1
// si inconnu.
static int decode_mode(const char* mode_str) {
    if      (strcmp(mode_str, "manual") == 0) return TREE_MANUAL;
    else if (strcmp(mode_str, "random") == 0) return TREE_RANDOM;
    else if (strcmp(mode_str, "linear") == 0) return TREE_LINEAR;
    else if (strcmp(mode_str, "greedy") == 0) return TREE_GREEDY;
    return -1;
}

int main(int argc, char *argv[]) {
    // Mode benchmark special
    if (argc == 2 && strcmp(argv[1], "benchmark") == 0) {
        run_benchmark();
        return 0;
    }

    // Mode --json : argc == 4 et argv[3] == "--json".
    // Sortie : une seule ligne JSON sur stdout, exit 0 ou 1.
    if (argc == 4 && strcmp(argv[3], "--json") == 0) {
        int mode = decode_mode(argv[2]);
        if (mode < 0) {
            print_json_error(argv[1], argv[2], "error", "mode inconnu");
            return 1;
        }
        return solve_formula_json(argv[1], mode);
    }

    // Mode --json-with-queries : argc == 4 et argv[3] == "--json-with-queries".
    // Compile le DAG puis chronometre toutes les requetes en interne.
    // Champs query_*_ms additionnels dans le JSON (timings des requetes).
    if (argc == 4 && strcmp(argv[3], "--json-with-queries") == 0) {
        int mode = decode_mode(argv[2]);
        if (mode < 0) {
            print_json_error(argv[1], argv[2], "error", "mode inconnu");
            return 1;
        }
        return solve_formula_json_with_queries(argv[1], mode);
    }

    if (argc < 3) {
        printf("Erreur : Mauvais nombre d'arguments.\n\n");
        printf("Utilisation : %s <fichier.cnf> <mode> [requete [args...]]\n", argv[0]);
        printf("       ou   : %s benchmark\n\n", argv[0]);
        printf("Modes disponibles :\n");
        printf("  manual : Arbre manuel (Figure 2, page 67 du papier)\n");
        printf("  random : Arbre aleatoire pur\n");
        printf("  linear : Linear Branch Decomposition (ordre par numero de variable)\n");
        printf("  greedy : GreedyOrder (heuristique du papier, Section 6, page 76)\n");
        printf("  benchmark : Execute toutes les instances de test\n\n");
        printf("Requetes optionnelles (sur le DAG d-DNNF compile) :\n");
        printf("  consistency       : F est-elle satisfaisable ? (CO)\n");
        printf("  validity          : F est-elle une tautologie ? (VA)\n");
        printf("  find_model        : Donne une affectation satisfaisante (ou UNSAT) (ME, 1 modele)\n");
        printf("  entails L1 L2...  : F entraine-t-elle (L1 v L2 v ...) ? (CE, litteraux DIMACS)\n");
        printf("  is_implicant L1...: terme (L1 ^ L2 ^ ...) est-il implicant de F ? (IM)\n");
        printf("  enumerate [LIMIT] : Enumere LIMIT modeles (defaut: 1000000) (ME multi)\n\n");
        printf("Flags machine-readable :\n");
        printf("  --json              : sortie JSON minimale (compatible run 1..3)\n");
        printf("  --json-with-queries : sortie JSON enrichie avec timings query_*_ms (run 4)\n\n");
        printf("Exemples :\n");
        printf("  %s data/exemple1.cnf greedy\n", argv[0]);
        printf("  %s data/exemple1.cnf greedy consistency\n", argv[0]);
        printf("  %s data/exemple1.cnf greedy entails 1 2\n", argv[0]);
        printf("  %s data/exemple1.cnf greedy is_implicant 1\n", argv[0]);
        printf("  %s data/exemple1.cnf greedy enumerate 10\n", argv[0]);
        printf("  %s data/exemple1.cnf greedy --json-with-queries | jq .\n", argv[0]);
        return 1;
    }

    // recup des arguments
    char* filename = argv[1];
    char* mode_str = argv[2];
    int execution_mode = decode_mode(mode_str);
    if (execution_mode < 0) {
        printf("Erreur : Mode '%s' inconnu.\n", mode_str);
        printf("Veuillez choisir parmi : manual, random, linear, greedy, benchmark\n");
        return 1;
    }

    // Decodage de la requete optionnelle (argv[3]) + arguments suivants.
    // Convention : query.argv[0..argc-1] sont les arguments suivant le nom
    // de la requete (literaux DIMACS pour entails/is_implicant, limite
    // optionnelle pour enumerate).
    QuerySpec query = { .name = NULL, .argc = 0, .argv = NULL };
    if (argc >= 4) {
        query.name = argv[3];
        query.argc = argc - 4;
        query.argv = (argc > 4) ? &argv[4] : NULL;

        int known = (strcmp(query.name, "consistency") == 0)
                 || (strcmp(query.name, "validity") == 0)
                 || (strcmp(query.name, "find_model") == 0)
                 || (strcmp(query.name, "entails") == 0)
                 || (strcmp(query.name, "is_implicant") == 0)
                 || (strcmp(query.name, "enumerate") == 0);
        if (!known) {
            printf("Erreur : Requete '%s' inconnue.\n", query.name);
            printf("Choix : consistency, validity, find_model, entails, is_implicant, enumerate.\n");
            return 1;
        }
        // entails et is_implicant exigent au moins 1 litteral.
        if ((strcmp(query.name, "entails") == 0
             || strcmp(query.name, "is_implicant") == 0)
            && query.argc < 1) {
            printf("Erreur : '%s' exige au moins 1 litteral DIMACS en argument.\n",
                   query.name);
            return 1;
        }
    }

    return solve_formula(filename, execution_mode, false, &query);
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
 * Si query != NULL, exécute en plus une requête sur le DAG d-DNNF compilé.
 *
 * @param filename        Chemin vers le fichier DIMACS CNF.
 * @param execution_mode  Mode de construction de l'arbre (TREE_MANUAL,
 *                        TREE_RANDOM, TREE_LINEAR, TREE_GREEDY).
 * @param compact         Si true, affiche une seule ligne résumé (mode benchmark).
 * @param query           Requête optionnelle ou NULL pour aucune.
 * @return                0 en cas de succès, 1 en cas d'erreur.
 */
static int solve_formula(const char* filename, int execution_mode,
                         bool compact, const QuerySpec* query) {
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

    // PROCEDURE 3 (DP) : Resolution de #SAT et MaxSAT + construction du DAG d-DNNF
    if (!compact) printf(">>> EXECUTION : PROCEDURE 3 (Programmation Dynamique #SAT & MaxSAT) <<<\n");
    // Pool proprietaire du DAG, alloue juste avant l'appel (Phase 1 KC).
    DNNFPool* dnnf_pool = create_dnnf_pool(1024);
    start_time = clock();
    DPResult dp_result = solve_dp(root, &f, all_clauses_mask, trie, dnnf_pool);
    end_time = clock();
    time_used_proc3 = ((double) (end_time - start_time)) / CLOCKS_PER_SEC;
    if (!compact) {
        printf("[Proc 3] Termine.\n");
        printf("[Temps] Execution Procedure 3 : %f secondes\n\n", time_used_proc3);
    }

    // Verification #SAT == dnnf_count(root) (Phase 1, critere central).
    // Le dernier parametre capture l'overflow eventuel du recompte.
    int dnnf_cnt_overflow_local = 0;
    long long dnnf_cnt = dnnf_count(dp_result.dnnf_root, dnnf_pool,
                                    &dnnf_cnt_overflow_local);

    // Export NNF optionnel via la variable d'environnement DNNF_DUMP.
    const char* dnnf_dump = getenv("DNNF_DUMP");
    if (dnnf_dump && dnnf_dump[0]) {
        FILE* fp = fopen(dnnf_dump, "w");
        if (fp) {
            dnnf_export_nnf(dp_result.dnnf_root, f.num_vars, dnnf_pool, fp);
            fclose(fp);
            if (!compact) printf("[DNNF] Export ecrit dans %s\n\n", dnnf_dump);
        } else if (!compact) {
            fprintf(stderr, "[DNNF] Impossible d'ouvrir %s en ecriture.\n", dnnf_dump);
        }
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
        printf("%-40s %6s %7d %7d %10d %12lld %12lld %12lld %10.3f\n",
               basename, mode_str, f.num_vars, f.num_clauses,
               ps_width, dp_result.maxsat_value, dp_result.sharpsat_count,
               dp_result.dnnf_num_edges, total_time);
        if (dnnf_cnt != dp_result.sharpsat_count) {
            fprintf(stderr, "[MISMATCH] %s : dnnf_count=%lld sharpsat=%lld\n",
                    basename, dnnf_cnt, dp_result.sharpsat_count);
        }
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
        printf("Taille du d-DNNF : %d noeuds, %lld aretes.\n",
               dnnf_pool->num_nodes, dp_result.dnnf_num_edges);
        printf("#SAT (via dnnf_count) : %lld %s\n",
               dnnf_cnt,
               (dnnf_cnt == dp_result.sharpsat_count) ? "[OK]" : "[MISMATCH]");
        printf("\nTemps total de resolution : %f secondes\n", total_time);

        if (execution_mode == TREE_MANUAL) {
            print_node_ps_set(root, &f, "racine");
            if (root->left) {
                print_node_ps_set(root->left, &f, "v");
                print_node_ps_barre_set(root->left, &f, "v");
            }
        }

        // REQUETES OPTIONNELLES sur le DAG d-DNNF compile
        if (query && query->name) {
            if (strcmp(query->name, "consistency") == 0) {
                printf("\n=== Requete : consistency (CO) ===\n");
                int co = dnnf_consistency(dp_result.dnnf_root, dnnf_pool);
                long long c = dnnf_count(dp_result.dnnf_root, dnnf_pool, NULL);
                printf("F est %s (#SAT = %lld).\n",
                       co ? "SAT (consistante)" : "UNSAT (insatisfaisable)",
                       c);
            }
            else if (strcmp(query->name, "validity") == 0) {
                printf("\n=== Requete : validity (VA) ===\n");
                long long count_v = 0, max_models = 0;
                int va = dnnf_validity(dp_result.dnnf_root, dnnf_pool,
                                       f.num_vars, &count_v, &max_models);
                switch (va) {
                    case DNNF_VALIDITY_UNSAT:
                        printf("F est NON-VALIDE (UNSAT).\n");
                        break;
                    case DNNF_VALIDITY_OVERFLOW:
                        printf("Trop de variables (n=%d) pour evaluer 2^n en entier 64 bits.\n",
                               f.num_vars);
                        break;
                    default:
                        printf("F est %s (#SAT = %lld, 2^%d = %lld).\n",
                               (va == DNNF_VALIDITY_VALID) ? "VALIDE (tautologie)" : "NON-VALIDE",
                               count_v, f.num_vars, max_models);
                        break;
                }
            }
            else if (strcmp(query->name, "find_model") == 0) {
                printf("\n=== Requete : find_model (ME, 1 modele) ===\n");
                if (!dp_result.dnnf_root || dp_result.sharpsat_count == 0) {
                    printf("Formule insatisfiable, aucun modele.\n");
                } else {
                    int* model = calloc(f.num_vars + 1, sizeof(int));
                    int rc = dnnf_find_model(dp_result.dnnf_root, dnnf_pool,
                                             f.num_vars, model);
                    if (rc == 1) {
                        printf("Affectation (x1..x%d) : ", f.num_vars);
                        for (int v = 1; v <= f.num_vars; v++) {
                            putchar(model[v] ? '1' : '0');
                        }
                        putchar('\n');
                    } else {
                        printf("Erreur interne lors de la recherche d'un modele.\n");
                    }
                    free(model);
                }
            }
            else if (strcmp(query->name, "entails") == 0) {
                printf("\n=== Requete : entails (CE) ===\n");
                int* lits = malloc(query->argc * sizeof(int));
                for (int i = 0; i < query->argc; i++) lits[i] = atoi(query->argv[i]);
                long long c = 0;
                int rc = dnnf_entails(dp_result.dnnf_root, dnnf_pool,
                                      lits, query->argc, f.num_vars, &c);
                printf("Clause testee : (");
                for (int i = 0; i < query->argc; i++) {
                    if (i > 0) printf(" v ");
                    if (lits[i] > 0) printf("x%d", lits[i]);
                    else             printf("~x%d", -lits[i]);
                }
                printf(")\n");
                switch (rc) {
                    case DNNF_ENTAILS_YES:
                        printf("F |= clause (YES, 0 contre-exemple).\n");
                        break;
                    case DNNF_ENTAILS_NO:
                        printf("F NE entraine PAS la clause (NO, %lld contre-exemples).\n", c);
                        break;
                    case DNNF_ENTAILS_VACUOUSLY:
                        printf("F est UNSAT, donc entraine vacuously toute clause.\n");
                        break;
                    case DNNF_ENTAILS_BAD_VAR:
                        printf("Erreur : un litteral mentionne une variable hors borne 1..%d.\n",
                               f.num_vars);
                        break;
                    case DNNF_ENTAILS_OVERFLOW:
                        printf("Verdict ambigu : dnnf_count a sature avec c > 0.\n");
                        break;
                }
                free(lits);
            }
            else if (strcmp(query->name, "is_implicant") == 0) {
                printf("\n=== Requete : is_implicant (IM) ===\n");
                int* lits = malloc(query->argc * sizeof(int));
                for (int i = 0; i < query->argc; i++) lits[i] = atoi(query->argv[i]);
                long long c = 0, target = 0;
                int k_distinct = 0;
                int rc = dnnf_is_implicant(dp_result.dnnf_root, dnnf_pool,
                                           f.num_vars, lits, query->argc,
                                           &c, &target, &k_distinct);
                printf("Terme teste : (");
                for (int i = 0; i < query->argc; i++) {
                    if (i > 0) printf(" ^ ");
                    if (lits[i] > 0) printf("x%d", lits[i]);
                    else             printf("~x%d", -lits[i]);
                }
                printf(") -- k=%d variables distinctes\n", k_distinct);
                switch (rc) {
                    case DNNF_IS_IMPLICANT_YES:
                        printf("gamma |= F (YES, #SAT(F | gamma) = %lld = 2^(%d-%d)).\n",
                               c, f.num_vars, k_distinct);
                        break;
                    case DNNF_IS_IMPLICANT_NO:
                        printf("gamma NE est PAS implicant (NO, #SAT(F | gamma) = %lld != %lld).\n",
                               c, target);
                        break;
                    case DNNF_IS_IMPLICANT_UNSAT:
                        printf("F est UNSAT, aucun terme n'est implicant non-trivialement.\n");
                        break;
                    case DNNF_IS_IMPLICANT_BAD_VAR:
                        printf("Erreur : un litteral mentionne une variable hors borne 1..%d.\n",
                               f.num_vars);
                        break;
                    case DNNF_IS_IMPLICANT_OVERFLOW:
                        printf("Overflow : 2^(n-k) ne tient pas, ou dnnf_count a sature.\n");
                        break;
                }
                free(lits);
            }
            else if (strcmp(query->name, "enumerate") == 0) {
                printf("\n=== Requete : enumerate (ME multi) ===\n");
                long long limit = (query->argc >= 1) ? atoll(query->argv[0]) : 1000000LL;
                if (!dp_result.dnnf_root) {
                    printf("Formule insatisfiable, 0 modele.\n");
                } else {
                    enum_ascii_ctx ctx = { .count = 0, .limit = limit };
                    long long total = dnnf_enumerate(dp_result.dnnf_root, dnnf_pool,
                                                     f.num_vars,
                                                     enum_ascii_print_cb, &ctx);
                    printf("Total enumerated: %lld (limit=%lld)\n", total, limit);
                }
            }
        }
    }

    // NETTOYAGE
    // Le pool DNNF doit etre libere apres tout acces a dp_result.dnnf_root ;
    // l'ordre entre pool / trie / tree / formula est sinon indifferent
    // (ressources independantes). On met le pool en tete par propriete.
    free_dnnf_pool(dnnf_pool);
    free_formula(f_ptr);
    free_trie(trie);
    free_tree(root);
    free_bitset(all_clauses_mask);

    return 0;
}

/**
 * Variante "JSON" de solve_formula : aucune sortie ASCII intermédiaire,
 * mesures par clock_gettime(CLOCK_MONOTONIC), une seule ligne JSON finale.
 *
 * Les long long sont sérialisés en nombres décimaux ; si > 1e18, on bascule
 * en string "overflow" avec un drapeau dédié pour rester dans la précision
 * JSON (mantisse 53 bits).
 *
 * @param filename       Chemin DIMACS (passé tel quel dans le JSON).
 * @param execution_mode TREE_MANUAL | TREE_RANDOM | TREE_LINEAR | TREE_GREEDY.
 * @return 0 si "ok", 1 sinon.
 */
static int solve_formula_json_impl(const char* filename, int execution_mode,
                                   int with_queries) {
    const char* mode_str = (execution_mode == TREE_GREEDY) ? "greedy" :
                           (execution_mode == TREE_LINEAR) ? "linear" :
                           (execution_mode == TREE_RANDOM) ? "random" : "manual";

    // Seed pour le mode random, lue dans l'environnement.
    int seed = 0;
    const char* seed_env = getenv("RANDOM_SEED");
    if (seed_env && seed_env[0]) {
        seed = atoi(seed_env);
    }

    struct timespec t0, t1;

    // PARSING DIMACS (timer dedie).
    double time_parsing_ms = 0.0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    SAT_Formula* f_ptr = parse_cnf(filename);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    time_parsing_ms = elapsed_ms_ts(t0, t1);

    if (!f_ptr) {
        print_json_error(filename, mode_str, "parse_error",
                         "fichier introuvable ou DIMACS invalide");
        return 1;
    }

    SAT_Formula f = *f_ptr;

    // Garde-fou pour le mode manual.
    if (execution_mode == TREE_MANUAL && (f.num_vars != 5 || f.num_clauses != 4)) {
        free_formula(f_ptr);
        print_json_error(filename, mode_str, "error",
                         "manual mode requires exactement 5 vars et 4 clauses");
        return 1;
    }

    // Initialisation des structures partagees (RAII manuel).
    Bitset* all_clauses_mask = create_bitset(f.num_clauses);
    if (!all_clauses_mask) {
        free_formula(f_ptr);
        print_json_error(filename, mode_str, "alloc_fail",
                         "create_bitset(all_clauses_mask)");
        return 1;
    }
    for (int i = 0; i < f.num_clauses; i++) {
        set_bit(all_clauses_mask, i);
    }

    int initial_capacity = (f.num_clauses * f.num_clauses > 100000) ?
                           (f.num_clauses * f.num_clauses * 2) : 100000;
    BinaryTrie* trie = create_trie(initial_capacity);
    if (!trie) {
        free_formula(f_ptr);
        free_bitset(all_clauses_mask);
        print_json_error(filename, mode_str, "alloc_fail", "create_trie");
        return 1;
    }

    // PHASE 0 : construction de l'arbre.
    Node* root = NULL;
    double time_phase0_ms = 0.0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    if (execution_mode == TREE_MANUAL) {
        // Re-construire l'arbre manuel de la Figure 2 (5 vars, 4 clauses).
        Node* leaf_x1 = create_leaf_node(NODE_LEAF_VAR, 1, f.num_clauses);
        Node* leaf_x2 = create_leaf_node(NODE_LEAF_VAR, 2, f.num_clauses);
        Node* A = create_internal_node(leaf_x1, leaf_x2, f.num_clauses);
        Node* leaf_c1 = create_leaf_node(NODE_LEAF_CLAUSE, 0, f.num_clauses);
        Node* leaf_c3 = create_leaf_node(NODE_LEAF_CLAUSE, 2, f.num_clauses);
        Node* B = create_internal_node(leaf_c1, leaf_c3, f.num_clauses);
        Node* v_node = create_internal_node(A, B, f.num_clauses);
        Node* leaf_x3 = create_leaf_node(NODE_LEAF_VAR, 3, f.num_clauses);
        Node* leaf_c4 = create_leaf_node(NODE_LEAF_CLAUSE, 3, f.num_clauses);
        Node* C = create_internal_node(leaf_x3, leaf_c4, f.num_clauses);
        Node* leaf_x5 = create_leaf_node(NODE_LEAF_VAR, 5, f.num_clauses);
        Node* leaf_c2 = create_leaf_node(NODE_LEAF_CLAUSE, 1, f.num_clauses);
        Node* D = create_internal_node(leaf_x5, leaf_c2, f.num_clauses);
        Node* E = create_internal_node(C, D, f.num_clauses);
        Node* leaf_x4 = create_leaf_node(NODE_LEAF_VAR, 4, f.num_clauses);
        Node* F_node = create_internal_node(E, leaf_x4, f.num_clauses);
        root = create_internal_node(v_node, F_node, f.num_clauses);
    } else if (execution_mode == TREE_RANDOM) {
        srand((unsigned int)seed);
        root = generate_random_tree(&f);
    } else if (execution_mode == TREE_LINEAR) {
        root = generate_linear_tree(&f);
    } else if (execution_mode == TREE_GREEDY) {
        root = generate_greedy_linear_tree(&f);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    time_phase0_ms = elapsed_ms_ts(t0, t1);

    if (!root) {
        free_formula(f_ptr);
        free_trie(trie);
        free_bitset(all_clauses_mask);
        print_json_error(filename, mode_str, "alloc_fail",
                         "construction de l'arbre");
        return 1;
    }

    // PHASE 1 : Procedure 1 (bottom-up PS'(Fv)).
    double time_phase1_ms = 0.0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    compute_ps_prime_bottom_up(root, &f, all_clauses_mask, trie);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    time_phase1_ms = elapsed_ms_ts(t0, t1);

    // PHASE 2 : Procedure 2 (top-down PS'(F_v_barre)).
    double time_phase2_ms = 0.0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    compute_ps_bar_top_down(root, &f, trie);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    time_phase2_ms = elapsed_ms_ts(t0, t1);

    // PHASE 3 : Procedure 3 (DP + construction du DAG d-DNNF).
    DNNFPool* dnnf_pool = create_dnnf_pool(1024);
    if (!dnnf_pool) {
        free_formula(f_ptr);
        free_trie(trie);
        free_tree(root);
        free_bitset(all_clauses_mask);
        print_json_error(filename, mode_str, "alloc_fail", "create_dnnf_pool");
        return 1;
    }
    double time_phase3_ms = 0.0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    DPResult dp_result = solve_dp(root, &f, all_clauses_mask, trie, dnnf_pool);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    time_phase3_ms = elapsed_ms_ts(t0, t1);

    // Si une allocation interne a echoue dans le trie ou le DNNFPool pendant
    // la DP, on ne peut pas faire confiance aux autres champs : on emet
    // print_json_error puis on cleanup et exit. Match avec B3 : aucun chemin
    // d'erreur ne sort de exit_code=1 sans JSON.
    if (dp_result.alloc_failed) {
        free_dnnf_pool(dnnf_pool);
        free_formula(f_ptr);
        free_trie(trie);
        free_tree(root);
        free_bitset(all_clauses_mask);
        print_json_error(filename, mode_str, "alloc_fail",
                         "trie ou DNNFPool a manque de memoire pendant la DP");
        return 1;
    }

    // Verification interne : dnnf_count == sharpsat_count (Lemme 4 BCMS).
    // Le drapeau dnnf_cnt_overflow capture l'overflow eventuel du recompte.
    int dnnf_cnt_overflow_local = 0;
    long long dnnf_cnt = dnnf_count(dp_result.dnnf_root, dnnf_pool,
                                    &dnnf_cnt_overflow_local);

    // Mesures structurelles.
    int ps_width      = calculate_tree_max_ps_width(root);
    int ps_width_pv   = calculate_tree_ps_width(root);
    int ps_width_pvb  = calculate_tree_ps_prime_barre_width(root);
    int trie_num_ps_sets = trie->num_ps_sets;
    int dnnf_nodes    = dnnf_pool->num_nodes;
    long long dnnf_edges = dp_result.dnnf_num_edges;
    long long maxsat  = dp_result.maxsat_value;
    long long sharpsat = dp_result.sharpsat_count;

    // Drapeaux d'overflow : on prefere les sticky flags propages depuis la DP
    // (B4) plutot que la detection post-hoc fragile sur la valeur finale (qui
    // ratait le wrap-around vers 0, bug observe sur type3_n>=200 dans le run 1).
    int sharpsat_overflow = dp_result.sharpsat_overflow ? 1 : 0;
    int dnnf_cnt_overflow = dnnf_cnt_overflow_local ? 1 : 0;

    // Match : si les deux ont overflow, on considere qu'ils sont en accord
    // (tous deux satures au meme grand entier). Sinon, comparaison stricte.
    int dnnf_count_match;
    if (sharpsat_overflow && dnnf_cnt_overflow) {
        dnnf_count_match = 1;
    } else if (sharpsat_overflow != dnnf_cnt_overflow) {
        dnnf_count_match = 0;
    } else {
        dnnf_count_match = (dnnf_cnt == sharpsat) ? 1 : 0;
    }

    // B5 : Borne BCMS 7 * k^3 * (n + m). Detection robuste de l'overflow via
    // chaine __builtin_mul_overflow. L'ancienne detection (psw > 2642245) ne
    // couvrait que l'overflow de psw^3 isole, pas les facteurs 7 et (n+m).
    // Preuve d'echec dans le run 1 : type1_v80_c100 linear, psw=757024 (sous
    // l'ancien seuil) a produit bound = -6764736377788170240 (negatif).
    int bound_overflow = 0;
    long long bound = 0;
    if (ps_width > 0) {
        long long psw_ll = (long long)ps_width;
        long long psw_sq, psw_cube, t7, nm;
        if (__builtin_mul_overflow(psw_ll, psw_ll, &psw_sq) ||
            __builtin_mul_overflow(psw_sq, psw_ll, &psw_cube) ||
            __builtin_mul_overflow(7LL, psw_cube, &t7)) {
            bound_overflow = 1;
        } else {
            nm = (long long)f.num_vars + (long long)f.num_clauses;
            if (__builtin_mul_overflow(t7, nm, &bound)) {
                bound_overflow = 1;
            }
        }
    }

    double total_ms = time_phase0_ms + time_phase1_ms + time_phase2_ms + time_phase3_ms;

    // SERIALISATION JSON : une seule ligne, pas d'espaces inutiles.
    printf("{");
    printf("\"schema_version\":1,");
    printf("\"status\":\"ok\",");
    printf("\"error_message\":null,");
    printf("\"file\":\""); print_json_escaped(filename); printf("\",");
    printf("\"mode\":\"%s\",", mode_str);
    printf("\"seed\":%d,", seed);
    printf("\"n_vars\":%d,", f.num_vars);
    printf("\"n_clauses\":%d,", f.num_clauses);
    printf("\"ps_width\":%d,", ps_width);
    printf("\"ps_width_pv\":%d,", ps_width_pv);
    printf("\"ps_width_pvb\":%d,", ps_width_pvb);
    printf("\"trie_num_ps_sets\":%d,", trie_num_ps_sets);
    printf("\"time_parsing_ms\":%.3f,", time_parsing_ms);
    printf("\"time_phase0_ms\":%.3f,", time_phase0_ms);
    printf("\"time_phase1_ms\":%.3f,", time_phase1_ms);
    printf("\"time_phase2_ms\":%.3f,", time_phase2_ms);
    printf("\"time_phase3_ms\":%.3f,", time_phase3_ms);
    printf("\"time_total_ms\":%.3f,", total_ms);
    printf("\"maxsat\":%lld,", maxsat);
    if (sharpsat_overflow) {
        printf("\"sharpsat\":\"overflow\",");
    } else {
        printf("\"sharpsat\":%lld,", sharpsat);
    }
    printf("\"sharpsat_overflow\":%s,", sharpsat_overflow ? "true" : "false");
    if (dnnf_cnt_overflow) {
        printf("\"dnnf_count_recomputed\":\"overflow\",");
    } else {
        printf("\"dnnf_count_recomputed\":%lld,", dnnf_cnt);
    }
    printf("\"dnnf_count_overflow\":%s,", dnnf_cnt_overflow ? "true" : "false");
    printf("\"dnnf_count_match\":%s,", dnnf_count_match ? "true" : "false");
    printf("\"dnnf_nodes\":%d,", dnnf_nodes);
    printf("\"dnnf_edges\":%lld,", dnnf_edges);
    if (bound_overflow) {
        printf("\"dnnf_bound_7k3nm\":\"overflow\",");
    } else {
        printf("\"dnnf_bound_7k3nm\":%lld,", bound);
    }
    printf("\"bound_overflow\":%s", bound_overflow ? "true" : "false");

    // ========================================================================
    // BENCH DES REQUETES SUR DAG COMPILE (mode --json-with-queries)
    // ========================================================================
    // 7 requetes mesurees in-process : CO, VA, CT, ME-find_model, CE, IM,
    // ME-enumerate (1er modele + total cape). 5 repetitions par requete,
    // 1ere jetee (warm-up cold), mediane des 4 suivantes.
    if (with_queries) {
        const int REPETITIONS = 5;
        const long long ENUM_ALL_CAP = 1000000LL;
        struct timespec qt0, qt1;
        double samples[5];

        // CO ---------------------------------------------------------------
        int co_result = 0;
        for (int r = 0; r < REPETITIONS; r++) {
            clock_gettime(CLOCK_MONOTONIC, &qt0);
            co_result = dnnf_consistency(dp_result.dnnf_root, dnnf_pool);
            clock_gettime(CLOCK_MONOTONIC, &qt1);
            samples[r] = elapsed_ms_ts(qt0, qt1);
        }
        double query_co_ms = median_of_5_drop_first(samples);

        // VA ---------------------------------------------------------------
        int va_result = 0;
        for (int r = 0; r < REPETITIONS; r++) {
            long long cv = 0, mm = 0;
            clock_gettime(CLOCK_MONOTONIC, &qt0);
            va_result = dnnf_validity(dp_result.dnnf_root, dnnf_pool,
                                      f.num_vars, &cv, &mm);
            clock_gettime(CLOCK_MONOTONIC, &qt1);
            samples[r] = elapsed_ms_ts(qt0, qt1);
        }
        double query_va_ms = median_of_5_drop_first(samples);

        // CT ---------------------------------------------------------------
        long long ct_count = 0;
        int ct_overflow = 0;
        for (int r = 0; r < REPETITIONS; r++) {
            int ovf = 0;
            clock_gettime(CLOCK_MONOTONIC, &qt0);
            ct_count = dnnf_count(dp_result.dnnf_root, dnnf_pool, &ovf);
            clock_gettime(CLOCK_MONOTONIC, &qt1);
            samples[r] = elapsed_ms_ts(qt0, qt1);
            ct_overflow = ovf;
        }
        double query_ct_ms = median_of_5_drop_first(samples);

        // ME-find_model ----------------------------------------------------
        // Skipper proprement si UNSAT (sinon dnnf_find_model retourne 0 sans
        // panique mais le timing perd du sens).
        double query_me_ms = 0.0;
        int me_skipped = (dp_result.sharpsat_count == 0
                          && !dp_result.sharpsat_overflow);
        if (!me_skipped) {
            int* mbuf = calloc(f.num_vars + 1, sizeof(int));
            for (int r = 0; r < REPETITIONS; r++) {
                clock_gettime(CLOCK_MONOTONIC, &qt0);
                (void)dnnf_find_model(dp_result.dnnf_root, dnnf_pool,
                                      f.num_vars, mbuf);
                clock_gettime(CLOCK_MONOTONIC, &qt1);
                samples[r] = elapsed_ms_ts(qt0, qt1);
            }
            free(mbuf);
            query_me_ms = median_of_5_drop_first(samples);
        }

        // CE : clause de reference = 1ere clause de F.
        // Resultat attendu = YES (F entraine trivialement ses propres clauses).
        int* ce_lits = malloc(((size_t)f.num_vars * 2 + 1) * sizeof(int));
        int ce_num_lits = 0;
        extract_first_clause_literals(&f, ce_lits, &ce_num_lits);
        long long ce_count = 0;
        int ce_result = 0;
        if (ce_num_lits == 0) {
            // Clause vide (defense, ne devrait pas arriver post-B1) : YES vacuously.
            ce_result = DNNF_ENTAILS_YES;
            samples[0] = samples[1] = samples[2] = samples[3] = samples[4] = 0.0;
        } else {
            for (int r = 0; r < REPETITIONS; r++) {
                long long cc = 0;
                clock_gettime(CLOCK_MONOTONIC, &qt0);
                ce_result = dnnf_entails(dp_result.dnnf_root, dnnf_pool,
                                         ce_lits, ce_num_lits, f.num_vars, &cc);
                clock_gettime(CLOCK_MONOTONIC, &qt1);
                samples[r] = elapsed_ms_ts(qt0, qt1);
                ce_count = cc;
            }
        }
        double query_ce_ms = median_of_5_drop_first(samples);
        free(ce_lits);

        // IM : terme de reference = gamma vide (= test "F est-elle tautologie ?").
        // Resultat attendu sur instances reelles = NO (F n'est presque jamais une tautologie).
        long long im_count = 0, im_target = 0;
        int im_k_distinct = 0, im_result = 0;
        for (int r = 0; r < REPETITIONS; r++) {
            long long cc = 0, tt = 0;
            int kd = 0;
            clock_gettime(CLOCK_MONOTONIC, &qt0);
            im_result = dnnf_is_implicant(dp_result.dnnf_root, dnnf_pool,
                                          f.num_vars, NULL, 0, &cc, &tt, &kd);
            clock_gettime(CLOCK_MONOTONIC, &qt1);
            samples[r] = elapsed_ms_ts(qt0, qt1);
            im_count = cc; im_target = tt; im_k_distinct = kd;
        }
        double query_im_ms = median_of_5_drop_first(samples);
        (void)im_count; (void)im_target; (void)im_k_distinct;

        // ME-enumerate (1er modele) : callback silencieux qui stoppe au 1er.
        double query_enum_first_ms = 0.0;
        if (!me_skipped) {
            for (int r = 0; r < REPETITIONS; r++) {
                clock_gettime(CLOCK_MONOTONIC, &qt0);
                (void)dnnf_enumerate(dp_result.dnnf_root, dnnf_pool, f.num_vars,
                                     enum_silent_first_cb, NULL);
                clock_gettime(CLOCK_MONOTONIC, &qt1);
                samples[r] = elapsed_ms_ts(qt0, qt1);
            }
            query_enum_first_ms = median_of_5_drop_first(samples);
        }

        // ME-enumerate (tous modeles, capes a ENUM_ALL_CAP). Skip si dnnf_count
        // depasse le cap (timeout garanti) ou si overflow (compte inconnu).
        // NB: La callback ASCII print_cb ferait du bourrage stdout ; on utilise
        // un cb silencieux qui se contente de compter.
        double query_enum_all_ms = 0.0;
        long long enum_count = 0;
        int enum_all_skipped = (ct_overflow || ct_count > ENUM_ALL_CAP || me_skipped);
        if (!enum_all_skipped) {
            // Callback silencieux : juste incrementer un compteur, jamais stopper.
            // NB on reuse enum_ascii_ctx : sans imprimer (limit=ENUM_ALL_CAP+1
            // mais le print_cb imprime si count<limit). Pour ne PAS imprimer,
            // on inline un cb local-style via un wrapper static.
            //
            // Simplification : on compte via une callback static dediee.
            for (int r = 0; r < REPETITIONS; r++) {
                long long counter = 0;
                clock_gettime(CLOCK_MONOTONIC, &qt0);
                (void)dnnf_enumerate(dp_result.dnnf_root, dnnf_pool, f.num_vars,
                                     enum_silent_cb, &counter);
                clock_gettime(CLOCK_MONOTONIC, &qt1);
                samples[r] = elapsed_ms_ts(qt0, qt1);
                enum_count = counter;
            }
            query_enum_all_ms = median_of_5_drop_first(samples);
        }

        // SERIALISATION JSON des champs query_*.
        printf(",\"query_repetitions\":%d", REPETITIONS);
        printf(",\"query_co_ms\":%.6f,\"query_co_result\":%d",
               query_co_ms, co_result);
        printf(",\"query_va_ms\":%.6f,\"query_va_result\":%d",
               query_va_ms, va_result);
        if (ct_overflow) {
            printf(",\"query_ct_ms\":%.6f,\"query_ct_count\":\"overflow\"",
                   query_ct_ms);
        } else {
            printf(",\"query_ct_ms\":%.6f,\"query_ct_count\":%lld",
                   query_ct_ms, ct_count);
        }
        if (me_skipped) {
            printf(",\"query_me_ms\":null");
        } else {
            printf(",\"query_me_ms\":%.6f", query_me_ms);
        }
        printf(",\"query_ce_ms\":%.6f,\"query_ce_result\":%d,\"query_ce_count\":%lld",
               query_ce_ms, ce_result, ce_count);
        printf(",\"query_im_ms\":%.6f,\"query_im_result\":%d",
               query_im_ms, im_result);
        if (me_skipped) {
            printf(",\"query_enum_first_ms\":null");
        } else {
            printf(",\"query_enum_first_ms\":%.6f", query_enum_first_ms);
        }
        if (enum_all_skipped) {
            printf(",\"query_enum_all_ms\":null,\"query_enum_count\":null,"
                   "\"query_enum_all_skipped\":true");
        } else {
            printf(",\"query_enum_all_ms\":%.6f,\"query_enum_count\":%lld,"
                   "\"query_enum_all_skipped\":false",
                   query_enum_all_ms, enum_count);
        }
    }

    printf("}\n");
    fflush(stdout);

    // NETTOYAGE.
    free_dnnf_pool(dnnf_pool);
    free_formula(f_ptr);
    free_trie(trie);
    free_tree(root);
    free_bitset(all_clauses_mask);

    return 0;
}