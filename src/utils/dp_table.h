#ifndef DP_TABLE_H
#define DP_TABLE_H

typedef struct {
    long long* maxsat;
    long long* sharpsat;
    int num_rows;
    int num_cols;
} DPTable;

DPTable* create_dp_table(int rows, int cols);
void free_dp_table(DPTable* tab);

#endif
