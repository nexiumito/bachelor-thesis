#include "ps_set.h"
#include "trie.h"

#include <stdlib.h>

PS_Set* create_ps_set(int initial_capacity) {
    PS_Set* set = malloc(sizeof(PS_Set));
    set->capacity = initial_capacity;
    set->size = 0;
    set->sets = malloc(set->capacity * sizeof(Bitset*)); // tableau pointeur vers bitset..
    set->ps_ids = malloc(set->capacity * sizeof(int)); // return du trie
    return set;
}


// Ajoute un Bitset au PS_Set du noeud SEULEMENT s'il n'y est pas déjà
void add_to_node_ps_set(PS_Set* set, Bitset* new_mask, int ps_id, int node_id, BinaryTrie* trie) {
    // O(1) : Est-ce que CE noeud a déjà vu CET identifiant ps_id ?
    if (trie->seen_array[ps_id] == node_id) {
        free_bitset(new_mask); // C'est un doublon local, on jette
        return;
    }
    
    // ce n'est pas un doublon --> on marque qu'on vient de le voir pour ce noeud
    trie->seen_array[ps_id] = node_id;
    
    
    // ajout si unique
    if (set->size == set->capacity) {
        set->capacity *= 2;
        set->sets = realloc(set->sets, set->capacity * sizeof(Bitset*));
        set->ps_ids = realloc(set->ps_ids, set->capacity * sizeof(int));
    }
    set->sets[set->size] = new_mask;
    set->ps_ids[set->size] = ps_id;
    set->size++;
}

void free_ps_set(PS_Set* ps_set) {
    if (!ps_set) return;

    // libérer chaque Bitset contenu dans le tableau
    if (ps_set->sets) {
        for (int i = 0; i < ps_set->size; i++) {
            if (ps_set->sets[i]) {
                free_bitset(ps_set->sets[i]);
            }
        }
        // libérer le tableau de pointeurs
        free(ps_set->sets);
    }

    // libérer le tableau des IDs
    if (ps_set->ps_ids) {
        free(ps_set->ps_ids);
    }

    // libérer la structure elle-même
    free(ps_set);
}
