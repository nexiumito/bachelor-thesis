#ifndef TRIE_H
#define TRIE_H

#include "bitset.h"


// Un noeud du binary trie
typedef struct {
    int left;   // index de l'enfant pour le bit 0 (-1 si n'existe pas)
    int right;  // index de l'enfant pour le bit 1 (idem)
    int ps_id;  // identifiant unique du PS-set si c'est une feuille (sinon -1)
} TrieNode;


// Memory Pool pour le binary trie
typedef struct {
    TrieNode* nodes;  // tableau contigu en mémoire
    int capacity;     // capacité totale actuelle du tableau
    int next_free;    // prochain index disponible (donc taille actuelle)
    int num_ps_sets;  // compteur global pour attribuer les ID uniques
    
    int* seen_array;   
    int seen_capacity;
} BinaryTrie;


BinaryTrie* create_trie(int initial_capacity);
int insert_or_get_ps_set(BinaryTrie* trie, Bitset* ps_set, int num_clauses);
void free_trie(BinaryTrie* trie);

#endif