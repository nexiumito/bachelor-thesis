#ifndef DNNF_QUERY_IS_IMPLICANT_H
#define DNNF_QUERY_IS_IMPLICANT_H

#include "../dnnf.h"

// ============================================================================
// IM : IMPLICANT CHECK (le terme gamma est-il un implicant de F ?)
// ============================================================================
//
// Reference : Darwiche & Marquis 2002, A Knowledge Compilation Map, Table 5.
//
// Equivalence utilisee :
//   gamma |= F  ssi  #SAT(F | gamma) = 2^(n - k)
// ou n = num_vars de F et k = nb de variables distinctes dans gamma. Apres
// conditionnement, le DAG peut perdre la lissite : on lisse avant de compter.
// ============================================================================

/**
 * Codes de retour de dnnf_is_implicant.
 */
#define DNNF_IS_IMPLICANT_YES         1   // gamma est implicant de F
#define DNNF_IS_IMPLICANT_NO          0   // gamma n'est pas implicant
#define DNNF_IS_IMPLICANT_UNSAT      -1   // F est UNSAT
#define DNNF_IS_IMPLICANT_BAD_VAR    -2   // litteral hors borne
#define DNNF_IS_IMPLICANT_OVERFLOW   -3   // n - k >= 63 (target ne tient pas) OU dnnf_count a sature

/**
 * Decide si le terme gamma = (l1 ^ ... ^ lk) est un implicant de F.
 *
 * @param literals      Tableau de litteraux DIMACS.
 * @param num_literals  Taille du tableau.
 * @param num_vars      Nombre de variables de F.
 * @param count_out     Si non-NULL, ecrit #SAT(F | gamma) apres lissage.
 * @param target_out    Si non-NULL, ecrit 2^(num_vars - k) ou k = nb vars
 *                      distinctes dans gamma.
 * @param k_distinct_out Si non-NULL, ecrit k (utile pour l'affichage).
 *
 * @return  Voir codes DNNF_IS_IMPLICANT_*.
 */
int dnnf_is_implicant(DNNFNode* root, DNNFPool* pool, int num_vars,
                      const int* literals, int num_literals,
                      long long* count_out, long long* target_out,
                      int* k_distinct_out);

#endif
