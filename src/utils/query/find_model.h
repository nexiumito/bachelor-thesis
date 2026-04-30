#ifndef DNNF_QUERY_FIND_MODEL_H
#define DNNF_QUERY_FIND_MODEL_H

#include "../dnnf.h"

// ============================================================================
// ME (variante 1 modele) : RECHERCHE D'UN MODELE
// ============================================================================
//
// Reference : Darwiche & Marquis 2002, A Knowledge Compilation Map.
// Lemme A.3 (page 244) : si une langue satisfait CO et CD, elle satisfait
// aussi ME en polytime. d-DNNF satisfait CO et CD, donc ME est polytime.
// find_model est ME arrete au 1er modele : O(|D|).
// ============================================================================

/**
 * Trouve une affectation satisfaisant la formule encodee par le DAG.
 *
 * Parcours top-down avec pre-calcul des comptes :
 *   - AND : descendre dans tous les enfants ;
 *   - OR  : descendre dans le 1er enfant dont dnnf_count > 0 ;
 *   - LIT_POS(x) : model_out[x] = 1 ;
 *   - LIT_NEG(x) : model_out[x] = 0 ;
 *   - TRUE : rien ;
 *   - FALSE : ne doit pas etre atteint (parent OR aurait du le filtrer).
 *
 * Variables libres (DAG non lisse) : laissees a 0 par defaut.
 *
 * @param root      Racine du DAG (NULL --> -1).
 * @param pool      Pool proprietaire (necessaire pour borner la memoisation).
 * @param num_vars  Nombre de variables (>= 0). Index 0 inutilise (DIMACS).
 * @param model_out Tampon de taille >= num_vars+1 fourni par l'appelant ;
 *                  rempli en [1..num_vars]. model_out[0] non touche.
 * @return  1 si un modele a ete trouve, 0 si UNSAT, -1 sur erreur d'argument.
 *
 * Complexite : O(|D|) (table des comptes + descente unique).
 */
int dnnf_find_model(DNNFNode* root, DNNFPool* pool,
                    int num_vars, int* model_out);

#endif
