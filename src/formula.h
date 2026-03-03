#ifndef FORMULA_H
#define FORMULA_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_VARS 100

// Structure globale pour stocker les informations de la formule F
typedef struct {
    int num_vars;
    int num_clauses;
    
    // On représentee un ensemble de clauses par un seul nombre entier uint64_t
    // Le bit à la position i correspond à la clause d'indice i
    // Si le bit vaut 1, la clause est dans l'ensemble, sinon non

    // masques pré-calculés (variables de 1 à n)
    uint64_t mask_pos[MAX_VARS + 1]; // liste de toutes les clauses qui sont satisfaites si x = vrai 
    uint64_t mask_neg[MAX_VARS + 1]; // liste de toutes les clauses qui sont satisfaites si x = faux 
} SAT_Formula;

// L'ensemble de clauses PS'(Fv). Stocke plusieurs combinaisons possibles de clauses satisfaites
typedef struct {
    uint64_t* masks; //tableau dynamique qui grandit avec size et capacity. Chaque élément du tableau est un masque (une combinaison)
    int size;
    int capacity;
} PS_Set;

// Le noeud de l'arbre de décomposition
typedef struct Node {
    bool is_leaf;
    int var_index;               
    uint64_t delta_clauses_mask; //masque indiquant quelles clauses sont à l'intérieur de delta(v) 
    struct Node* left; // pointeur vers les enfants de v
    struct Node* right;
    PS_Set* ps_prime_v; // pointeur pour stocker le résultat    
} Node;

#endif