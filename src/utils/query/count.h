#ifndef DNNF_QUERY_COUNT_H
#define DNNF_QUERY_COUNT_H

#include "../dnnf.h"

// ============================================================================
// CT : MODEL COUNTING (#SAT)
// ============================================================================
//
// Reference : Darwiche & Marquis 2002, A Knowledge Compilation Map, Table 5.
// d-DNNF satisfait CT en polytime grace a la decomposabilite (AND -> produit)
// et au determinisme (OR -> somme sans double comptage).
// ============================================================================

/**
 * Nombre de modeles du DAG via parcours DFS post-ordre avec memoisation.
 * Regles : TRUE=1, FALSE=0, LIT=1, AND=produit, OR=somme.
 * Retourne 0 si root == NULL. Complexite O(|D|).
 *
 * @param pool  Proprietaire du DAG ; pool->num_nodes borne les ids accessibles.
 */
long long dnnf_count(DNNFNode* root, DNNFPool* pool);

/**
 * Construit la table memoisee des comptes pour tous les noeuds accessibles
 * depuis root. Pour tout noeud N accessible : table[N->id] = #modeles du
 * sous-DAG enracine en N. Les entrees pour les noeuds non accessibles
 * restent 0.
 *
 * Le tableau retourne est possede par l'appelant (free obligatoire).
 * Necessaire pour les requetes qui doivent decider OR-par-OR quel enfant
 * contient au moins un modele (cf. find_model).
 */
long long* dnnf_count_table(DNNFNode* root, DNNFPool* pool);

#endif
