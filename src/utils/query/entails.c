#include "entails.h"
#include "count.h"
#include "../dnnf_transform.h"

int dnnf_entails(DNNFNode* root, DNNFPool* pool,
                 const int* literals, int num_literals, int num_vars,
                 long long* count_out) {
    if (count_out) *count_out = 0;

    if (!root) {
        // F est UNSAT : entraine vacuously toute clause.
        return DNNF_ENTAILS_VACUOUSLY;
    }

    // Reduction : F |= (l1 v ... v lk) <=> F ^ ~l1 ^ ... ^ ~lk est UNSAT
    //                                  <=> dnnf_count(condition(F, ~l1, ..., ~lk)) == 0.
    DNNFNode* current = root;
    for (int i = 0; i < num_literals; i++) {
        int lit = literals[i];
        int var = (lit > 0) ? lit : -lit;
        if (var < 1 || var > num_vars) {
            return DNNF_ENTAILS_BAD_VAR;
        }
        // Negation : si lit > 0, ~lit = (var = 0) ; sinon (var = 1).
        int neg_value = (lit > 0) ? 0 : 1;
        current = dnnf_condition(current, pool, var, neg_value);
    }

    // Comptage avec detection sticky d'overflow : la signature publique de
    // dnnf_count expose un canal d'overflow ; on le propage pour distinguer
    // "0 modeles strict" de "satures vers LLONG_MAX".
    int local_overflow = 0;
    long long c = dnnf_count(current, pool, &local_overflow);
    if (count_out) *count_out = c;

    if (local_overflow && c > 0) {
        // Compte sature : impossible de prouver l'UNSAT, verdict NO ambigu.
        // Sur c == 0, l'overflow est silencieux (0 reste 0 quoi qu'il arrive).
        return DNNF_ENTAILS_OVERFLOW;
    }
    return (c == 0) ? DNNF_ENTAILS_YES : DNNF_ENTAILS_NO;
}
