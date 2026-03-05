#include <stdlib.h>
#include <time.h>
#include "tree_generation.h"

Node* create_leaf_node(NodeType type, int index, int num_clauses) {
    Node* n = malloc(sizeof(Node));
    n->type = type;
    n->index = index;
    n->left = NULL;
    n->right = NULL;
    n->delta_mask = create_bitset(num_clauses);
    
    // si c'est une clause, on active le bit correspondant dans son delta_mask
    if (type == NODE_LEAF_CLAUSE) {
        int word_idx = index / 64;
        int bit_idx = index % 64;
        n->delta_mask->words[word_idx] |= (1ULL << bit_idx);
    }
    return n;
}

Node* create_internal_node(Node* left, Node* right, int num_clauses) {
    Node* n = malloc(sizeof(Node));
    n->type = NODE_INTERNAL;
    n->index = 0;
    n->left = left;
    n->right = right;
    n->delta_mask = create_bitset(num_clauses);
    
    // le masque d'un parent est l'union des masques de ses enfants
    for (int i = 0; i < n->delta_mask->num_words; i++) {
        n->delta_mask->words[i] = left->delta_mask->words[i] | right->delta_mask->words[i];
    }
    return n;
}

Node* generate_random_tree(SAT_Formula* f) {
    int total_leaves = f->num_vars + f->num_clauses;
    Node** pool = malloc(total_leaves * sizeof(Node*));
    
    // initialisation des feuilles
    int pool_size = 0;
    for (int i = 1; i <= f->num_vars; i++) {
        pool[pool_size++] = create_leaf_node(NODE_LEAF_VAR, i, f->num_clauses);
    }
    for (int i = 0; i < f->num_clauses; i++) {
        pool[pool_size++] = create_leaf_node(NODE_LEAF_CLAUSE, i, f->num_clauses);
    }
    
    // construction bottom-up aléatoire
    srand(time(NULL)); // générateur aléatoire
    
    while (pool_size > 1) {
        // sélection de deux indices distincts au hasard
        int idx1 = rand() % pool_size;
        int idx2;
        do {
            idx2 = rand() % pool_size;
        } while (idx1 == idx2);
        
        // création du nœud parent
        Node* parent = create_internal_node(pool[idx1], pool[idx2], f->num_clauses);
        
        // màj : on remplace idx1 par le parent, et on bouche le trou laissé par idx2 avec le dernier élément
        pool[idx1] = parent;
        pool[idx2] = pool[pool_size - 1];
        pool_size--;
    }
    
    Node* root = pool[0];
    free(pool);
    return root;
}

void free_tree(Node* root) {
    if (!root) return;
    free_tree(root->left);
    free_tree(root->right);
    free_bitset(root->delta_mask);
    free(root);
}