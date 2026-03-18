#ifndef DECOMPOSITION_TREE_H
#define DECOMPOSITION_TREE_H

#include "../utils/bitset.h"
#include "../utils/ps_set.h"
#include "formula.h"

// ARBRE DE DECOMPOSITION
typedef enum {
    NODE_INTERNAL,
    NODE_LEAF_VAR,
    NODE_LEAF_CLAUSE
} NodeType;

typedef struct Node {
    int id;
    NodeType type;
    int index;           // ID de la variable OU ID de la clause (0 si interne)
    Bitset* delta_mask;  // masque dynamique indiquant quelles clauses sont dans delta(v)
    
    struct Node* left;
    struct Node* right;
    
    PS_Set* ps_prime_v; // pour un noeud v : 
    PS_Set* ps_prime_v_barre;  
} Node;

Node* create_leaf_node(NodeType type, int index, int num_clauses);
Node* create_internal_node(Node* left, Node* right, int num_clauses);
Node* generate_random_tree(SAT_Formula* f);
Node* generate_linear_tree(SAT_Formula* f);
void free_tree(Node* root);
int calculate_tree_ps_width(Node* node);
int calculate_tree_ps_prime_barre_width(Node* node);
int calculate_tree_max_ps_width(Node* node);

#endif