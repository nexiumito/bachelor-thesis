#include <stdio.h>
#include <stdlib.h>

#include "procedure1.h"


/*
 GESTION DES DOUBLONS DANS PS'(F_v)
 Le but de cette fonction est d'ajouter un masque représentant un ensemble de clauses satisfaites, tout en garantissant l'unicité (élimination des doublons)
 La taille maximale de cet ensemble correspond à la ps-width (notée k).

3 approches possibles et leurs compromis :

 * 1. TABLEAU DYNAMIQUE (l'implémentation actuelle) : parcourt tout le tableau (boucle for) avant chaque ajout
 - Avantages : 
   * rapide pour des petits ensembles (k faible)
   * bon cache locality pour le CPU (données contiguës)
   * comparaison de 64 clauses en 1 seule instruction d'horloge (==).
 - Inconvénients :
   * Complexité temporelle en O(k) par insertion.
   * Si k devient très grand (ex: 10 000), le temps d'insertion explose (goulot d'étranglement en O(k^2) au global)

 * 2. TRIE BINAIRE (recommandation théorique du papier, p. 68) : arbre de profondeur m, où chaque niveau teste le bit d'une clause (0=gauche, 1=droite)
 - Avantages :
   * complexité temporelle en O(m) stricte par insertion
   * temps d'insertion totalement indépendant de k (taille de PS')
   * aligné avec la théorie du papier
 - Inconvénients (langage C...) :
   * demande bcp d'allocations mémoire dynamiques (malloc par noeud)
   * cache misses constants
   * plus lent ?

 * 3. TABLE DE HACHAGE / HASH SET : calcule un hash du masque de bits et on le place dans un tableau indexé par ce hash
 - Avantages :
   * complexité moyenne en O(1)
   * combine la performance matérielle du tableau (bonne localité comme le tableau dynamique) et l'indépendance par rapport à k (comme le trie binaire)
 - Inconvénients :
  * Nécessite de coder une bonne fonction de hachage et de gérer les collisions (?)

 L'implémentation actuelle agit comme une preuve de concept (PoC) optimisée au niveau matériel pour de petites instances. 
 Pour des formules réelles massives générant une grande ps-width (et dépassant la limite d'un seul uint64_t), transition vers une table de hachage devrait être la solution la plus robuste pour concilier la théorie du papier et les performances du langage C

*/


// Fonction pour ajouter un masque sans doublon (remplace le Trie binaire (table de hachage ?) pour le moment)
void add_to_ps_set(PS_Set* set, uint64_t mask) {
    // vérification des doublons en O(|cla(F)|) simulé ici par O(k) oû k est le nombre d'élément deja trouvé
    for (int i = 0; i < set->size; i++) {
        if (set->masks[i] == mask) return; // doublon ignoré
    }
    
    // ajout dynamique
    if (set->size == set->capacity) {
        set->capacity *= 2;
        set->masks = realloc(set->masks, set->capacity * sizeof(uint64_t));
    }
    set->masks[set->size++] = mask;
}

// Fonction cas de base pour les feuilles
PS_Set* compute_leaf_ps_prime(Node* leaf, SAT_Formula* f, uint64_t all_clauses_mask) {
    // initialisation de l'ensemble
    PS_Set* ps_v = malloc(sizeof(PS_Set));
    ps_v->capacity = 2; // maximum 2 affectations (Vrai ou Faux)
    ps_v->size = 0;
    ps_v->masks = malloc(ps_v->capacity * sizeof(uint64_t));

    // identifier la variable de cette feuille
    int x = leaf->var_index;

    // masque de filtrage : cla(F_v) = cla(F) \ delta(v)
    uint64_t mask_Fv = all_clauses_mask & ~(leaf->delta_clauses_mask);

    // Cas 1 : si on affecte x = Vrai
    // On prend les clauses satisfaites par x=Vrai et on applique le filtre
    uint64_t C_true = f->mask_pos[x] & mask_Fv;
    add_to_ps_set(ps_v, C_true);

    // Cas 2 : Si on affecte x = Faux 
    // On prend les clauses satisfaites par x=Faux et on applique le filtre
    uint64_t C_false = f->mask_neg[x] & mask_Fv;
    add_to_ps_set(ps_v, C_false);

    return ps_v;
}


// PS'(Fv) = (C1 U C2) ET cla(Fv)
PS_Set* compute_ps_prime_bottom_up(Node* node, SAT_Formula* f, uint64_t all_clauses_mask){
    //condition d'arrêt de la récursivité
    if (node->is_leaf) {
        node->ps_prime_v = compute_leaf_ps_prime(node, f, all_clauses_mask);
        return node->ps_prime_v;
    }

    // appels récursifs bottom-up
    PS_Set* ps_c1 = compute_ps_prime_bottom_up(node->left, f, all_clauses_mask);
    PS_Set* ps_c2 = compute_ps_prime_bottom_up(node->right, f, all_clauses_mask);

    // initialisation ensemble vide (L dans le papier)
    PS_Set* ps_v = malloc(sizeof(PS_Set));
    ps_v->capacity = 16;
    ps_v->size = 0;
    ps_v->masks = malloc(ps_v->capacity * sizeof(uint64_t));

    // masque de filtrage cla(F_v) = cla(F) \ delta(v)
    uint64_t mask_Fv = all_clauses_mask & ~(node->delta_clauses_mask);

    // produit cartésien et filtrage
    for (int i = 0; i < ps_c1->size; i++) {
        for (int j = 0; j < ps_c2->size; j++) {
            
            uint64_t C1 = ps_c1->masks[i];
            uint64_t C2 = ps_c2->masks[j];
            
            uint64_t C_union = C1 | C2;
            uint64_t C_filtered = C_union & mask_Fv;
            
            // ajout avec vérif des doublons
            add_to_ps_set(ps_v, C_filtered);
        }
    }

    node->ps_prime_v = ps_v;
    return ps_v;
}