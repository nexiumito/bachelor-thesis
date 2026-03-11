#include <stdio.h>
#include <stdlib.h>
#include "trie.h"

// Alloue un nouveau noeud dans le pool (gère automatiquement le redimensionnement du pool si la capacité est atteinte)
static int allocate_trie_node(BinaryTrie* trie) {
    if (trie->next_free >= trie->capacity) {
        trie->capacity *= 2;
        trie->nodes = realloc(trie->nodes, trie->capacity * sizeof(TrieNode));
        if (!trie->nodes) { // adresse mémoire pour le tableau de nodes non valide
            fprintf(stderr, "Erreur : Impossible d'allouer de la mémoire pour le Trie Binaire.\n");
            exit(EXIT_FAILURE);
        }
    }
    
    int node_index = trie->next_free++;
    trie->nodes[node_index].left = -1;
    trie->nodes[node_index].right = -1;
    trie->nodes[node_index].ps_id = -1;
    
    return node_index;
}


// Initialise le binary trie. initial_capacity : nombre de noeuds pré-alloués pour éviter les realloc fréquents (par défaut 1024)
BinaryTrie* create_trie(int initial_capacity) {
    BinaryTrie* trie = malloc(sizeof(BinaryTrie));
    trie->capacity = initial_capacity > 0 ? initial_capacity : 1024;
    trie->nodes = malloc(trie->capacity * sizeof(TrieNode));
    if (!trie->nodes) { // pas assez de mémoire
        fprintf(stderr, "Erreur : Impossible d'allouer de la mémoire pour le Trie Binaire.\n");
        exit(EXIT_FAILURE);
    }
    trie->next_free = 0;
    trie->num_ps_sets = 0;

    // noeud racine toujours à l'index 0
    allocate_trie_node(trie);

    return trie;
}


// Libère proprement tout le Trie 
void free_trie(BinaryTrie* trie) {
    if (trie) { 
        if (trie->nodes) {
            free(trie->nodes);
        }
        free(trie);
    }
}

/*
 * Insère un PS-set (Bitset) dans le Trie ou le retrouve s'il existe déjà.
 * Retourne son identifiant unique (ps_id). 
 */
int insert_or_get_ps_set(BinaryTrie* trie, Bitset* ps_set, int num_clauses) {
    int current_node = 0; // commence à la racine (index 0)

    // parcourt chaque clause (chaque bit) du PS-set
    for (int i = 0; i < num_clauses; i++) {
        // extraction du bit i depuis notre tableau d'entiers 64-bits
        int word_index = i / 64;
        int bit_index = i % 64;
        uint64_t bit = (ps_set->words[word_index] >> bit_index) & 1ULL;

        if (bit == 0) {
            // clause pas dans l'ensemble : gauche
            if (trie->nodes[current_node].left == -1) {
                trie->nodes[current_node].left = allocate_trie_node(trie);
            }
            current_node = trie->nodes[current_node].left;
        } 
        
        else {
            // clause est dans l'ensemble : droite
            if (trie->nodes[current_node].right == -1) {
                trie->nodes[current_node].right = allocate_trie_node(trie);
            }
            current_node = trie->nodes[current_node].right;
        }
    }

    // feuille : profondeur = num_clauses
    if (trie->nodes[current_node].ps_id == -1) {
        // nouveau PS-set : attribution d'un identifiant unique
        trie->nodes[current_node].ps_id = trie->num_ps_sets++;
    }

    // retourne l'ID qu'il vienne d'être créé ou qu'il existait déjà
    return trie->nodes[current_node].ps_id;
}