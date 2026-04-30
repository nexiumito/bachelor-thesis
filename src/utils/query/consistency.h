#ifndef DNNF_QUERY_CONSISTENCY_H
#define DNNF_QUERY_CONSISTENCY_H

#include "../dnnf.h"

// ============================================================================
// CO : CONSISTENCY (la formule est-elle satisfaisable ?)
// ============================================================================
//
// Reference : Darwiche & Marquis 2002, A Knowledge Compilation Map, Table 5.
// La decomposabilite suffit (DNNF satisfait CO en polytime).
// ============================================================================

/**
 * Decide si le DAG admet au moins un modele.
 *
 * Implementation : retourne (dnnf_count(root, pool) > 0).
 *
 * @return  1 si SAT (au moins un modele), 0 si UNSAT, ou si root == NULL.
 */
int dnnf_consistency(DNNFNode* root, DNNFPool* pool);

#endif
