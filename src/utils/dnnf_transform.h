#ifndef DNNF_TRANSFORM_H
#define DNNF_TRANSFORM_H

#include "dnnf.h"

// ============================================================================
// DNNF_TRANSFORM : helpers internes (compress, smooth, condition)
// ============================================================================
//
// Ces fonctions ne sont PAS exposees comme requetes CLI. Elles servent de
// briques bas niveau aux requetes qui dependent du conditioning et du lissage
// (entails, is_implicant). dnnf_compress est aussi utilise comme optimisation
// post-DP.
//
// References :
//   - Bova et al. 2016 : hash-consing semantique (compress)
//   - Darwiche & Marquis 2002, Lemme A.1 page 244 : smoothing
//   - Darwiche & Marquis 2002, Definition 5.4 page 241 : conditioning (CD)
// ============================================================================

/**
 * Compresse le DAG accessible depuis root par hash-consing semantique.
 *
 * Parcours bottom-up avec memoisation par id. Pour chaque noeud, calcule
 * une cle canonique :
 *   - LIT_POS / LIT_NEG : (type, var_index)
 *   - AND / OR          : (type, ids des enfants tries croissant)
 *   - TRUE / FALSE      : deja singletons, conserves tels quels
 *
 * Pour les OR, les enfants canoniques tries sont dedupliques.
 *
 * Effet de bord : invalide pool->scope_by_id (les ids changent).
 *
 * Idempotent : un deuxieme appel sur la racine compressee retourne le
 * meme pointeur.
 */
DNNFNode* dnnf_compress(DNNFNode* root, DNNFPool* pool);

/**
 * (Re)calcule la table pool->scope_by_id : pour chaque noeud du pool,
 * un Bitset des variables mentionnees dans le sous-DAG.
 *
 * Idempotent. Effet de bord : positionne pool->num_vars et
 * pool->scope_capacity = pool->num_nodes.
 */
void dnnf_compute_scopes(DNNFPool* pool, int num_vars);

/**
 * Construit un DAG semantiquement equivalent qui est LISSE (chaque OR a tous
 * ses enfants avec la meme portee). Pour chaque OR avec enfants de portees
 * differentes, chaque enfant est remplace par AND(enfant, AND_v (v ou ~v))
 * sur les variables manquantes.
 *
 * Justification : Lemme A.1, Darwiche-Marquis 2002 (p. 244).
 *
 * Effet de bord : invalide pool->scope_by_id.
 */
DNNFNode* dnnf_smooth(DNNFNode* root, DNNFPool* pool, int num_vars);

/**
 * Conditionne le DAG sur la valeur d'une variable : retourne la racine d'un
 * nouveau DAG semantiquement equivalent a F | (var = value).
 *
 * Justification : d-DNNF satisfait CD (Table 7, Darwiche-Marquis 2002).
 * Definition formelle : Definition 5.4 du meme paper.
 *
 * Conservation du determinisme : preservee par construction.
 * Conservation de la lissesse : NON -- le DAG retourne peut etre non lisse.
 * L'appelant qui veut compter via dnnf_count doit appliquer dnnf_smooth ensuite.
 *
 * @param var    Variable a conditionner (>= 1).
 * @param value  Valeur a fixer (0 ou 1).
 *
 * Complexite : O(|D|) avec memoisation par id.
 */
DNNFNode* dnnf_condition(DNNFNode* root, DNNFPool* pool,
                         int var, int value);

/**
 * Libere les structures auxiliaires (hash-consing + table de portees) du pool.
 * Utilise par free_dnnf_pool pour cleanup. No-op si non allouees.
 */
void dnnf_transform_free_pool_extras(DNNFPool* pool);

#endif
