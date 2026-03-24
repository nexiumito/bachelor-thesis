#include "dp_table.h"

#include <stdlib.h>

/**
 * Crée une table de programmation dynamique Tab_v pour un noeud v.
 *
 * La table est indexée par (C, C') appartient à PS'(Fv) x PS'(F_v_barre), stockée
 * comme un tableau 1D de taille rows x cols. Deux tableaux parallèles :
 *   - maxsat[i*cols+j] : valeur MaxSAT pour la paire (i, j), initialisée à -1
 *   - sharpsat[i*cols+j] : compteur #SAT pour la paire (i, j), initialisé à 0
 *
 * Correspond à Tab_v(C, C') de la Contrainte (1) du papier (page 69).
 *
 * @param rows  Nombre de lignes = |PS'(Fv)|.
 * @param cols  Nombre de colonnes = |PS'(F_v_barre)|.
 * @return      Pointeur vers la table allouée.
 */
DPTable* create_dp_table(int rows, int cols) {
    DPTable* tab = malloc(sizeof(DPTable));
    tab->num_rows = rows;
    tab->num_cols = cols;

    int total = rows * cols;
    if (total == 0) {
        tab->maxsat = NULL;
        tab->sharpsat = NULL;
        return tab;
    }

    tab->maxsat = malloc(total * sizeof(long long));
    tab->sharpsat = calloc(total, sizeof(long long));
    for (int i = 0; i < total; i++) {
        tab->maxsat[i] = -1;
    }

    return tab;
}

/**
 * Libère la mémoire d'une table DP.
 *
 * @param tab  Pointeur vers la table à libérer (peut être NULL).
 */
void free_dp_table(DPTable* tab) {
    if (!tab) return;

    if (tab->maxsat) free(tab->maxsat);
    if (tab->sharpsat) free(tab->sharpsat);
    free(tab);
}
