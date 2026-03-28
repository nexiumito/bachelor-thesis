@staticmethod
def simplifier_one(cnf_list: list[set]) -> list[set]:
    """Supprime les clauses subsumées par d'autres dans la liste CNF."""
    return [clause for clause in cnf_list if not any(other < clause for other in cnf_list)]

@staticmethod
def simplifier_two(cnf_list: list[set[str]]) -> list[set[str]]:
    
    def resolve_if_possible(A: set[str], B: set[str]) -> set[str] | None:
        # On veut que A soit la plus petite (le parent subsumé)
        if len(A) > len(B):
            return None

        for lit in B:
            neg_lit = lit[1:]
            if neg_lit in A:
                # test si A sans le pivot est inclus dans B
                if A - {neg_lit} <= B:
                    return B - {lit}
        return None


    for i in range(len(cnf_list) - 1):
        for j in range(i + 1, len(cnf_list)):
            A = cnf_list[i]
            B = cnf_list[j]

            resolved = resolve_if_possible(A, B)
            if resolved:
                cnf_list[j] = resolved
            else:
                resolved = resolve_if_possible(B, A)
                if resolved:
                    cnf_list[i] = resolved
    return cnf_list

@staticmethod  
def simplifier_three(cnf_list: list[set]) -> list[set]:
    for i in range(len(cnf_list) - 1):
        for j in range(i + 1, len(cnf_list)):
            if len(cnf_list[i]) == 2 and len(cnf_list[j]) == 2:
                (liti1, liti2) = cnf_list[i]
                if (liti1[1:] in cnf_list[j] and "-" + liti2 in cnf_list[j]) or ("-" + liti1 in cnf_list[j] and liti2[1:] in cnf_list[j]):
                    pure_liti1 = liti1[1:] if liti1.startswith("-") else liti1
                    pure_liti2 = liti2[1:] if liti2.startswith("-") else liti2
                    new_cnf = []
                    for clause in cnf_list:
                        if pure_liti2 in clause or "-" + pure_liti2 in clause:
                            if clause != cnf_list[i] and clause != cnf_list[j]:
                                new_clause = set()
                                for lit in clause:
                                    if lit == pure_liti2:
                                        new_clause.add(pure_liti1)
                                    elif lit == "-" + pure_liti2:
                                        new_clause.add("-" + pure_liti1)
                                    else:
                                        new_clause.add(lit)
                                clause = new_clause
                        new_cnf.append(clause)
                    cnf_list = new_cnf         
    return cnf_list


@staticmethod
def simplification_unit_clause(clauses):
    modified = True

    while modified:
        modified = False
        
        unit_clauses = [list(c)[0] for c in clauses if len(c) == 1] # récupérer chaque littéral au sain des clauses de longueur 1
        
        for var in unit_clauses:
            neg = "-" + var if not var.startswith("-") else var[1:]

            new_clauses = []
            for clause in clauses:
                if var in clause: # Clause satisfaite, on la retire
                    modified = True
                    continue
                if neg in clause: # On supprime la variable négative de la clause
                    new_clause = clause.copy()
                    new_clause.discard(neg)
                    new_clauses.append(new_clause)
                    modified = True
                else:
                    new_clauses.append(clause)

            clauses = new_clauses

    return clauses


@staticmethod
def normalize(lines):
    return set(frozenset(s) for s in lines)   


@staticmethod
def simplify_cnf(dimacs_file: str, partial_resolution: bool=False):
    """Applique toutes les simplifications sur une CNF en format dimacs"""
    with open(dimacs_file, "r") as f:
        lines = f.readlines()
        
    lines_set = []
    for line in lines:
        if not line.startswith('p cnf'):
            lits = line.strip().split()
            lits.remove("0")
            lines_set.append(set(lits))
    
    previous = None
    while True:
        if partial_resolution:
            lines_set = simplification_unit_clause(lines_set)
        print(lines_set)
        lines_set = simplifier_one(lines_set)
        print(lines_set)
        lines_set = simplifier_two(lines_set)
        print(lines_set)
        lines_set = simplifier_three(lines_set)
        print(lines_set)

        normalized = normalize(lines_set) # évite de faire des deepcopy pour comparer les s1, s2, s3
        if normalized == previous: # ça veut dire qu'il y a pas eu de simplification 
            break
        previous = normalized

    num_vars = len(set(var.lstrip('-') for clause in lines_set for var in clause))
    num_clauses = len(lines_set)
    
    try:
        with open(dimacs_file, 'x') as f:
            f.write(f"p cnf {num_vars} {num_clauses}\n")
            for clause in lines_set:
                line = " ".join(map(str, clause)) + " 0\n"
                f.write(line)
    except FileExistsError:
        with open(dimacs_file, 'w') as f:
            f.write(f"p cnf {num_vars} {num_clauses}\n")
            for clause in lines_set:
                line = " ".join(map(str, clause)) + " 0\n"
                f.write(line)



@staticmethod
def parse_formula(cnf_str: str, output_file: str):
    var_map = {}
    var_counter = 1
    clauses = []
    cnf_str = cnf_str.replace(" ", "") 
    cnf_str = cnf_str.split("*") 
    for clause in cnf_str:
        literals = re.findall(r"¬?\w+", clause) 
        clauses.append(list(literals))
    clauses_list = []
    for literals in clauses:
        clause = []
        for lit in literals:
            is_neg = False
            if lit.startswith("¬"): # gérer la négation
                is_neg = True
                lit = lit.strip("¬")
            if lit not in var_map: 
                var_map[lit] = var_counter
                var_counter += 1
            var_num = var_map[lit]
            clause.append("-" + str(var_num) if is_neg else str(var_num))
        clauses_list.append(set(clause))

    num_vars = len(var_map)
    num_clauses = len(clauses)
    
    try:
        with open(output_file, 'x') as f:
            f.write(f"p cnf {num_vars} {num_clauses}\n")
            for clause in clauses_list:
                line = " ".join(map(str, clause)) + " 0\n"
                f.write(line)
    except:
        with open(output_file, 'w') as f:
            f.write(f"p cnf {num_vars} {num_clauses}\n")
            for clause in clauses_list:
                line = " ".join(map(str, clause)) + " 0\n"
                f.write(line)
        
    return var_map
    
import re
"(¬g1+g3) * (g2+¬g4) * (g3+g4) * (g3+g5) * (g6+g7+g8) * (g6+¬g7) * (g1)"
formule = "(g1+g2+g3) * (¬g1+g3) * (g1+¬g3) * (g2+¬g4) * (g1+g5) * (g3+g5) * (g3+g4+¬g5) * (g6+g7+g8) * (g6+¬g7)"
file = "test1.dimacs"
parse_formula(formule, file)
simplify_cnf(file, True)