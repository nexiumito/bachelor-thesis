#include "parser.h"
#include "../utils/bitset.h"

#include <stdio.h>  
#include <stdlib.h> 
#include <string.h>
#include <stdbool.h>

SAT_Formula* parse_cnf(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Erreur d'ouverture du fichier DIMACS");
        return NULL;
    }

    SAT_Formula* formula = malloc(sizeof(SAT_Formula));
    char line[1024];
    int current_clause_idx = 0;
    bool header_found = false;

    while (fgets(line, sizeof(line), file)) {
        // Ignorer les commentaires et lignes vides
        if (line[0] == 'c' || line[0] == '\n' || line[0] == '\r') continue;

        if (line[0] == 'p') {
            sscanf(line, "p cnf %d %d", &formula->num_vars, &formula->num_clauses);
            
            // Allocation en respectant ton indexation (1 à num_vars)
            formula->mask_pos = malloc((formula->num_vars + 1) * sizeof(Bitset*));
            formula->mask_neg = malloc((formula->num_vars + 1) * sizeof(Bitset*));
            
            // Utilisation de TON utilitaire existant
            for (int i = 0; i <= formula->num_vars; i++) {
                formula->mask_pos[i] = create_bitset(formula->num_clauses);
                formula->mask_neg[i] = create_bitset(formula->num_clauses);
            }
            header_found = true;
            continue;
        }

        if (header_found) {
            char* token = strtok(line, " \t\n\r");
            while (token != NULL) {
                int lit = atoi(token);
                
                if (lit == 0) {
                    current_clause_idx++;
                    if (current_clause_idx >= formula->num_clauses) break;
                } else {
                    // DIMACS utilise l'indexation de 1 à N, tout comme ton code
                    int var_index = abs(lit); 
                    
                    if (lit > 0) {
                        set_bit(formula->mask_pos[var_index], current_clause_idx);
                    } else {
                        set_bit(formula->mask_neg[var_index], current_clause_idx);
                    }
                }
                token = strtok(NULL, " \t\n\r");
            }
        }
    }

    fclose(file);
    return formula;
}

void free_formula(SAT_Formula* f) {
    if (f) {
        // Libération en utilisant TON utilitaire existant
        for (int i = 0; i <= f->num_vars; i++) {
            free_bitset(f->mask_pos[i]);
            free_bitset(f->mask_neg[i]);
        }
        free(f->mask_pos);
        free(f->mask_neg);
        free(f);
    }
}