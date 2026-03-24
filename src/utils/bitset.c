#include "bitset.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * Crée un bitset initialisé à zéro pour représenter un sous-ensemble de clauses.
 *
 * Chaque bit i correspond à la clause i de la formule. Le bitset utilise
 * des mots de 64 bits (uint64_t) pour un stockage compact et des opérations
 * bit-à-bit efficaces sur les ensembles de clauses (PS-sets).
 *
 * @param num_clauses  Nombre total de clauses (détermine la taille du bitset).
 * @return             Pointeur vers le bitset alloué, tous bits à 0.
 */
Bitset* create_bitset(int num_clauses) {
    Bitset* b = malloc(sizeof(Bitset));
    b->num_words = (num_clauses + 63) / 64; // arrondi supérieur
    b->words = calloc(b->num_words, sizeof(uint64_t)); // met tout à 0
    return b;
}


/**
 * Libère la mémoire d'un bitset.
 *
 * @param b  Pointeur vers le bitset à libérer (peut être NULL).
 */
void free_bitset(Bitset* b) {
    if (b) {
        if (b->words) free(b->words);
        free(b);
    }
}


/**
 * Crée une copie profonde d'un bitset.
 *
 * @param src          Bitset source à copier.
 * @param num_clauses  Nombre total de clauses (pour dimensionner la copie).
 * @return             Nouveau bitset identique à src.
 */
Bitset* bitset_copy(Bitset* src, int num_clauses) {
    Bitset* dest = create_bitset(num_clauses);
    memcpy(dest->words, src->words, src->num_words * sizeof(uint64_t)); // copie mémoire
    return dest;
}


/**
 * Active le bit correspondant à une clause dans le bitset.
 *
 * @param b             Le bitset à modifier.
 * @param clause_index  Indice de la clause dont le bit doit être mis à 1.
 */
void set_bit(Bitset* b, int clause_index) {
    int word_idx = clause_index / 64;
    int bit_idx = clause_index % 64;
    b->words[word_idx] |= (1ULL << bit_idx);
}


/**
 * Calcule dest = (b1 OU b2) ET filter_mask.
 *
 * Opération centrale de la Procédure 1 (page 69) : pour chaque paire
 * (C1, C2) appartient à PS'(Fc1) × PS'(Fc2), on calcule (C1 OU C2) ET cla(Fv)
 * où filter_mask représente cla(Fv) = cla(F) \ delta(v).
 *
 * @param dest         Bitset destination recevant le résultat.
 * @param b1           Premier opérande (C1).
 * @param b2           Deuxième opérande (C2).
 * @param filter_mask  Masque de filtrage (cla(Fv)).
 */
void bitset_union_and_filter(Bitset* dest, Bitset* b1, Bitset* b2, Bitset* filter_mask) {
    for (int i = 0; i < dest->num_words; i++) {
        uint64_t un = b1->words[i] | b2->words[i];
        dest->words[i] = un & filter_mask->words[i];
    }
}
