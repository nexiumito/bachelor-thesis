#ifndef PROCEDURE1_H
#define PROCEDURE1_H

#include "../core/decomposition_tree.h"
#include "../core/formula.h"
#include "../utils/bitset.h"
#include "../utils/ps_set.h"
#include "../utils/trie.h"


PS_Set* compute_ps_prime_bottom_up(Node* node, SAT_Formula* f, Bitset* all_clauses_mask, BinaryTrie* trie);

#endif