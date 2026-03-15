#include "bitset.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

Bitset* create_bitset(int num_clauses) {
    Bitset* b = malloc(sizeof(Bitset));
    b->num_words = (num_clauses + 63) / 64; // arrondi supérieur
    b->words = calloc(b->num_words, sizeof(uint64_t)); // met tout à 0
    return b;
}


void free_bitset(Bitset* b) {
    if (b) {
        if (b->words) free(b->words);
        free(b);
    }
}


Bitset* bitset_copy(Bitset* src, int num_clauses) {
    Bitset* dest = create_bitset(num_clauses);
    memcpy(dest->words, src->words, src->num_words * sizeof(uint64_t)); // copie mémoire
    return dest;
}


void set_bit(Bitset* b, int clause_index) {
    int word_idx = clause_index / 64;
    int bit_idx = clause_index % 64;
    b->words[word_idx] |= (1ULL << bit_idx);
}


// dest = (b1 U b2) ∩ filter_mask
void bitset_union_and_filter(Bitset* dest, Bitset* b1, Bitset* b2, Bitset* filter_mask) {
    for (int i = 0; i < dest->num_words; i++) {
        uint64_t un = b1->words[i] | b2->words[i];
        dest->words[i] = un & filter_mask->words[i];
    }
}
