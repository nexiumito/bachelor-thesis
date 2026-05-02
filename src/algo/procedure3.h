#ifndef PROCEDURE3_H
#define PROCEDURE3_H

#include "../core/decomposition_tree.h"
#include "../core/formula.h"
#include "../utils/bitset.h"
#include "../utils/dnnf.h"
#include "../utils/dp_table.h"
#include "../utils/reverse_maps.h"
#include "../utils/trie.h"

typedef struct {
    long long  maxsat_value;    // nombre maximum de clauses satisfaisables
    long long  sharpsat_count;  // nombre d'affectations satisfaisant toutes les clauses
    DNNFNode*  dnnf_root;       // racine du DAG d-DNNF, possedee par le pool (NULL si vide)
    long long  dnnf_num_edges;  // taille |D| (nombre d'aretes accessibles) pour l'affichage
    // Drapeau alloc_failed : 1 si une allocation interne a echoue dans le
    // trie ou le DNNFPool pendant la DP. Les autres champs ne sont alors
    // pas fiables ; le caller doit emettre print_json_error("alloc_fail").
    int        alloc_failed;
    // Drapeau sharpsat_overflow : 1 si une multiplication ou une addition
    // long long sur sharpsat a deborde au moins une fois dans la procedure 3
    // (sticky -- une fois leve, reste leve). Sur overflow, sharpsat_count
    // n'est pas fiable ; main.c serialise sharpsat="overflow" et le drapeau.
    int        sharpsat_overflow;
} DPResult;

DPResult solve_dp(Node* root, SAT_Formula* f,
                  Bitset* all_clauses_mask, BinaryTrie* trie,
                  DNNFPool* pool);

#endif
