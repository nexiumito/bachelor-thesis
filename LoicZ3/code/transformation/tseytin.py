import re

class TseytinTransformation:
    
    def __init__(self):
        self.i = 0  
        self.sat = ""   
        self.dimacs = ""
        
        
        
    def AND(self, A: str, B: str) -> str:
        gate = f"gate{self.i}"
        res = f"(¬{A} + ¬{B} + {gate}) * ({A} + ¬{gate}) * ({B} + ¬{gate})"
        self.i += 1 
        return res
    
    
    
    def OR(self, A: str, B: str) -> str:
        gate = f"gate{self.i}"
        res = f"({A} + {B} + ¬{gate}) * (¬{A} + {gate}) * (¬{B} + {gate})"
        self.i += 1 
        return res
    
    
    
    def XOR(self, A: str, B: str) -> str:
        gate = f"gate{self.i}"
        res = f"(¬{A} + ¬{B} + ¬{gate}) * ({A} + {B} + ¬{gate}) * ({A} + ¬{B} + {gate}) * (¬{A} + {B} + {gate})"
        self.i += 1 
        return res
    
    
    
    def half_ADD(self, A: str, B: str) -> str:
        m1 = self.XOR(A, B)
        g1 = f"gate{self.i-1}"
        m2 = self.AND(A, B)
        g2 = f"gate{self.i-1}"
        
        res = m1 + " * " + m2
        
        return (res, g1, g2)
    
    
    
    def full_ADD(self, A: str, B: str, c_in: str) -> str:
        m1 = self.XOR(A, B)
        g1 = f"gate{self.i-1}"
        m2 = self.XOR(g1, c_in)
        g2 = f"gate{self.i-1}" #correspond à la sortie z de l'additionneur
        m3 = self.AND(g1, c_in)
        g3 = f"gate{self.i-1}"
        m4 = self.AND(A, B)
        g4 = f"gate{self.i-1}"
        m5 = self.OR(g3, g4)
        g5 = f"gate{self.i-1}"
        
        res = m1 + " * " + m2 + " * " + m3 + " * " + m4 + " * " + m5 

        return (res, g2, g5)
    
    
    
    def MULT(self, size_x: int, size_y: int) -> str:
        """
        Ajoute à la variable 'sat' la formule SAT correspondant à x * y = z

        Args:
            size_x, size_y (int):
                taille des entrées x et y en bits 
                
        """
        
        X, Y = [f"x{i}" for i in range(size_x)], [f"y{i}" for i in range(size_y)]
        Z = []
        
        self.sat += self.AND(X[0], Y[0]) # "étage 0": première multiplication dont on récupère directement le résultat en sortie
        
        for i in range(size_y-1): # longueur des facteurs moins 1, correspond à l'indice des B
            for j in range(size_x): # correspond à l'indice des A
                if i == 0: # 1ère étage
                    if j < size_x - 1:
                        m1 = self.AND(X[j+1], Y[i])
                        g1 =  f"gate{self.i-1}"
                        m2 = self.AND(X[j], Y[i+1])
                        g2 = f"gate{self.i-1}"
                        
                        res, n, c_out = self.half_ADD(g1, g2) if j == 0 else self.full_ADD(g1, g2, c_out)
                        Z.append(n)
                        
                        self.sat += " * " + m1 + " * " + m2 + " * " + res
                        
                    else:
                        m = self.AND(X[j], Y[i+1])
                        g = f"gate{self.i-1}"
                        
                        res, n, c_out = self.half_ADD(c_out, g)
                        Z.append(n)
                        Z.append(c_out)
                        
                        self.sat += " * " + m + " * " + res
                        
                else:
                    
                    m = self.AND(X[j], Y[i+1])
                    g =  f"gate{self.i-1}"
                    res, n, c_out = self.half_ADD(Z[j], g) if j == 0 else self.full_ADD(Z[j], g, c_out)
                    Z[j] = n 
                    if j == size_x - 1:
                        Z.append(c_out)
                    
                    self.sat += " * " + m + " * " + res 
                    
            Z.pop(0) #permet de supprimer le resultat qui sera la sortie et donc plus nécessaire pour la suite des calculs
                
                
                
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
                lines_set = TseytinTransformation.simplification_unit_clause(lines_set)
            lines_set = TseytinTransformation.simplifier_one(lines_set)
            lines_set = TseytinTransformation.simplifier_two(lines_set)
            lines_set = TseytinTransformation.simplifier_three(lines_set)

            normalized = TseytinTransformation.normalize(lines_set) # évite de faire des deepcopy pour comparer les s1, s2, s3
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
    
    
    
    @staticmethod
    def get_input_gate(var_dict):
        x_vars = []
        y_vars = []

        for key, value in var_dict.items():
            if key.startswith("x"):
                x_vars.append(value)
            elif key.startswith("y"):
                y_vars.append(value)

        return sorted(x_vars), sorted(y_vars)          
                
                    
                    
    @staticmethod          
    def get_output_gate(size_x: int, size_y: int) -> list:
        """
        Trouve le numéro des variables correspondant à la sortie dans la formule sous forme DIMACS

        Args:
            size_x, size_y (int):
                taille des entrées x et y en bits 
                
        """
        
        
        outputs = []
        gate = 0
        smaller, bigger = (size_x, size_y) if size_x < size_y else (size_y, size_x)
        
        for i in range(smaller):
            
            if i == 0:
                gate += 3                       # 2 portes A0 et B0 + AND
                outputs.append(gate)
            
            elif i == 1:
                gate += 2 + 2 + 1                       # 2 portes: une A1 et B1 + 2*AND + 1ère porte HADD
                outputs.append(gate)
                gate += 1                               # 2ème porte HADD
                gate += (1 + 2 + 5) * (bigger - 2)      # [porte Ai + 2*AND + ADD] k-2 fois
                gate += 1 + 2                           # porte B + HADD
                
            elif i > 1 and i != smaller-1:
                gate += 1 + 1 + 1                       # porte Bi + AND + 1ère porte HADD
                outputs.append(gate)
                gate += 1                               # 2ème porte HADD
                gate += (1 + 5) * (bigger - 1)          # [AND + ADD] * k-1 fois
                
            else:
                gate += 1 + 1 + 1               # porte Bi + AND + 1ère porte HADD
                outputs.append(gate)
                gate += 1                       # 2ème porte HADD
                for _ in range(bigger - 1):
                    gate += 1 + 2               # AND + 2 premières porte du ADD
                    outputs.append(gate)
                    gate += 3                   # 3 dernières portes 
                outputs.append(gate)        
        
        return outputs
    
    
    
    @staticmethod
    def add_z_to_dimacs(z_bin: int, size_x: int, size_y: int, dimacs_file: str): 
        """
        Ajoute au fichier DIMACS la valeur des variables qui correspondent à la sortie z

        Args:
            z (int): 
                résultat en binaire de la multiplication x * y = z
            k (int):
                taille des entrées x et y en bits 
            dimacs_file (str):
                nom du fichier où se trouve la formule SAT en format DIMACS
                
        """
        
        z_var = TseytinTransformation.get_output_gate(size_x, size_y)
        
        while len(z_bin) != len (z_var):
            z_bin = "0" + z_bin
            
        with open(dimacs_file, "a") as f:
            for zi_bin, zi_var in zip(z_bin[::-1], z_var):
                f.write(f"{str(zi_var)} 0\n") if zi_bin == "1" else f.write(f"-{str(zi_var)} 0\n")
        
        with open(dimacs_file, "r") as f:
                lines = f.readlines()  # Lire toutes les lignes

        # Modifier la première ligne car on a ajouté des variables
        if lines[0].startswith("p cnf"):
            parts = lines[0].split()
            num_vars = parts[2]  # Récupérer le nombre de variables
            num_clauses = int(parts[3]) + len(z_var)  # Ajouter les nouvelles clauses
            lines[0] = f"p cnf {num_vars} {num_clauses}\n"  # Mettre à jour la ligne

        # Réécrire le fichier avec la ligne modifiée
        with open(dimacs_file, "w") as f:
            f.writelines(lines)