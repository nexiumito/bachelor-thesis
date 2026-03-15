#include "trie.h"

#include <stdio.h>
#include <stdlib.h>

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

    trie->seen_capacity = trie->capacity;
    trie->seen_array = calloc(trie->seen_capacity, sizeof(int));

    // noeud racine toujours à l'index 0
    allocate_trie_node(trie);

    return trie;
}


// Libère proprement tout le Trie 
void free_trie(BinaryTrie* trie) {
    if (trie) { 
        if (trie->nodes) free(trie->nodes);
        if (trie->seen_array) free(trie->seen_array); // NOUVEAU
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
                int new_node_index = allocate_trie_node(trie); 
                trie->nodes[current_node].left = new_node_index; 
            }
            current_node = trie->nodes[current_node].left;
        } 
        
        else {
            // clause est dans l'ensemble : droite
            if (trie->nodes[current_node].right == -1) {
                int new_node_index = allocate_trie_node(trie); 
                trie->nodes[current_node].right = new_node_index; 
            }
            current_node = trie->nodes[current_node].right;
        }
    }

    // feuille : profondeur = num_clauses
    if (trie->nodes[current_node].ps_id == -1) {
        // anti seg-fault
        if (trie->num_ps_sets >= trie->seen_capacity) {
            int old_cap = trie->seen_capacity;
            trie->seen_capacity *= 2; // On double la taille !
            trie->seen_array = realloc(trie->seen_array, trie->seen_capacity * sizeof(int));
            
            // realloc n'initialise pas à zéro, il faut le faire manuellement
            // pour que les nouvelles cases soient bien considérées comme "jamais vues"
            for (int i = old_cap; i < trie->seen_capacity; i++) {
                trie->seen_array[i] = 0; 
            }
        }
        // nouveau PS-set : attribution d'un identifiant unique
        trie->nodes[current_node].ps_id = trie->num_ps_sets++;
    }

    // retourne l'ID qu'il vienne d'être créé ou qu'il existait déjà
    return trie->nodes[current_node].ps_id;
}