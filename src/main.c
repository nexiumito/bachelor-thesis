#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "procedure1.h"

#define MAX_VARS 100 // à ajuster ???

// parser pas encore implémenté, fonction temporaire pour avoir un exemple à tester
SAT_Formula create_exemple_formula() {
    SAT_Formula f = {0}; // initialise tout à 0
    f.num_vars = 5;
    f.num_clauses = 4;

    // 1ULL : 1 au format unsigned long long (64 bits --> 000...0001)
    // << i : shift qui décale ce 1 de i positions vers la gauche
    // |= (ou binaire) pour ajouter une clause à notre ensemble sans effacer les précédentes

    // Clause C0 : {x1, x2}
    f.mask_pos[1] |= (1ULL << 0); // ajoute C0 au masque positif de x1
    f.mask_pos[2] |= (1ULL << 0); // ajoute C0 au masque positif de x2

    // Clause C1 : {x1, ¬x2, x3}
    f.mask_pos[1] |= (1ULL << 1); // ajoute C1 au masque positif de x1
    f.mask_neg[2] |= (1ULL << 1); // ajoute C1 au masque negatif de x2
    f.mask_pos[3] |= (1ULL << 1); // ajoute C1 au masque positif de x3

    // Clause C2 : {x2, ¬x4, x5}
    f.mask_pos[2] |= (1ULL << 2); // ajoute C2 au masque positif de x2
    f.mask_neg[4] |= (1ULL << 2); // ajoute C2 au masque negatif de x4
    f.mask_pos[5] |= (1ULL << 2); // ajoute C2 au masque positif de x5

    // Clause C3 : {x2, x3, x5}
    f.mask_pos[2] |= (1ULL << 3); // ajoute C3 au masque positif de x2
    f.mask_pos[3] |= (1ULL << 3); // ajoute C3 au masque positif de x3
    f.mask_pos[5] |= (1ULL << 3); // ajoute C3 au masque positif de x5

    return f;
}

// Créer une feuille (qui représente ici une variable)
Node* create_leaf(int var_index, uint64_t delta_mask) {
    Node* n = malloc(sizeof(Node));
    n->is_leaf = true;
    n->var_index = var_index;
    n->delta_clauses_mask = delta_mask;
    n->left = NULL;
    n->right = NULL;
    n->ps_prime_v = NULL;
    return n;
}

// Créer un noeud interne (qui unit deux sous-arbres)
Node* create_internal_node(Node* left, Node* right, uint64_t delta_mask) {
    Node* n = malloc(sizeof(Node));
    n->is_leaf = false;
    n->var_index = 0; // Un noeud interne ne représente pas de variable
    n->delta_clauses_mask = delta_mask;
    n->left = left;
    n->right = right;
    n->ps_prime_v = NULL;
    return n;
}

// Fonction pour afficher un entier 64 bits en binaire (très utile pour débugger !)
void print_binary(uint64_t n, int num_clauses) {
    for (int i = num_clauses - 1; i >= 0; i--) {
        printf("%llu", (n >> i) & 1ULL);
    }
}

// --- LE MAIN ---

int main() {
    // 1. Initialiser la formule
    SAT_Formula f = create_exemple_formula();

    // 2. Initialiser all_clauses_mask
    // On veut un masque avec des 1 pour toutes les clauses de la formule.
    // Pour 4 clauses, on veut '0000...1111' en binaire, ce qui vaut 15 en décimal.
    // Astuce C : (1ULL << 4) donne 16 ('10000'). Si on fait -1, ça donne 15 ('01111').
    uint64_t all_clauses_mask = (1ULL << f.num_clauses) - 1;

    // 3. Construire un petit arbre de test manuellement
    // Enfant gauche : la variable x1. Au début, aucune clause n'est dans sa coupe.
    Node* leaf_x1 = create_leaf(1, 0ULL); 
    
    // Enfant droit : la variable x2. Au début, aucune clause n'est dans sa coupe.
    Node* leaf_x2 = create_leaf(2, 0ULL); 

    // Noeud parent : Il unit x1 et x2.
    // Pour tester le "filtre" de la procédure 1, on va dire que ce noeud "englobe" 
    // entièrement la Clause C0 (index 0). Donc la clause 0 est DANS delta(v).
    // Son masque de delta_clauses devient donc (1ULL << 0), soit le bit 0 à 1.
    uint64_t parent_delta_mask = (1ULL << 0);
    Node* parent = create_internal_node(leaf_x1, leaf_x2, parent_delta_mask);

    // 4. Lancer la Procédure 1 (Bottom-Up)
    printf("Lancement de compute_ps_prime_bottom_up...\n");
    PS_Set* result = compute_ps_prime_bottom_up(parent, &f, all_clauses_mask);

    // 5. Afficher les résultats
    printf("\n=== RESULTATS AU NOEUD PARENT ===\n");
    printf("Taille de PS'(F_v) : %d\n", result->size);
    printf("Masques retenus (Bits: C3 C2 C1 C0) :\n");
    for (int i = 0; i < result->size; i++) {
        printf("  - Ensemble %d : ", i);
        print_binary(result->masks[i], f.num_clauses);
        printf("\n");
    }

    return 0;
}