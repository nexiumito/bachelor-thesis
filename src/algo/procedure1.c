#include "procedure1.h" 

#include <stddef.h>

// ============================================================================
// PROCEDURE 1 : GENERATION DE PS'(F_v)
// ============================================================================

/**
 * Calcule PS'(Fv) pour un noeud feuille v (cas de base de la Procédure 1).
 *
 * Pour une feuille variable x :
 *   - Deux affectations possibles (x=0, x=1), chacune produisant un
 *     PS-set C = sat'(Fv, T) = clauses satisfaites par T dans cla(F)\delta(v) (T une assignation).
 *   - C_true = mask_pos[x] ET cla(Fv), C_false = mask_neg[x] ET cla(Fv).
 *
 * Pour une feuille clause c :
 *   - Aucune variable dans le sous-arbre --> PS'(Fv) = {vide}.
 *
 * @param leaf     Noeud feuille (variable ou clause).
 * @param f        La formule SAT.
 * @param mask_Fv  Masque cla(Fv) = cla(F) \ delta(v).
 * @param trie     Le trie binaire pour la déduplication des PS-sets.
 * @return         PS_Set contenant les éléments de PS'(Fv).
 */
PS_Set* compute_leaf_ps_prime(Node* leaf, SAT_Formula* f, Bitset* mask_Fv, BinaryTrie* trie) {
    PS_Set* ps_v = create_ps_set(2);

    if (leaf->type == NODE_LEAF_VAR) {
        int x = leaf->index;

        // Cas 1 : variable x = Vrai (mask_pos[x] & mask_Fv)
        Bitset* C_true = create_bitset(f->num_clauses);
        for(int i = 0; i < C_true->num_words; i++) {
            C_true->words[i] = f->mask_pos[x]->words[i] & mask_Fv->words[i];
        }
        int id_true = insert_or_get_ps_set(trie, C_true, f->num_clauses);
        add_to_node_ps_set(ps_v, C_true, id_true, leaf->id, trie);

        // Cas 2 : variable x = Faux (mask_neg[x] & mask_Fv)
        Bitset* C_false = create_bitset(f->num_clauses);
        for(int i = 0; i < C_false->num_words; i++) {
            C_false->words[i] = f->mask_neg[x]->words[i] & mask_Fv->words[i];
        }
        int id_false = insert_or_get_ps_set(trie, C_false, f->num_clauses);
        add_to_node_ps_set(ps_v, C_false, id_false, leaf->id, trie);

    } else if (leaf->type == NODE_LEAF_CLAUSE) {
        // feuille clause = ensemble vide
        Bitset* C_empty = create_bitset(f->num_clauses);
        int id_empty = insert_or_get_ps_set(trie, C_empty, f->num_clauses);
        add_to_node_ps_set(ps_v, C_empty, id_empty, leaf->id, trie);
    }

    return ps_v;
}

/**
 * Calcule PS'(Fv) pour tous les noeuds de l'arbre par parcours bottom-up.
 *
 * Implémentation de la Procédure 1 du papier (page 69) :
 *   Pour un noeud interne v avec enfants c1, c2 :
 *     PS'(Fv) = { (C1 OU C2) ET cla(Fv) : C1 appartient à PS'(Fc1), C2 appartient à PS'(Fc2) }
 *
 * Le résultat est stocké dans node->ps_prime_v pour chaque noeud.
 * Les doublons sont éliminés via le trie binaire.
 *
 * Complexité totale : O(k^2 * m * (m+n)) (Théorème 1).
 *
 * @param node             Noeud courant (racine du sous-arbre à traiter).
 * @param f                La formule SAT.
 * @param all_clauses_mask Bitset avec tous les bits clause activés.
 * @param trie             Le trie binaire pour la déduplication.
 * @return                 PS_Set contenant PS'(Fv) pour ce noeud.
 */
PS_Set* compute_ps_prime_bottom_up(Node* node, SAT_Formula* f, Bitset* all_clauses_mask, BinaryTrie* trie) {
    // mask_Fv = cla(F) \ delta(v)
    Bitset* mask_Fv = create_bitset(f->num_clauses);
    for(int i = 0; i < mask_Fv->num_words; i++) {
        mask_Fv->words[i] = all_clauses_mask->words[i] & ~(node->delta_mask->words[i]);
    }

    // condition d'arrêt (feuille)
    if (node->type == NODE_LEAF_VAR || node->type == NODE_LEAF_CLAUSE) {
        PS_Set* leaf_set = compute_leaf_ps_prime(node, f, mask_Fv, trie);
        free_bitset(mask_Fv);
        
        node->ps_prime_v = leaf_set; 
        return leaf_set;
    }

    PS_Set* ps_c1 = compute_ps_prime_bottom_up(node->left, f, all_clauses_mask, trie);
    PS_Set* ps_c2 = compute_ps_prime_bottom_up(node->right, f, all_clauses_mask, trie);

    // ensemble L pour le noeud courant
    PS_Set* ps_v = create_ps_set(16);

    // produit cartéesien (C1 OU C2) ET cla(Fv)
    for (int i = 0; i < ps_c1->size; i++) {
        for (int j = 0; j < ps_c2->size; j++) {
            Bitset* C1 = ps_c1->sets[i];
            Bitset* C2 = ps_c2->sets[j];
            
            Bitset* C_filtered = create_bitset(f->num_clauses);
            bitset_union_and_filter(C_filtered, C1, C2, mask_Fv);
            
            // vérifie dans le binary trie pour récupérer un entier unique
            int id = insert_or_get_ps_set(trie, C_filtered, f->num_clauses);
            
            // tente de l'ajouter à l'ensemble du noeud
            add_to_node_ps_set(ps_v, C_filtered, id, node->id, trie);
        }
    }

    free_bitset(mask_Fv);
    
    node->ps_prime_v = ps_v; 
    return ps_v;
}