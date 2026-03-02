#include <stdint.h>
#include <stdio.h>

#define MAX_VARS 100 // à ajuster ???

// Structure globale pour stocker les informations de la formule F
typedef struct {
    int num_vars;
    int num_clauses;
    
    // On veut représenter un ensemble de clauses par un seul nombre entier uint64_t (64 bits)
    // Le bit à la position i correspond à la clause d'indice i
    // Si le bit vaut 1, la clause est dans l'ensemble, sinon non

    // masques pré-calculés (indice 0 pas utilisé car les variables vont de 1 à n)
    uint64_t mask_pos[MAX_VARS + 1]; // liste de toutes les clauses qui sont satisfaites si x = vrai (clause où x apparait sous forme de littéral positif)
    uint64_t mask_neg[MAX_VARS + 1]; // liste de toutes les clauses qui sont satisfaites si x = faux (clause où x apparait sous forme de littéral negatif)
} SAT_Formula;

// parser pas encore implémenté, fonction temporaire pour avoir un exemple à tester
SAT_Formula create_toy_formula() {
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