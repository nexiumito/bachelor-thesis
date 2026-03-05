#ifndef FORMULA_H
#define FORMULA_H

#include <stdint.h>
#include <stdbool.h>


// BITSET DYNAMIQUE 
typedef struct {
    int num_words;    // nombre de uint64_t nécessaires (ceil(m / 64.0))
    uint64_t* words;  // tableau dynamique contenant les bits
} Bitset;


// ENSEMBLE PS' (PS_Set)
typedef struct {
    Bitset** sets;  // tableau de pointeurs vers les Bitsets
    int* ps_ids;    // identifiants uniques associés venant du Trie
    int size;
    int capacity;
} PS_Set;


// FORMULE SAT
typedef struct {
    int num_vars;
    int num_clauses;
    
    // tableaux de pointeurs vers des Bitsets (1 pointeur par variable)
    Bitset** mask_pos; 
    Bitset** mask_neg; 
} SAT_Formula;


// ARBRE DE DECOMPOSITION
typedef enum {
    NODE_INTERNAL,
    NODE_LEAF_VAR,
    NODE_LEAF_CLAUSE
} NodeType;

typedef struct Node {
    NodeType type;
    int index;           // ID de la variable OU ID de la clause (0 si interne)
    Bitset* delta_mask;  // masque dynamique indiquant quelles clauses sont dans delta(v)
    
    struct Node* left;
    struct Node* right;
    
    PS_Set* ps_prime_v;  
} Node;

#endif