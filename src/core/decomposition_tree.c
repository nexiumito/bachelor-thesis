#include "decomposition_tree.h"
#include "../utils/ps_set.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>

static int next_node_id = 1;

/**
 * Crée un noeud feuille de l'arbre de décomposition (branch decomposition).
 *
 * Chaque feuille correspond soit à une variable soit à une clause de la
 * formule F, conformément à la bijection delta entre les feuilles de T et
 * les éléments de F (Section 2.3 du papier).
 *
 * Le delta_mask encode l'ensemble delta(v) : pour une feuille clause, le bit
 * correspondant est activé ; pour une feuille variable, le masque reste vide
 * (les variables ne sont pas des clauses).
 *
 * @param type       Type du noeud : NODE_LEAF_VAR ou NODE_LEAF_CLAUSE.
 * @param index      Indice de la variable (1..n) ou de la clause (0..m-1).
 * @param num_clauses Nombre total de clauses dans la formule (pour dimensionner le bitset).
 * @return           Pointeur vers le noeud feuille alloué.
 */
Node* create_leaf_node(NodeType type, int index, int num_clauses) {
    Node* n = malloc(sizeof(Node));
    n->id = next_node_id++;
    n->type = type;
    n->index = index;
    n->left = NULL;
    n->right = NULL;

    n->ps_prime_v = NULL;
    n->ps_prime_v_barre = NULL;

    n->delta_mask = create_bitset(num_clauses);
    
    // si c'est une clause, on active le bit correspondant dans son delta_mask
    if (type == NODE_LEAF_CLAUSE) {
        int word_idx = index / 64;
        int bit_idx = index % 64;
        n->delta_mask->words[word_idx] |= (1ULL << bit_idx); // fonctionne très bien avec un simple "=", utilisation OU logique est une bonne pratique...
    }
    return n;
}

/**
 * Crée un noeud interne de l'arbre de décomposition.
 *
 * Un noeud interne v a deux enfants c1 et c2 dans l'arbre binaire T.
 * Son delta_mask représente delta(v) = delta(c1) ∪ delta(c2), c'est-à-dire l'ensemble
 * des clauses et variables présentes dans le sous-arbre enraciné en v
 * (Section 2.3 : "delta(v) = {delta(l) : l est une feuille du sous-arbre enraciné en v}").
 *
 * @param left        Enfant gauche (c1).
 * @param right       Enfant droit (c2).
 * @param num_clauses Nombre total de clauses dans la formule.
 * @return            Pointeur vers le noeud interne alloué.
 */
Node* create_internal_node(Node* left, Node* right, int num_clauses) {
    Node* n = malloc(sizeof(Node));
    n->id = next_node_id++;
    n->type = NODE_INTERNAL;
    n->index = 0;
    n->left = left;
    n->right = right;

    n->ps_prime_v = NULL;
    n->ps_prime_v_barre = NULL;

    n->delta_mask = create_bitset(num_clauses);
    
    // le masque d'un parent est l'union des masques de ses enfants
    for (int i = 0; i < n->delta_mask->num_words; i++) {
        n->delta_mask->words[i] = left->delta_mask->words[i] | right->delta_mask->words[i];
    }
    return n;
}

/**
 * Génère un arbre de décomposition binaire aléatoire pour la formule F.
 *
 * Construit une branch decomposition (T, delta) en regroupant aléatoirement
 * les feuilles bottom-up : à chaque étape, deux noeuds sont choisis au
 * hasard dans le pool et fusionnés sous un nouveau noeud interne, jusqu'à
 * obtenir un unique noeud racine.
 *
 * Cette méthode sert de baseline pour comparer avec les heuristiques plus
 * sophistiquées (arbre linéaire, GreedyOrder).
 *
 * @param f  Pointeur vers la formule SAT (fournit num_vars et num_clauses).
 * @return   Pointeur vers la racine de l'arbre de décomposition généré.
 */
Node* generate_random_tree(SAT_Formula* f) {
    int total_leaves = f->num_vars + f->num_clauses;
    Node** pool = malloc(total_leaves * sizeof(Node*)); // tableau contenant des pointeurs vers tout les noeuds qui n'ont pas de parent
    
    // initialisation des feuilles
    int pool_size = 0;
    for (int i = 1; i <= f->num_vars; i++) {
        pool[pool_size++] = create_leaf_node(NODE_LEAF_VAR, i, f->num_clauses);
    }
    for (int i = 0; i < f->num_clauses; i++) {
        pool[pool_size++] = create_leaf_node(NODE_LEAF_CLAUSE, i, f->num_clauses);
    }
    
    // construction bottom-up aléatoire
    srand(time(NULL)); // génération seed aléatoire
    
    while (pool_size > 1) {
        // sélection de deux indices distincts au hasard
        int idx1 = rand() % pool_size; // nouvelle seed calculé à partir de l'ancienne à chaque fois
        int idx2;
        do {
            idx2 = rand() % pool_size;
        } while (idx1 == idx2);
        
        // création du noeud parent
        Node* parent = create_internal_node(pool[idx1], pool[idx2], f->num_clauses);
        
        // màj : on remplace idx1 par le parent, et on bouche le trou laissé par idx2 avec le dernier élément
        pool[idx1] = parent;
        pool[idx2] = pool[pool_size - 1];
        pool_size--;
    }
    
    Node* root = pool[0];
    free(pool);
    return root;
}


/**
 * Génère un arbre de décomposition linéaire (linear branch decomposition).
 *
 * Dans une décomposition linéaire, les noeuds internes induisent un chemin
 * (Section 2.3 du papier). L'ordre utilisé ici est basé sur les variables :
 * les variables sont ajoutées dans l'ordre 1..n, et chaque clause est greffée
 * juste après sa variable de plus grand indice (max_var).
 *
 * Cette structure permet de réduire la complexité du DP d'un facteur k
 * (Théorème 3 : O(k^2·m·(m+n)) au lieu de O(k^3·m·(m+n))), car l'un des
 * deux enfants de chaque noeud interne est toujours une feuille.
 *
 * @param f  Pointeur vers la formule SAT.
 * @return   Pointeur vers la racine de l'arbre linéaire généré.
 */
Node* generate_linear_tree(SAT_Formula* f) {
    // trouver la variable maximale de chaque clause
    int* max_var_of_clause = calloc(f->num_clauses, sizeof(int));
    
    // on teste pour chaque variable si elle apparaît dans la clause c et on retient la plus grande
    for (int c = 0; c < f->num_clauses; c++) {
        int max_v = 1;
        // cherche la variable la plus grande présente dans la clause 'c'
        for (int v = 1; v <= f->num_vars; v++) {
            int word_idx = c / 64;
            int bit_idx = c % 64;
            // est-ce que la variable v est dans la clause c (en positif ou négatif) ?
            bool contains = ((f->mask_pos[v]->words[word_idx] >> bit_idx) & 1ULL) ||
                            ((f->mask_neg[v]->words[word_idx] >> bit_idx) & 1ULL);
            
            if (contains && v > max_v) {
                max_v = v;
            }
        }
        max_var_of_clause[c] = max_v;
    }

    Node* current_root = NULL;

    // construction : on crée une feuille variable qu'on greffe à l'arbre, et on greffe ensuite toutes les clauses dont max_var = v
    for (int v = 1; v <= f->num_vars; v++) {
        //  ajoute la variable v
        Node* leaf_v = create_leaf_node(NODE_LEAF_VAR, v, f->num_clauses);
        if (!current_root) {
            current_root = leaf_v;
        } else {
            current_root = create_internal_node(current_root, leaf_v, f->num_clauses);
        }
        
        // ajoute toutes les clauses qui se terminent à cette variable v
        for (int c = 0; c < f->num_clauses; c++) {
            if (max_var_of_clause[c] == v) {
                Node* leaf_c = create_leaf_node(NODE_LEAF_CLAUSE, c, f->num_clauses);
                current_root = create_internal_node(current_root, leaf_c, f->num_clauses);
            }
        }
    }

    free(max_var_of_clause);
    return current_root;
}

/**
 * Génère un arbre de décomposition linéaire via l'heuristique GreedyOrder.
 *
 * Implémentation de l'algorithme GreedyOrder décrit en Section 6 (page 76)
 * du papier de Saether, Telle & Vatshelle (2015). L'heuristique construit
 * un ordre linéaire sigma sur les sommets du graphe d'incidence I(F) :
 *
 *   - À chaque étape, le sommet v appartenant à R ayant le plus grand Ldegree
 *     (nombre de voisins déjà choisis dans L) est sélectionné.
 *   - En cas d'égalité, le sommet de plus petit degré total est préféré.
 *   - Les feuilles sont greffées dans cet ordre pour former une
 *     décomposition linéaire (T, delta).
 *
 * L'objectif est de minimiser la ps-width de la décomposition résultante,
 * permettant ensuite un DP efficace via le Théorème 3.
 *
 * @param f  Pointeur vers la formule SAT.
 * @return   Pointeur vers la racine de l'arbre linéaire généré.
 */
Node* generate_greedy_linear_tree(SAT_Formula* f) {
    int n = f->num_vars;      // variables : indices 1..n
    int m = f->num_clauses;   // clauses   : indices 0..m-1
    int total = n + m;

    // Sommets du graphe d'incidence : 0..n-1 = variables (var v -> index v-1)
    //                                 n..n+m-1 = clauses  (clause c -> index n+c)

    // Calculer le degre total de chaque sommet dans I(F)
    int* degree = calloc(total, sizeof(int));
    int* ldegree = calloc(total, sizeof(int));  // nb de voisins deja choisis
    bool* chosen = calloc(total, sizeof(bool));

    // Degre des variables : nb de clauses contenant la variable
    for (int v = 1; v <= n; v++) {
        int deg = 0;
        for (int c = 0; c < m; c++) {
            int word_idx = c / 64;
            int bit_idx = c % 64;
            bool contains = ((f->mask_pos[v]->words[word_idx] >> bit_idx) & 1ULL) ||
                            ((f->mask_neg[v]->words[word_idx] >> bit_idx) & 1ULL);
            if (contains) deg++;
        }
        degree[v - 1] = deg;
    }

    // Degre des clauses : nb de variables dans la clause
    for (int c = 0; c < m; c++) {
        int deg = 0;
        for (int v = 1; v <= n; v++) {
            int word_idx = c / 64;
            int bit_idx = c % 64;
            bool contains = ((f->mask_pos[v]->words[word_idx] >> bit_idx) & 1ULL) ||
                            ((f->mask_neg[v]->words[word_idx] >> bit_idx) & 1ULL);
            if (contains) deg++;
        }
        degree[n + c] = deg;
    }

    // Construire l'arbre lineaire en suivant l'ordre GreedyOrder
    Node* current_root = NULL;
    int num_chosen = 0;

    while (num_chosen < total) {
        // Choisir le sommet avec max ldegree, tie-break par min degree
        int best = -1;
        int best_ldeg = -1;
        int best_deg = total + 1;

        for (int i = 0; i < total; i++) {
            if (chosen[i]) continue;
            if (ldegree[i] > best_ldeg ||
                (ldegree[i] == best_ldeg && degree[i] < best_deg)) {
                best = i;
                best_ldeg = ldegree[i];
                best_deg = degree[i];
            }
        }

        // Marquer comme choisi
        chosen[best] = true;
        num_chosen++;

        // Creer la feuille correspondante
        Node* leaf;
        if (best < n) {
            // C'est une variable (best = v-1, donc variable = best+1)
            leaf = create_leaf_node(NODE_LEAF_VAR, best + 1, m);
        } else {
            // C'est une clause (best = n+c, donc clause = best-n)
            leaf = create_leaf_node(NODE_LEAF_CLAUSE, best - n, m);
        }

        if (!current_root) {
            current_root = leaf;
        } else {
            current_root = create_internal_node(current_root, leaf, m);
        }

        // Mettre a jour ldegree des voisins non encore choisis
        if (best < n) {
            // best est la variable (best+1) : ses voisins sont les clauses la contenant
            int v = best + 1;
            for (int c = 0; c < m; c++) {
                if (chosen[n + c]) continue;
                int word_idx = c / 64;
                int bit_idx = c % 64;
                bool contains = ((f->mask_pos[v]->words[word_idx] >> bit_idx) & 1ULL) ||
                                ((f->mask_neg[v]->words[word_idx] >> bit_idx) & 1ULL);
                if (contains) ldegree[n + c]++;
            }
        } else {
            // best est la clause (best-n) : ses voisins sont les variables qu'elle contient
            int c = best - n;
            for (int v = 1; v <= n; v++) {
                if (chosen[v - 1]) continue;
                int word_idx = c / 64;
                int bit_idx = c % 64;
                bool contains = ((f->mask_pos[v]->words[word_idx] >> bit_idx) & 1ULL) ||
                                ((f->mask_neg[v]->words[word_idx] >> bit_idx) & 1ULL);
                if (contains) ldegree[v - 1]++;
            }
        }
    }

    free(degree);
    free(ldegree);
    free(chosen);
    return current_root;
}


/**
 * Libère récursivement toute la mémoire de l'arbre de décomposition.
 *
 * Parcourt l'arbre en post-ordre et libère pour chaque noeud : le
 * delta_mask (bitset), les ensembles PS'(Fv) et PS'(F_v_barre) s'ils existent, puis le noeud lui-même.
 *
 * @param root  Racine du sous-arbre à libérer (peut être NULL).
 */
void free_tree(Node* root) {
    if (!root) return;
    free_tree(root->left);
    free_tree(root->right);
    free_bitset(root->delta_mask);

    if (root->ps_prime_v) free_ps_set(root->ps_prime_v);
    if (root->ps_prime_v_barre) free_ps_set(root->ps_prime_v_barre);

    free(root);
}


/**
 * Calcule la ps-width de la décomposition basée sur PS'(Fv).
 *
 * Parcourt récursivement l'arbre et retourne le maximum de |PS'(Fv)|
 * sur tous les noeuds v de T. Correspond à une composante de la
 * définition : psw(T, delta) = max{ps(delta(v)) : v est un noeud de T}
 * où ps(delta(v)) = max{|PS(Fv)|, |PS(F_v_barre)|} (Section 2.3).
 *
 * @param node  Racine du sous-arbre à analyser.
 * @return      Taille maximale de PS'(Fv) dans le sous-arbre.
 */
int calculate_tree_ps_width(Node* node) {
    if (!node || !node->ps_prime_v) return 0;
    
    int max_width = node->ps_prime_v->size;
    int left_width = calculate_tree_ps_width(node->left);
    int right_width = calculate_tree_ps_width(node->right);
    
    if (left_width > max_width) max_width = left_width;
    if (right_width > max_width) max_width = right_width;
    
    return max_width;
}

/**
 * Calcule la ps-width de la décomposition basée sur PS'(F_v_barre).
 *
 * Parcourt récursivement l'arbre et retourne le maximum de |PS'(F_v_barre)|
 * sur tous les noeuds v de T. C'est la seconde composante de
 * ps(delta(v)) = max{|PS(Fv)|, |PS(F_v_barre)|}.
 *
 * @param node  Racine du sous-arbre à analyser.
 * @return      Taille maximale de PS'(F_v_barre) dans le sous-arbre.
 */
int calculate_tree_ps_prime_barre_width(Node* node) {
    if (!node || !node->ps_prime_v_barre) return 0;
    
    int max_width = node->ps_prime_v_barre->size;
    int left_width = calculate_tree_ps_prime_barre_width(node->left);
    int right_width = calculate_tree_ps_prime_barre_width(node->right);
    
    if (left_width > max_width) max_width = left_width;
    if (right_width > max_width) max_width = right_width;
    
    return max_width;
}
/**
 * Calcule la ps-width globale de la décomposition (T, delta).
 *
 * Retourne max{max_v |PS'(Fv)|, max_v |PS'(F_v_barre)|}, ce qui correspond
 * à la définition de psw(T, delta) = max{ps(delta(v)) : v noeud de T}
 * avec ps(delta(v)) = max{|PS(Fv)|, |PS(F_v_barre)|} (Section 2.3).
 *
 * @param node  Racine de l'arbre de décomposition.
 * @return      La ps-width de la décomposition.
 */
int calculate_tree_max_ps_width(Node* node) {
    int ps_width = calculate_tree_ps_width(node);
    int ps_prime_barre_width = calculate_tree_ps_prime_barre_width(node);
    return (ps_width > ps_prime_barre_width) ? ps_width : ps_prime_barre_width;
}