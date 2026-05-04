#include "enumerate.h"

#include <stdlib.h>

// Etat global de l'enumeration -- partage entre tous les frames recursifs.
typedef struct {
    int*                 model;        // tampon [num_vars+1], alloue ici
    int                  num_vars;
    dnnf_model_callback  cb;
    void*                user;
    long long            count;        // nb de modeles emis jusqu'ici
    int                  interrupted;  // 1 si la callback a retourne 1
} enum_state;

// Type de continuation : "que faire quand un sous-DAG a fini d'emettre un
// modele complet". Toute branche qui produit un modele appelle k(k_data).
typedef void (*enum_cont)(enum_state* st, void* k_data);

// Forward declaration -- recursion mutuelle avec and_cont_callback.
static void enum_at(DNNFNode* n, enum_state* st,
                    enum_cont k, void* k_data);

// Continuation pour un AND k-aire.
typedef struct {
    DNNFNode*    and_node;
    int          next_idx;
    enum_cont    parent_cont;
    void*        parent_data;
} and_cont_data;

static void and_cont_callback(enum_state* st, void* k_data) {
    if (st->interrupted) return;
    and_cont_data* c = (and_cont_data*)k_data;

    if (c->next_idx == c->and_node->num_children) {
        c->parent_cont(st, c->parent_data);
        return;
    }

    and_cont_data next = {
        .and_node    = c->and_node,
        .next_idx    = c->next_idx + 1,
        .parent_cont = c->parent_cont,
        .parent_data = c->parent_data,
    };
    enum_at(c->and_node->children[c->next_idx], st,
            and_cont_callback, &next);
}

static void enum_at(DNNFNode* n, enum_state* st,
                    enum_cont k, void* k_data) {
    if (st->interrupted) return;

    switch (n->type) {
        case DNNF_FALSE:
            return;

        case DNNF_TRUE:
            k(st, k_data);
            return;

        case DNNF_LIT_POS:
            st->model[n->var_index] = 1;
            k(st, k_data);
            return;

        case DNNF_LIT_NEG:
            st->model[n->var_index] = 0;
            k(st, k_data);
            return;

        case DNNF_OR:
            for (int i = 0; i < n->num_children; i++) {
                if (st->interrupted) return;
                enum_at(n->children[i], st, k, k_data);
            }
            return;

        case DNNF_AND: {
            if (n->num_children == 0) {
                k(st, k_data);
                return;
            }
            and_cont_data start = {
                .and_node    = n,
                .next_idx    = 1,
                .parent_cont = k,
                .parent_data = k_data,
            };
            enum_at(n->children[0], st, and_cont_callback, &start);
            return;
        }
    }
}

static void enum_top_cont(enum_state* st, void* k_data) {
    (void)k_data;
    if (st->interrupted) return;
    int rc = st->cb(st->model, st->num_vars, st->user);
    st->count++;
    if (rc == 1) st->interrupted = 1;
}

long long dnnf_enumerate(DNNFNode* root, DNNFPool* pool, int num_vars,
                         dnnf_model_callback cb, void* user_data) {
    if (!root || !pool || num_vars < 0 || !cb) return 0;

    int* model = calloc((size_t)num_vars + 1, sizeof(int));
    enum_state st = {
        .model       = model,
        .num_vars    = num_vars,
        .cb          = cb,
        .user        = user_data,
        .count       = 0,
        .interrupted = 0,
    };

    enum_at(root, &st, enum_top_cont, NULL);

    free(model);
    return st.count;
}
