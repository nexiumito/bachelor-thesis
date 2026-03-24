#include "trie.h"

#include <stdio.h>
#include <stdlib.h>

/**
 * Alloue un nouveau noeud dans le pool du trie.
 *
 * Gère automatiquement le redimensionnement (doublement de capacité)
 * si le pool est plein. Le nouveau noeud est initialisé sans enfants
 * (left = right = -1) et sans identifiant (ps_id = -1).
 *
 * @param trie  Le trie binaire contenant le pool de noeuds.
 * @return      Index du noeud nouvellement alloué dans le tableau nodes[].
 */
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


/**
 * Crée et initialise un trie binaire pour la déduplication des PS-sets.
 *
 * Le trie sert à gérer les PS-sets comme décrit en
 * Section 3 (page 68) du papier : "We use a binary trie datastructure.
 * We can add and retrieve a PS-set to and from a trie in O(|cla(F)|) time."
 *
 * Chaque PS-set est un chemin de la racine à une feuille à profondeur
 * num_clauses, où chaque bit du bitset détermine si on va à gauche (0)
 * ou à droite (1). Les feuilles stockent un identifiant unique (ps_id).
 *
 * Le seen_array permet la déduplication O(1) dans add_to_node_ps_set :
 * seen_array[ps_id] = node_id indique que le noeud node_id a déjà vu
 * ce PS-set.
 *
 * @param initial_capacity  Nombre de noeuds pré-alloués (défaut recommandé : 1024+).
 * @return                  Pointeur vers le trie initialisé.
 */
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


/**
 * Libère toute la mémoire du trie binaire.
 *
 * @param trie  Pointeur vers le trie à libérer (peut être NULL).
 */
void free_trie(BinaryTrie* trie) {
    if (trie) { 
        if (trie->nodes) free(trie->nodes);
        if (trie->seen_array) free(trie->seen_array); 
        free(trie);
    }
}

/**
 * Insère un PS-set dans le trie ou retrouve son identifiant s'il existe déjà.
 *
 * Parcourt le bitset bit par bit (clause par clause) : bit 0 --> enfant gauche,
 * bit 1 --> enfant droit. À la feuille (profondeur = num_clauses), si aucun
 * ps_id n'est attribué, on en crée un nouveau. Garantit l'unicité : deux
 * bitsets identiques reçoivent toujours le même ps_id.
 *
 * Complexité : O(num_clauses) = O(|cla(F)|) par insertion/recherche.
 *
 * @param trie         Le trie binaire.
 * @param ps_set       Le bitset représentant le PS-set à insérer/chercher.
 * @param num_clauses  Nombre total de clauses (profondeur du trie).
 * @return             Identifiant unique (ps_id) du PS-set.
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
            trie->seen_capacity *= 2; // double la taille 
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