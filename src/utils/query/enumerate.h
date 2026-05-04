#ifndef DNNF_QUERY_ENUMERATE_H
#define DNNF_QUERY_ENUMERATE_H

#include "../dnnf.h"

// ============================================================================
// ME (variante multi-modeles) : ENUMERATION DES MODELES
// ============================================================================
//
// Reference : Darwiche & Marquis 2002, A Knowledge Compilation Map, Table 5.
// d-DNNF satisfait ME en polytime (Lemme A.3, p. 244).
//
// Strategie : top-down avec callback ; pour AND k-aire, produit cartesien
// encode en CPS (continuation passing style).
//
// PIEGE : sur un DAG NON LISSE, les variables non visitees sur un chemin
// gardent leur valeur du modele precedent -> doublons probables. L'appelant
// doit smoother avant pour que le nombre de modeles produits == dnnf_count.
// ============================================================================

/**
 * Type de la callback invoquee pour chaque modele complet.
 *
 * @param model     Tampon possede par l'enumerator ; valide UNIQUEMENT pendant
 *                  l'appel.
 * @param num_vars  Taille utile (model[1..num_vars] valide ; model[0] non touche).
 * @param user_data Pointeur opaque transmis tel quel depuis dnnf_enumerate.
 * @return  0 pour continuer l'enumeration, 1 pour interrompre.
 */
typedef int (*dnnf_model_callback)(const int* model, int num_vars,
                                    void* user_data);

/**
 * Enumere tous les modeles satisfaisant le DAG. Pour chaque modele complet,
 * invoque cb. Retourne le nombre de modeles produits.
 *
 * Justification : d-DNNF satisfait ME (Table 5, Darwiche-Marquis 2002).
 *
 * @param root      Racine du DAG (NULL --> 0).
 * @param pool      Pool proprietaire ; necessaire.
 * @param num_vars  Nombre de variables.
 * @param cb        Callback non-NULL invoquee a chaque modele complet.
 * @param user_data Pointeur opaque transmis a chaque appel de cb.
 * @return  Nombre de modeles produits (>= 0). Si cb a retourne 1, le retour
 *          est le nombre de modeles emis avant l'interruption.
 */
long long dnnf_enumerate(DNNFNode* root, DNNFPool* pool, int num_vars,
                         dnnf_model_callback cb, void* user_data);

#endif
