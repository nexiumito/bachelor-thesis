#ifndef BITSET_H
#define BITSET_H

#include <stdint.h> 

typedef struct {
    int num_words;
    uint64_t* words;
} Bitset;

Bitset* create_bitset(int num_clauses);
Bitset* bitset_copy(Bitset* src, int num_clauses);
void free_bitset(Bitset* b);
void set_bit(Bitset* b, int clause_index);
void bitset_union_and_filter(Bitset* dest, Bitset* b1, Bitset* b2, Bitset* filter_mask);
void print_binary(Bitset* b, int num_clauses);

#endif
