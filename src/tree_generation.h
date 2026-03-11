#ifndef TREE_GENERATION_H
#define TREE_GENERATION_H

#include "formula.h"
#include "procedure1.h"

Node* create_leaf_node(NodeType type, int index, int num_clauses);
Node* create_internal_node(Node* left, Node* right, int num_clauses);
Node* generate_random_tree(SAT_Formula* f);
void free_tree(Node* root);

#endif