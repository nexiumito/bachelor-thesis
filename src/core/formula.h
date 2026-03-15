#ifndef FORMULA_H
#define FORMULA_H

#include "../utils/bitset.h"

// FORMULE SAT
typedef struct {
    int num_vars;
    int num_clauses;
    
    // tableaux de pointeurs vers des Bitsets (1 pointeur par variable)
    Bitset** mask_pos; 
    Bitset** mask_neg; 
} SAT_Formula;


#endif