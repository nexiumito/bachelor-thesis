#ifndef DP_TABLE_H
#define DP_TABLE_H

struct dnnf_node;

typedef struct {
    long long*           maxsat;
    long long*           sharpsat;
    struct dnnf_node**   dnnf;       // dnnf[i*cols+j] = phi_v(S_i, S'_j), ou NULL
    int                  num_rows;
    int                  num_cols;
} DPTable;

DPTable* create_dp_table(int rows, int cols);
void free_dp_table(DPTable* tab);

#endif
