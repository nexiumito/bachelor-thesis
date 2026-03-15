#ifndef PARSER_H
#define PARSER_H

#include "formula.h"

SAT_Formula* parse_cnf(const char* filename);
void free_formula(SAT_Formula* f);

#endif