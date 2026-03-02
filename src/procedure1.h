#ifndef PROCEDURE1_H
#define PROCEDURE1_H

#include "formula.h"

// Uniquement les signatures des fonctions de la procédure 1
PS_Set* compute_ps_prime_bottom_up(Node* node, SAT_Formula* f, uint64_t all_clauses_mask);
void add_to_ps_set(PS_Set* set, uint64_t mask);
PS_Set* compute_leaf_ps_prime(Node* leaf, SAT_Formula* f, uint64_t all_clauses_mask);

#endif