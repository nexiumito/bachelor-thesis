#include "decomposition_tree.h"
#include "../utils/ps_set.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>

static int next_node_id = 1;

Node* create_leaf_node(NodeType type, int index, int num_clauses) {
    Node* n = malloc(sizeof(Node));
    n->id = next_node_id++;
    n->type = type;
    n->index = index;
    n->left = NULL;
    n->right = NULL;

    n->ps_prime_v = NULL;
    n->ps_prime_v_barre = NULL;

    n->delta_mask = create_bitset(num_clauses);
    
    // si c'est une clause, on active le bit correspondant dans son delta_mask
    if (type == NODE_LEAF_CLAUSE) {
        int word_idx = index / 64;
        int bit_idx = index % 64;
        n->delta_mask->words[word_idx] |= (1ULL << bit_idx); // fonctionne très bien avec un simple "=", utilisation OU logique est une bonne pratique...
    }
    return n;
}

Node* create_internal_node(Node* left, Node* right, int num_clauses) {
    Node* n = malloc(sizeof(Node));
    n->id = next_node_id++;
    n->type = NODE_INTERNAL;
    n->index = 0;
    n->left = left;
    n->right = right;

    n->ps_prime_v = NULL;
    n->ps_prime_v_barre = NULL;

    n->delta_mask = create_bitset(num_clauses);
    
    // le masque d'un parent est l'union des masques de ses enfants
    for (int i = 0; i < n->delta_mask->num_words; i++) {
        n->delta_mask->words[i] = left->delta_mask->words[i] | right->delta_mask->words[i];
    }
    return n;
}

Node* generate_random_tree(SAT_Formula* f) {
    int total_leaves = f->num_vars + f->num_clauses;
    Node** pool = malloc(total_leaves * sizeof(Node*)); // tableau contenant des pointeurs vers tout les noeuds qui n'ont pas de parent
    
    // initialisation des feuilles
    int pool_size = 0;
    for (int i = 1; i <= f->num_vars; i++) {
        pool[pool_size++] = create_leaf_node(NODE_LEAF_VAR, i, f->num_clauses);
    }
    for (int i = 0; i < f->num_clauses; i++) {
        pool[pool_size++] = create_leaf_node(NODE_LEAF_CLAUSE, i, f->num_clauses);
    }
    
    // construction bottom-up aléatoire
    srand(time(NULL)); // génération seed aléatoire
    
    while (pool_size > 1) {
        // sélection de deux indices distincts au hasard
        int idx1 = rand() % pool_size; // nouvelle seed calculé à partir de l'ancienne à chaque fois
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


Node* generate_linear_tree(SAT_Formula* f) {
    // trouver la variable maximale de chaque clause
    int* max_var_of_clause = calloc(f->num_clauses, sizeof(int));
    
    for (int c = 0; c < f->num_clauses; c++) {
        int max_v = 1;
        // cherche la variable la plus grande présente dans la clause 'c'
        for (int v = 1; v <= f->num_vars; v++) {
            int word_idx = c / 64;
            int bit_idx = c % 64;
            // est-ce que la variable v est dans la clause c (en positif ou négatif) ?
            bool contains = ((f->mask_pos[v]->words[word_idx] >> bit_idx) & 1ULL) ||
                            ((f->mask_neg[v]->words[word_idx] >> bit_idx) & 1ULL);
            
            if (contains && v > max_v) {
                max_v = v;
            }
        }
        max_var_of_clause[c] = max_v;
    }

    Node* current_root = NULL;

    // construciton
    for (int v = 1; v <= f->num_vars; v++) {
        //  ajoute la variable v
        Node* leaf_v = create_leaf_node(NODE_LEAF_VAR, v, f->num_clauses);
        if (!current_root) {
            current_root = leaf_v;
        } else {
            current_root = create_internal_node(current_root, leaf_v, f->num_clauses);
        }
        
        // ajoute toutes les clauses qui se terminent à cette variable v
        for (int c = 0; c < f->num_clauses; c++) {
            if (max_var_of_clause[c] == v) {
                Node* leaf_c = create_leaf_node(NODE_LEAF_CLAUSE, c, f->num_clauses);
                current_root = create_internal_node(current_root, leaf_c, f->num_clauses);
            }
        }
    }

    free(max_var_of_clause);
    return current_root;
}

void free_tree(Node* root) {
    if (!root) return;
    free_tree(root->left);
    free_tree(root->right);
    free_bitset(root->delta_mask);

    if (root->ps_prime_v) free_ps_set(root->ps_prime_v);
    if (root->ps_prime_v_barre) free_ps_set(root->ps_prime_v_barre);

    free(root);
}


int calculate_tree_ps_width(Node* node) {
    if (!node || !node->ps_prime_v) return 0;
    
    int max_width = node->ps_prime_v->size;
    int left_width = calculate_tree_ps_width(node->left);
    int right_width = calculate_tree_ps_width(node->right);
    
    if (left_width > max_width) max_width = left_width;
    if (right_width > max_width) max_width = right_width;
    
    return max_width;
}

int calculate_tree_ps_prime_barre_width(Node* node) {
    if (!node || !node->ps_prime_v_barre) return 0;
    
    int max_width = node->ps_prime_v_barre->size;
    int left_width = calculate_tree_ps_prime_barre_width(node->left);
    int right_width = calculate_tree_ps_prime_barre_width(node->right);
    
    if (left_width > max_width) max_width = left_width;
    if (right_width > max_width) max_width = right_width;
    
    return max_width;
}
int calculate_tree_max_ps_width(Node* node) {
    int ps_width = calculate_tree_ps_width(node);
    int ps_prime_barre_width = calculate_tree_ps_prime_barre_width(node);
    return (ps_width > ps_prime_barre_width) ? ps_width : ps_prime_barre_width;
}

