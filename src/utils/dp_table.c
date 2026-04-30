#include "dp_table.h"

#include <stdlib.h>

/**
 * Cree une table de programmation dynamique Tab_v pour un noeud v.
 *
 * La table est indexee par (C, C') appartient a PS'(Fv) x PS'(F_v_barre), stockee
 * comme un tableau 1D de taille rows x cols. Trois tableaux paralleles :
 *   - maxsat[i*cols+j]   : valeur MaxSAT pour la paire (i, j), initialisee a -1
 *   - sharpsat[i*cols+j] : compteur #SAT pour la paire (i, j), initialise a 0
 *   - dnnf[i*cols+j]     : phi_v(S_i, S'_j), pointeur emprunte au DNNFPool,
 *                          initialise a NULL (cellule sans modele -> pas de
 *                          contribution au DAG)
 *
 * Correspond a Tab_v(C, C') de la Contrainte (1) du papier (page 69).
 *
 * @param rows  Nombre de lignes = |PS'(Fv)|.
 * @param cols  Nombre de colonnes = |PS'(F_v_barre)|.
 * @return      Pointeur vers la table allouee.
 */
DPTable* create_dp_table(int rows, int cols) {
    DPTable* tab = malloc(sizeof(DPTable));
    tab->num_rows = rows;
    tab->num_cols = cols;

    int total = rows * cols;
    if (total == 0) {
        tab->maxsat = NULL;
        tab->sharpsat = NULL;
        tab->dnnf = NULL;
        return tab;
    }

    tab->maxsat = malloc(total * sizeof(long long));
    tab->sharpsat = calloc(total, sizeof(long long));
    tab->dnnf = calloc(total, sizeof(struct dnnf_node*));
    for (int i = 0; i < total; i++) {
        tab->maxsat[i] = -1;
    }

    return tab;
}

/**
 * Libere la memoire d'une table DP.
 *
 * Regle d'or : tab->dnnf[k] est un pointeur emprunte au DNNFPool. Ne JAMAIS
 * appeler free() dessus. Le tableau lui-meme, oui (juste les pointeurs).
 *
 * @param tab  Pointeur vers la table a liberer (peut etre NULL).
 */
void free_dp_table(DPTable* tab) {
    if (!tab) return;

    if (tab->maxsat) free(tab->maxsat);
    if (tab->sharpsat) free(tab->sharpsat);
    if (tab->dnnf) free(tab->dnnf);     // libere le tableau de pointeurs, pas les noeuds
    free(tab);
}
