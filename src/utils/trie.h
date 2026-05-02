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

    // Drapeau sticky : passe a 1 des qu'une allocation interne a echoue
    // (realloc des nodes ou du seen_array). Une fois leve, toutes les
    // operations suivantes deviennent des no-op qui retournent 0 sans
    // segfault. Le caller (solve_dp) doit verifier ce drapeau et router
    // vers un chemin d'erreur JSON propre.
    int alloc_failed;
} BinaryTrie;


// Retourne NULL si l'allocation initiale echoue.
BinaryTrie* create_trie(int initial_capacity);
// Sur echec d'allocation interne, leve trie->alloc_failed et retourne 0.
// Les appelants doivent verifier `trie->alloc_failed` apres une serie
// d'insertions (ou en sortie de boucle critique) pour propager l'erreur.
int insert_or_get_ps_set(BinaryTrie* trie, Bitset* ps_set, int num_clauses);
void free_trie(BinaryTrie* trie);

#endif