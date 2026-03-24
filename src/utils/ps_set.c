#include "ps_set.h"
#include "trie.h"

#include <stdlib.h>

/**
 * Crée un ensemble PS (Projection Satisfiable) vide avec une capacité initiale.
 *
 * Un PS_Set stocke les éléments de PS'(Fv) ou PS'(F_v_barre) pour un noeud v
 * de l'arbre de décomposition. Chaque élément est un bitset (sous-ensemble
 * de clauses de cla(F)) avec son identifiant unique ps_id venant du trie.
 *
 * @param initial_capacity  Capacité initiale du tableau (sera doublée si nécessaire).
 * @return                  Pointeur vers le PS_Set alloué.
 */
PS_Set* create_ps_set(int initial_capacity) {
    PS_Set* set = malloc(sizeof(PS_Set));
    set->capacity = initial_capacity;
    set->size = 0;
    set->sets = malloc(set->capacity * sizeof(Bitset*)); // tableau pointeur vers bitset..
    set->ps_ids = malloc(set->capacity * sizeof(int)); // return du trie
    return set;
}


/**
 * Ajoute un bitset au PS_Set d'un noeud, avec déduplication O(1).
 *
 * Utilise le seen_array du trie pour vérifier en O(1) si ce ps_id a déjà
 * été ajouté pour ce node_id. Correspond à la propriété du trie décrite
 * page 68 : "Trying to add a PS-set to a trie already containing an
 * equivalent PS-set will not alter the content of the trie."
 *
 * Si c'est un doublon, le bitset est libéré immédiatement. Sinon, il est
 * ajouté au tableau et le seen_array est mis à jour. Le tableau est
 * redimensionné (doublé) si nécessaire.
 *
 * @param set       Le PS_Set cible.
 * @param new_mask  Le bitset à ajouter (sera free si doublon).
 * @param ps_id     Identifiant unique du PS-set (venant du trie).
 * @param node_id   Identifiant du noeud de l'arbre (pour la déduplication).
 * @param trie      Le trie binaire (contient le seen_array).
 */
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

/**
 * Libère toute la mémoire d'un PS_Set.
 *
 * Libère chaque bitset contenu, le tableau de pointeurs, le tableau
 * des identifiants, puis la structure elle-même.
 *
 * @param ps_set  Pointeur vers le PS_Set à libérer (peut être NULL).
 */
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
