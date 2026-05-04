#ifndef DNNF_QUERY_ENTAILS_H
#define DNNF_QUERY_ENTAILS_H

#include "../dnnf.h"

// ============================================================================
// CE : CLAUSAL ENTAILMENT (F entraine-t-elle une clause donnee ?)
// ============================================================================
//
// Reference : Darwiche & Marquis 2002, A Knowledge Compilation Map, Table 5.
// Theoreme cle (page 240) : si une langue satisfait CO et CD, elle satisfait
// aussi CE en polytime. d-DNNF satisfait CO et CD.
//
// Reduction utilisee :
//   F |= (l1 v ... v lk)  ssi  F ^ ~l1 ^ ... ^ ~lk est UNSAT
//                         ssi  conditioner F sur (~l1, ..., ~lk) donne UNSAT
//
// Implementation : conditionnements successifs + dnnf_count.
// ============================================================================

/**
 * Codes de retour de dnnf_entails.
 */
#define DNNF_ENTAILS_YES         1   // F entraine la clause
#define DNNF_ENTAILS_NO          0   // F n'entraine pas la clause
#define DNNF_ENTAILS_VACUOUSLY  -1   // F est UNSAT, donc entraine vacuously toute clause
#define DNNF_ENTAILS_BAD_VAR    -2   // un litteral mentionne une variable hors borne
#define DNNF_ENTAILS_OVERFLOW   -3   // dnnf_count a sature et c > 0 (verdict NO ambigu)

/**
 * Decide si F entraine la clause (l1 v l2 v ... v lk).
 *
 * @param literals      Tableau de litteraux DIMACS (positifs ou negatifs, jamais 0).
 * @param num_literals  Taille du tableau.
 * @param num_vars      Nombre de variables de F (pour valider les litteraux).
 * @param count_out     Si non-NULL, ecrit #SAT(F | ~l1, ..., ~lk) (= nb d'affectations
 *                      qui satisfont F sans satisfaire la clause). 0 si F entraine
 *                      la clause, sinon le nombre de contre-exemples.
 *
 * @return  Voir codes DNNF_ENTAILS_*.
 */
int dnnf_entails(DNNFNode* root, DNNFPool* pool,
                 const int* literals, int num_literals, int num_vars,
                 long long* count_out);

#endif
