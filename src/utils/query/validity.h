#ifndef DNNF_QUERY_VALIDITY_H
#define DNNF_QUERY_VALIDITY_H

#include "../dnnf.h"

// ============================================================================
// VA : VALIDITY (la formule est-elle une tautologie ?)
// ============================================================================
//
// Reference : Darwiche & Marquis 2002, A Knowledge Compilation Map, Table 5.
// d-DNNF satisfait VA en polytime grace au determinisme : VA est equivalent
// a "#SAT(F) == 2^n", ce qui se decide via CT (model counting).
//
// REMARQUE : sur un DAG non lisse, la comparaison count == 2^n peut etre
// faussee par les variables manquantes dans certaines branches OR. Sur le
// public ce n'est pas un probleme : le DAG construit en Phase 1 est lisse
// par construction (cf. invariant Phase 1).
// ============================================================================

/**
 * Codes de retour de dnnf_validity.
 */
#define DNNF_VALIDITY_VALID      1   // F est une tautologie (#SAT == 2^n)
#define DNNF_VALIDITY_NOT_VALID  0   // F n'est pas une tautologie
#define DNNF_VALIDITY_UNSAT     -1   // F est UNSAT (cas degenere : non valide)
#define DNNF_VALIDITY_OVERFLOW  -2   // num_vars >= 63 : 2^n ne tient pas en long long

/**
 * Decide si le DAG est une tautologie (toute affectation est un modele).
 *
 * Si count_out != NULL, ecrit dnnf_count(root, pool) (utile pour l'affichage).
 * Si max_models_out != NULL, ecrit 2^num_vars (ou -1 si overflow).
 *
 * @param num_vars  Nombre total de variables de la formule.
 * @return  Voir codes DNNF_VALIDITY_*.
 */
int dnnf_validity(DNNFNode* root, DNNFPool* pool, int num_vars,
                  long long* count_out, long long* max_models_out);

#endif
