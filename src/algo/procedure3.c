#include "procedure3.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

// ============================================================================
// PROCEDURE 3 : PROGRAMMATION DYNAMIQUE POUR #SAT ET MAXSAT
//               + CONSTRUCTION DU DAG D-DNNF (Phase 1, Section 3.3 BCMS)
// ============================================================================
//
// Instrumentation Phase 1 : chaque cellule Tab_v(C, C') recoit un pointeur
// phi_v(C, C') stocke dans tab->dnnf[cell] (NULL si la cellule n'est pas
// generatrice). Les branches MaxSAT et #SAT sont conservees bit-a-bit
// identiques ; le DAG est construit en parallele, apres les mises a jour
// existantes, dans la meme iteration de boucle (pour ne traverser les
// triplets qu'une seule fois).
//
// Le DNNFPool est propage a travers toute la recursion ; aucun DNNFNode
// n'est libere par free_dp_table (cf. regle d'or dans dp_table.c).
// ============================================================================


/**
 * Cas de base
 * Calcule la table DP pour un noeud feuille (cas de base de la Procédure 3).
 *
 * Pour une feuille variable x (delta(v) = {x}, aucune clause) :
 *   - MaxSAT : 0 partout (rien à satisfaire localement dans delta(v))
 *   - #SAT : 2 si les deux affectations (x=0, x=1) produisent le même PS-set
 *            (|PS'(Fv)| = 1), 1 sinon (|PS'(Fv)| = 2) car deux assignations différentes satisfonts les mêmes clauses
 *   - DAG : emission d'une feuille litterale selon la polarite codee par
 *           bs_i = ps_v->sets[i]. Si |PS'(Fv)|=1,
 *           les deux polarites produisent le meme PS-set et on emet un OR
 *           (x ou ~x) -- cellule comptant 2.
 *
 * Pour une feuille clause c (delta(v) = {c}, aucune variable locale) :
 *   - Tab(vide, C') = 1 si c appartient à C' (clause satisfaite par l'extérieur), 0 sinon
 *   - DAG : singletons TRUE / FALSE du pool.
 *
 * @param leaf  Noeud feuille (variable ou clause).
 * @param f     Formule SAT (pour lire mask_pos/mask_neg sur les feuilles variables).
 * @param pool  Pool proprietaire des DNNFNode emis.
 * @return      Table DP allouée et initialisée.
 */
static DPTable* compute_leaf_table(Node* leaf, SAT_Formula* f, DNNFPool* pool) {
    PS_Set* ps_v = leaf->ps_prime_v;
    PS_Set* ps_v_bar = leaf->ps_prime_v_barre;

    DPTable* tab = create_dp_table(ps_v->size, ps_v_bar->size);

    if (leaf->type == NODE_LEAF_VAR) {
        // delta(v) = {variable x}, aucune clause dans delta(v)
        // MaxSAT : 0 partout (rien à satisfaire localement)
        // #SAT : il y a 2 affectations possibles (x=0, x=1)
        //   - Si les deux produisent le même PS-set --> ps_v->size = 1, count = 2
        //   - Sinon --> ps_v->size = 2, count = 1 pour chaque
        int x = leaf->index;
        long long count = (ps_v->size == 1) ? 2 : 1;

        // Precomputation de la feuille DAG commune a toutes les colonnes de
        // chaque ligne. Le ps_v_bar ne change rien a la polarite : seule la
        // ligne i determine l'assignation de x.
        DNNFNode** row_leaf = NULL;
        if (ps_v->size > 0 && ps_v_bar->size > 0) {
            row_leaf = malloc(ps_v->size * sizeof(DNNFNode*));
            if (ps_v->size == 2) {
                // Chaque ligne correspond a UNE polarite. On compare les
                // bitsets mot par mot a mask_pos[x] et mask_neg[x].
                Bitset* mp = f->mask_pos[x];
                Bitset* mn = f->mask_neg[x];
                int nw = mp->num_words;
                for (int i = 0; i < ps_v->size; i++) {
                    Bitset* bs_i = ps_v->sets[i];
                    int eq_pos = 1, eq_neg = 1;
                    for (int w = 0; w < nw; w++) {
                        if (bs_i->words[w] != mp->words[w]) eq_pos = 0;
                        if (bs_i->words[w] != mn->words[w]) eq_neg = 0;
                    }
                    if (eq_pos) {
                        row_leaf[i] = dnnf_make_literal(pool, x, 1);
                    } else if (eq_neg) {
                        row_leaf[i] = dnnf_make_literal(pool, x, 0);
                    } else {
                        // Ne devrait pas arriver : ps_v est l'ensemble des
                        // PS-sets des affectations de x.
                        assert(0 && "PS-set de feuille variable inconnu");
                        row_leaf[i] = NULL;
                    }
                }
            } else if (ps_v->size == 1) {
                // Les deux assignations produisent le meme PS-set. Le DAG
                // doit tout de meme compter 2 : on emet (x v ~x).
                DNNFNode* or_node = dnnf_make_or(pool, 2);
                dnnf_or_add_child(or_node, dnnf_make_literal(pool, x, 1));
                dnnf_or_add_child(or_node, dnnf_make_literal(pool, x, 0));
                row_leaf[0] = or_node;
            } else {
                // size == 0 : impossible sur une feuille variable. Defense.
                assert(0 && "|PS'(Fv)| = 0 sur feuille variable");
            }
        }

        for (int i = 0; i < ps_v->size; i++) {
            for (int j = 0; j < ps_v_bar->size; j++) {
                int cell = i * tab->num_cols + j;
                tab->maxsat[cell] = 0;
                tab->sharpsat[cell] = count;
                tab->dnnf[cell] = row_leaf ? row_leaf[i] : NULL;
            }
        }

        if (row_leaf) free(row_leaf);
    }
    else if (leaf->type == NODE_LEAF_CLAUSE) {
        // delta(v) = {clause c}, aucune variable locale
        // L'affectation vide est la seule possible
        // Tab(vide, C') = 1 si c appartient à C' (clause satisfaite par l'extérieur), 0 sinon
        // DAG : singleton TRUE si c in C', FALSE sinon (reutilisation, pas d'allocation).
        int clause_idx = leaf->index;
        int word_idx = clause_idx / 64; // mot de 64 bit qui contient ce bit
        int bit_idx = clause_idx % 64; // index de la clause c dans le mot

        for (int j = 0; j < ps_v_bar->size; j++) {
            int c_in_set = (ps_v_bar->sets[j]->words[word_idx] >> bit_idx) & 1; // extrait bit correspondant à c dans le bitset de C'
            // i = 0 car PS'(Fv) = {vide} (une seule ligne)
            tab->maxsat[j] = c_in_set; // c appartient à C' : 1, 0 sinon
            tab->sharpsat[j] = c_in_set; // pareil (affectation vide valide car satisfaite par l'extérieur)
            tab->dnnf[j] = c_in_set ? dnnf_make_constant_true(pool)
                                    : dnnf_make_constant_false(pool);
        }
    }

    return tab;
}

/**
 * Cas récursif
 * Calcule récursivement la table DP pour un noeud interne (Procédure 3).
 *
 * Implémentation de la Procédure 3 du papier (page 70, Lemme 2) :
 * Pour chaque triplet (C_c1, C_c2, C'_v) appartient à PS'(Fc1) × PS'(Fc2) × PS'(F_v_barre) :
 *   1. Calcule les 3 indices implicites :
 *      - C_v   = (C_c1 OU C_c2) \ delta(v)
 *      - C'_c1 = (C_c2 OU C'_v) ET delta(c1)
 *      - C'_c2 = (C_c1 OU C'_v) ET delta(c2)
 *   2. Met à jour Tab_v :
 *      - MaxSAT : Tab_v(C_v, C'_v) = max(Tab_v, Tab_c1 + Tab_c2)
 *      - #SAT   : Tab_v(C_v, C'_v) += Tab_c1 x Tab_c2
 *      - DAG    : accumule un AND(phi_c1, phi_c2) dans l'OR de la cellule
 *                 (Lemmes 5 et 6).
 *
 * Les reverse maps permettent de retrouver en O(1) les indices locaux
 * à partir des ps_ids calculés par le trie.
 *
 * Complexité par noeud : O(k^3 . m) (Lemme 2).
 *
 * @param node   Noeud courant de l'arbre.
 * @param f      La formule SAT.
 * @param trie   Le trie binaire (pour insert_or_get_ps_set).
 * @param rmaps  Reverse maps réutilisées entre les noeuds.
 * @param pool   Pool proprietaire des DNNFNode emis.
 * @param temp1  Bitset temporaire pré-alloué (évite les malloc dans la boucle).
 * @param temp2  Bitset temporaire pré-alloué.
 * @param temp3  Bitset temporaire pré-alloué.
 * @return       Table DP du noeud, à libérer par l'appelant.
 */
static DPTable* solve_dp_recursive(Node* node, SAT_Formula* f, BinaryTrie* trie, ReverseMaps* rmaps, DNNFPool* pool, Bitset* temp1, Bitset* temp2, Bitset* temp3) {
    // Cas de base : feuille
    if (node->type != NODE_INTERNAL) {
        return compute_leaf_table(node, f, pool);
    }

    Node* c1 = node->left;
    Node* c2 = node->right;

    // Résolution récursive des sous-arbres
    DPTable* tab_c1 = solve_dp_recursive(c1, f, trie, rmaps, pool, temp1, temp2, temp3);
    DPTable* tab_c2 = solve_dp_recursive(c2, f, trie, rmaps, pool, temp1, temp2, temp3);

    // PS-sets du noeud courant et de ses enfants
    PS_Set* ps_v = node->ps_prime_v;
    PS_Set* ps_v_bar = node->ps_prime_v_barre;
    PS_Set* ps_c1 = c1->ps_prime_v;
    PS_Set* ps_c2 = c2->ps_prime_v;
    PS_Set* ps_c1_bar = c1->ps_prime_v_barre;
    PS_Set* ps_c2_bar = c2->ps_prime_v_barre;

    DPTable* tab_v = create_dp_table(ps_v->size, ps_v_bar->size);

    int num_words = node->delta_mask->num_words;
    int num_clauses = f->num_clauses;
    int map_size = rmaps->map_size;

    // Remplir les reverse maps pour les 3 PS_Sets cibles
    fill_reverse_map(rmaps->map_v, map_size, ps_v);
    fill_reverse_map(rmaps->map_c1_bar, map_size, ps_c1_bar);
    fill_reverse_map(rmaps->map_c2_bar, map_size, ps_c2_bar);

    // Boucle principale : triplets (C_c1, C_c2, C'_v)
    // Complexité : O(|PS'(Fc1)| × |PS'(Fc2)| × |PS'(Fv_barre)|) = O(k^3)

    // on itère sur PS'(Fc1) (index i1) et PS'(Fc2) (index i2)
    for (int i1 = 0; i1 < ps_c1->size; i1++) {
        Bitset* bs_c1 = ps_c1->sets[i1];

        for (int i2 = 0; i2 < ps_c2->size; i2++) {
            Bitset* bs_c2 = ps_c2->sets[i2];

            // C_v = (C_c1 OU C_c2) \ delta(v) - indépendant de C'_v (optimisation)
            for (int w = 0; w < num_words; w++) {
                temp3->words[w] = (bs_c1->words[w] | bs_c2->words[w])
                                  & ~(node->delta_mask->words[w]);
            }
            int id_Cv = insert_or_get_ps_set(trie, temp3, num_clauses);
            // Piege : id_Cv est l'identifiant GLOBAL attribue par
            // le trie. Pour indexer tab_v il faut idx_v, l'indice LOCAL dans
            // PS'(Fv). Idem ci-dessous pour idx_c1b, idx_c2b.
            int idx_v = (id_Cv < map_size) ? rmaps->map_v[id_Cv] : -1;
            if (idx_v == -1) continue; // C_v appartient pas à PS'(Fv), combinaison (C_c1, C_c2) invalide

            for (int jv = 0; jv < ps_v_bar->size; jv++) {
                Bitset* bs_v_bar = ps_v_bar->sets[jv];

                // C'_c1 = (C_c2 OU C'_v) ET delta(c1)
                for (int w = 0; w < num_words; w++) {
                    temp1->words[w] = (bs_c2->words[w] | bs_v_bar->words[w])
                                      & c1->delta_mask->words[w];
                }
                int id_c1b = insert_or_get_ps_set(trie, temp1, num_clauses);
                int idx_c1b = (id_c1b < map_size) ? rmaps->map_c1_bar[id_c1b] : -1;
                if (idx_c1b == -1) continue; // C'_c1 appartient pas à PS'(Fc1_barre), combinaison (C_c2, C'_v) invalide

                // C'_c2 = (C_c1 OU C'_v) ET delta(c2)
                for (int w = 0; w < num_words; w++) {
                    temp2->words[w] = (bs_c1->words[w] | bs_v_bar->words[w])
                                      & c2->delta_mask->words[w];
                }
                int id_c2b = insert_or_get_ps_set(trie, temp2, num_clauses);
                int idx_c2b = (id_c2b < map_size) ? rmaps->map_c2_bar[id_c2b] : -1;
                if (idx_c2b == -1) continue; // C'_c2 appartient pas à PS'(Fc2_barre), combinaison (C_c1, C'_v) invalide


                // Indices dans les tables
                int cell_v  = idx_v * tab_v->num_cols  + jv;
                int cell_c1 = i1    * tab_c1->num_cols + idx_c1b;
                int cell_c2 = i2    * tab_c2->num_cols + idx_c2b;

                // MaxSAT
                // t = Tab_c1(C_c1, C'_c1) + Tab_c2(C_c2, C'_c2) (addition car delta(c1) et delta(c2) sont disjoints donc pas de double comptage)
                // Mise à jour seulement si les deux cellules enfants sont valides
                if (tab_c1->maxsat[cell_c1] >= 0 && tab_c2->maxsat[cell_c2] >= 0) {
                    long long t = tab_c1->maxsat[cell_c1] + tab_c2->maxsat[cell_c2];
                    if (tab_v->maxsat[cell_v] < t) {
                        tab_v->maxsat[cell_v] = t;
                    }
                }

                // #SAT
                // Tab_v(C_v, C'_v) += Tab_c1(C_c1, C'_c1) * Tab_c2(C_c2, C'_c2) (multiplie car les affectations de c1 et c2 sont indépendantes)
                tab_v->sharpsat[cell_v] += tab_c1->sharpsat[cell_c1]
                                         * tab_c2->sharpsat[cell_c2];

                // DAG : emission d'un AND(phi_c1, phi_c2)
                // accumule dans le OR de la shape (idx_v, jv). Le OR est
                // alloue paresseusement au premier triplet valide. Une cellule
                // qui reste NULL a la fin correspond a sharpsat == 0, ce qui
                // est conforme (ne jamais allouer d'OR vide).
                // dnnf_make_and simplifie si un enfant est FALSE / TRUE.
                DNNFNode* child1 = tab_c1->dnnf[cell_c1];
                DNNFNode* child2 = tab_c2->dnnf[cell_c2];
                if (child1 && child2) {
                    DNNFNode* and_node = dnnf_make_and(pool, child1, child2);
                    if (and_node != pool->node_false) {
                        if (tab_v->dnnf[cell_v] == NULL) {
                            tab_v->dnnf[cell_v] = dnnf_make_or(pool, 4);
                        }
                        dnnf_or_add_child(tab_v->dnnf[cell_v], and_node);
                    }
                }
            }
        }
    }

    // Nettoyage des reverse maps (remet à -1 les entrées utilisées)
    clear_reverse_map(rmaps->map_v, map_size, ps_v);
    clear_reverse_map(rmaps->map_c1_bar, map_size, ps_c1_bar);
    clear_reverse_map(rmaps->map_c2_bar, map_size, ps_c2_bar);

    // Libérer les tables des enfants (plus nécessaires).
    // Les DNNFNode* referencees survivent via le DNNFPool (cf. dp_table.c).
    free_dp_table(tab_c1);
    free_dp_table(tab_c2);

    return tab_v;
}


/**
 * Point d'entrée de la Procédure 3 : résout #SAT et MaxSAT par DP.
 *
 * Orchestre le calcul bottom-up récursif des tables DP le long de l'arbre
 * de décomposition (T, delta). À la racine r, Tab_r(vide, vide) contient :
 *   - MaxSAT : le nombre maximum de clauses satisfaisables (Théorème 2)
 *   - #SAT : le nombre d'affectations satisfaisant toutes les clauses
 *   - phi_r(vide, vide) : racine du DAG d-DNNF equivalent a F (Phase 1,
 *     Section 3.3 BCMS, Lemme 4).
 *
 * Prérequis : les Procédures 1 et 2 doivent avoir été exécutées pour
 * calculer PS'(Fv) et PS'(F_v_barre) sur chaque noeud.
 *
 * Complexité totale : O(k^3 . m . (m+n)) (Théorème 2).
 *
 * @param root             Racine de l'arbre de décomposition.
 * @param f                La formule SAT.
 * @param all_clauses_mask Masque de toutes les clauses (non utilisé directement ici).
 * @param trie             Le trie binaire.
 * @param pool             Pool proprietaire du DAG d-DNNF (cree par l'appelant).
 * @return                 Structure DPResult avec maxsat_value, sharpsat_count,
 *                         dnnf_root (dans pool), dnnf_num_edges.
 */
DPResult solve_dp(Node* root, SAT_Formula* f, Bitset* all_clauses_mask, BinaryTrie* trie, DNNFPool* pool) {
    DPResult result = {-1, 0, NULL, 0};

    if (!root || !root->ps_prime_v || !root->ps_prime_v_barre) {
        fprintf(stderr, "Erreur : PS-sets non calcules. Executer Proc 1 et 2 d'abord.\n");
        return result;
    }

    // Taille des reverse maps = nombre de PS-sets connus avant le DP.
    // Tout ps_id >= map_size est forcément nouveau (créé par insert_or_get_ps_set pendant le DP) et ne peut pas appartenir à un PS_Set existant.
    int map_size = trie->num_ps_sets;
    if (map_size <= 0) map_size = 1;
    ReverseMaps* rmaps = create_reverse_maps(map_size);

    // Bitsets temporaires réutilisés dans la boucle interne (évite les malloc répétés)
    Bitset* temp1 = create_bitset(f->num_clauses);
    Bitset* temp2 = create_bitset(f->num_clauses);
    Bitset* temp3 = create_bitset(f->num_clauses);

    // Calcul bottom-up récursif
    DPTable* tab_root = solve_dp_recursive(root, f, trie, rmaps, pool, temp1, temp2, temp3);

    // À la racine : PS'(Fv) = {vide}, PS'(F_v_barre) = {vide}
    // Tab_r(vide, vide) donne directement les résultats
    if (tab_root && tab_root->num_rows > 0 && tab_root->num_cols > 0) {
        result.maxsat_value = tab_root->maxsat[0];
        result.sharpsat_count = tab_root->sharpsat[0];
        result.dnnf_root = tab_root->dnnf[0];
        result.dnnf_num_edges = dnnf_size(result.dnnf_root, pool);
    }

    // Nettoyage. Les DNNFNode* survivent via le DNNFPool (cf. dp_table.c).
    free_dp_table(tab_root);
    free_reverse_maps(rmaps);
    free_bitset(temp1);
    free_bitset(temp2);
    free_bitset(temp3);

    (void)all_clauses_mask; // déjà encodé dans les PS-sets calculés par Proc 1/2

    return result;
}
