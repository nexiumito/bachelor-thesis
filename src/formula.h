#ifndef FORMULA_H
#define FORMULA_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_VARS 100

// Structure globale pour stocker les informations de la formule F
typedef struct {
    int num_vars;
    int num_clauses;
    
    // On veut représenter un ensemble de clauses par un seul nombre entier uint64_t (64 bits)
    // Le bit à la position i correspond à la clause d'indice i
    // Si le bit vaut 1, la clause est dans l'ensemble, sinon non

    // masques pré-calculés (indice 0 pas utilisé car les variables vont de 1 à n)
    uint64_t mask_pos[MAX_VARS + 1]; // liste de toutes les clauses qui sont satisfaites si x = vrai (clause où x apparait sous forme de littéral positif)
    uint64_t mask_neg[MAX_VARS + 1]; // liste de toutes les clauses qui sont satisfaites si x = faux (clause où x apparait sous forme de littéral negatif)
} SAT_Formula;

// L'ensemble de clauses PS'
typedef struct {
    uint64_t* masks;
    int size;
    int capacity;
} PS_Set;

// Le noeud de l'arbre de décomposition
typedef struct Node {
    bool is_leaf;
    int var_index;               
    uint64_t delta_clauses_mask; 
    struct Node* left;
    struct Node* right;
    PS_Set* ps_prime_v;          
} Node;

#endif