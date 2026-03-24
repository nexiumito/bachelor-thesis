#ifndef PROCEDURE2_H
#define PROCEDURE2_H

#include "../core/decomposition_tree.h"
#include "../core/formula.h"
#include "../utils/trie.h"

void compute_ps_bar_top_down(Node* root, SAT_Formula* f, BinaryTrie* trie);

#endif