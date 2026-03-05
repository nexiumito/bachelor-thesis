#ifndef PROCEDURE1_H
#define PROCEDURE1_H

#include "formula.h"
#include "trie.h"


Bitset* create_bitset(int num_clauses);
void free_bitset(Bitset* b);
Bitset* bitset_copy(Bitset* src, int num_clauses);
void bitset_union_and_filter(Bitset* dest, Bitset* b1, Bitset* b2, Bitset* filter_mask);

PS_Set* compute_ps_prime_bottom_up(Node* node, SAT_Formula* f, Bitset* all_clauses_mask, BinaryTrie* trie);

#endif