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
} DPResult;

DPResult solve_dp(Node* root, SAT_Formula* f,
                  Bitset* all_clauses_mask, BinaryTrie* trie,
                  DNNFPool* pool);

#endif
