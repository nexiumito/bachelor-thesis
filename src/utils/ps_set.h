#ifndef PS_SET_H
#define PS_SET_H

#include "bitset.h"
#include "trie.h"

typedef struct {
    Bitset** sets;  // tableau de pointeurs vers les Bitsets
    int* ps_ids;    // identifiants uniques associés venant du Trie
    int size;
    int capacity;
} PS_Set;

PS_Set* create_ps_set(int initial_capacity);
void add_to_node_ps_set(PS_Set* set, Bitset* new_mask, int ps_id, int node_id, BinaryTrie* trie);
void free_ps_set(PS_Set* ps_set);

#endif