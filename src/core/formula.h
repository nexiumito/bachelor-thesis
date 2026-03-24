#ifndef FORMULA_H
#define FORMULA_H

#include "../utils/bitset.h"

// FORMULE SAT
typedef struct {
    int num_vars;
    int num_clauses;
    
    // tableaux de pointeurs vers des Bitsets (1 pointeur par variable)
    Bitset** mask_pos; // tableau de bitsets qui contient un bit à 1 pour chaque clause où la variable x apparaît en positif
    Bitset** mask_neg; // tableau de bitsets qui contient un bit à 1 pour chaque clause où la variable x apparaît en négatif
} SAT_Formula;


#endif