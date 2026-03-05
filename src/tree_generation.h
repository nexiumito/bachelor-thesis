#ifndef TREE_GENERATION_H
#define TREE_GENERATION_H

#include "formula.h"
#include "procedure1.h"

Node* generate_random_tree(SAT_Formula* f);
void free_tree(Node* root);

#endif