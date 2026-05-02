#ifndef DP_TABLE_H
#define DP_TABLE_H

struct dnnf_node;

typedef struct {
    long long*           maxsat;
    long long*           sharpsat;
    struct dnnf_node**   dnnf;       // dnnf[i*cols+j] = phi_v(S_i, S'_j), ou NULL
    int                  num_rows;
    int                  num_cols;
    // Sticky overflow : leve a 1 des qu'une multiplication ou addition
    // long long sur sharpsat deborde dans une cellule de cette table.
    // Propage parent <- enfants par OR a la fin de chaque appel recursif
    // (cf. solve_dp_recursive). Une fois remonte a la table racine, exporte
    // dans DPResult.sharpsat_overflow et serialise par main.c.
    int                  overflow;
} DPTable;

// Retourne NULL si une allocation echoue (RLIMIT_AS, OOM). L'appelant
// doit tester et propager (typiquement en levant trie->alloc_failed ou
// en ne pas creant la cellule).
DPTable* create_dp_table(int rows, int cols);
void free_dp_table(DPTable* tab);

#endif
