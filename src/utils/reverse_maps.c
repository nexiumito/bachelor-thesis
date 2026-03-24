#include "reverse_maps.h"

#include <stdlib.h>
#include <string.h>

/**
 * Crée les reverse maps pour la Procédure 3.
 *
 * Les reverse maps permettent de retrouver en O(1) l'index local d'un
 * PS-set dans un PS_Set à partir de son ps_id global. Trois maps sont
 * nécessaires par itération de la boucle principale (Procédure 3) :
 *   - map_v : ps_id --> index dans PS'(Fv)
 *   - map_c1_bar : ps_id --> index dans PS'(F_c1_barre)
 *   - map_c2_bar : ps_id --> index dans PS'(F_c2_barre)
 *
 * Initialisées à -1 (0xFF) pour indiquer "non présent".
 *
 * @param size  Taille des maps = nombre de PS-sets connus dans le trie.
 * @return      Pointeur vers la structure ReverseMaps allouée.
 */
ReverseMaps* create_reverse_maps(int size) {
    ReverseMaps* rm = malloc(sizeof(ReverseMaps));
    rm->map_size = size;
    rm->map_v = malloc(size * sizeof(int));
    rm->map_c1_bar = malloc(size * sizeof(int));
    rm->map_c2_bar = malloc(size * sizeof(int));

    memset(rm->map_v, 0xFF, size * sizeof(int));
    memset(rm->map_c1_bar, 0xFF, size * sizeof(int));
    memset(rm->map_c2_bar, 0xFF, size * sizeof(int));

    return rm;
}

/**
 * Libère la mémoire des reverse maps.
 *
 * @param rm  Pointeur vers les reverse maps à libérer (peut être NULL).
 */
void free_reverse_maps(ReverseMaps* rm) {
    if (!rm) return;

    free(rm->map_v);
    free(rm->map_c1_bar);
    free(rm->map_c2_bar);
    free(rm);
}

/**
 * Remplit une reverse map à partir d'un PS_Set.
 *
 * Pour chaque élément i du PS_Set, enregistre map[ps_ids[i]] = i,
 * permettant ensuite de retrouver l'index local d'un PS-set en O(1)
 * à partir de son ps_id global. Utilisé avant la boucle principale
 * de la Procédure 3 pour les trois PS_Sets cibles (PS'(Fv), PS'(F_c1_barre), PS'(F_c2_barre)).
 *
 * @param map       Tableau à remplir (indexé par ps_id).
 * @param map_size  Taille du tableau map.
 * @param ps        PS_Set source dont on extrait les ps_ids.
 */
void fill_reverse_map(int* map, int map_size, PS_Set* ps) {
    for (int i = 0; i < ps->size; i++) {
        int id = ps->ps_ids[i];
        if (id < map_size) {
            map[id] = i;
        }
    }
}

/**
 * Nettoie une reverse map en remettant à -1 les entrées utilisées.
 *
 * Remet uniquement les entrées correspondant aux ps_ids du PS_Set
 * donné, évitant un memset complet sur tout le tableau. Appelé après
 * chaque noeud interne dans la Procédure 3.
 *
 * @param map       Tableau à nettoyer.
 * @param map_size  Taille du tableau map.
 * @param ps        PS_Set dont les entrées doivent être réinitialisées.
 */
void clear_reverse_map(int* map, int map_size, PS_Set* ps) {
    for (int i = 0; i < ps->size; i++) {
        int id = ps->ps_ids[i];
        if (id < map_size) {
            map[id] = -1;
        }
    }
}
