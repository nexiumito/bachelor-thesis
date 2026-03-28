import hashlib
import time
import random
import matplotlib.pyplot as plt
import numpy as np
import csv
from z3 import *
from transformation.tseytin import *
from transformation.binaryConverter import *
from utils.config import DIMACS_DIR

def SAT_producer(x: int, y: int, simplifie: bool = True, partial_resolution: bool = False, bloat: bool = False, repository: str = DIMACS_DIR):
    """
    Génère un fichier dimacs dans 'repository'. Retourne la 'TseytinTransformation', les valeurs des variables des entrée 
    x et y ainsi que les valeurs des variables de sorties z.

    Args:
        x (int): _description_
        y (int): _description_
        simplifie (bool, optional): _description_. Defaults to True.
        bloat (bool, optional): _description_. Defaults to False.
        repository (str, optional): _description_. Defaults to DIMACS_DIR.

    Returns:
        _type_: _description_
    """
    if y > x:
        tmp = x
        x = y
        y = tmp
        
    z = x * y
    x_bin = dec_to_bin(x)
    y_bin = dec_to_bin(y)
    z_bin = dec_to_bin(z)
    
    if bloat:
        x_bin = len(x_bin) * "0" + x_bin
        y_bin = len(y_bin) * "0" + y_bin
        
    size_x = len(x_bin)
    size_y = len(y_bin)

    
    filename = f"{repository}/{size_x}x{size_y}_{hashlib.sha256((str(x) + str(y)).encode()).hexdigest()[:6]}_simplified.dimacs" if simplifie else f"{repository}/{size_x}x{size_y}_{hashlib.sha256((str(x) + str(y)).encode()).hexdigest()[:6]}.dimacs"
    
    
    ts_transform = TseytinTransformation()
    ts_transform.MULT(size_x, size_y)
    ts_transform.dimacs = filename
    
    vars_map = TseytinTransformation.parse_formula(ts_transform.sat, ts_transform.dimacs)
    TseytinTransformation.add_z_to_dimacs(z_bin, size_x, size_y, ts_transform.dimacs)
    
    x_vars, y_vars = TseytinTransformation.get_input_gate(vars_map)
    z_vars = TseytinTransformation.get_output_gate(size_x, size_y)
    
    if simplifie:
        TseytinTransformation.simplify_cnf(ts_transform.dimacs, partial_resolution)
    
    return ts_transform, x_vars, y_vars, z_vars



def solve(file: str, display: bool = False, x_vars: list = None, y_vars: list = None, z_vars: list = None):
    """ Résout la formule SAT décrit par le fichier 'file' qui esr en format dimacs avec z3 """
    if display:
        if None in (x_vars, y_vars, z_vars):
            raise ValueError("Quand 'display=True', vous devez spécifier 'x_vars', 'y_vars' et 'z_vars'")

    solver = Solver()
    solver.from_file(file)

    if solver.check() == sat:
        if display:
            model = solver.model()

            result = { "x": {}, "y": {}, "z": {} }
            
            for d in model.decls():
                var_num = int(d.name()[2:])  # Suppression du préfixe ζ et conversion en int
                
                if var_num in x_vars:
                    result["x"][var_num] = model[d]
                elif var_num in y_vars:
                    result["y"][var_num] = model[d]
                elif var_num in z_vars:
                    result["z"][var_num] = model[d]

            # Construire les sorties binaires en utilisant join() 
            def binary_output(variable_results):
                return "".join("0" if not variable_results[k] else "1" for k in sorted(variable_results.keys(), reverse=True))

            x_output = binary_output(result["x"])
            y_output = binary_output(result["y"])
            z_output = binary_output(result["z"])
            
            x_output_dec = bin_to_dec(str(x_output))
            y_output_dec = bin_to_dec(str(y_output))
            z_output_dec = bin_to_dec(str(z_output))
            
            print(f"{x_output_dec} * {y_output_dec} = {z_output_dec}")
    else:
        print("Problème insatisfiable.")


def time_to_solve(dimacs_file: str):
    """ Evalue le temps que prend la résolution d'une formule SAT grâce à z3 """
    start_time = time.time()
    solve(dimacs_file)
    finished_time = time.time() - start_time
        
    return finished_time



def get_varClause(filepath: str):
    """ Extraire nb variables et clauses """
    with open(filepath, 'r') as f:
        for line in f:
            if line.startswith('p cnf'):
                _, _, vars_, clauses = line.strip().split()
                return int(vars_), int(clauses)



def generate_mult(primes: list, count: int = 10) -> list[tuple[int, int]]:
    mults = set()
    tries = 0
    while len(mults) < count and tries < 1000:
        x, y = random.sample(primes, 2)
        pair = tuple(sorted((x, y)))
        mults.add(pair)
        tries += 1
    return list(mults)



def count_clause_sizes(dimacs_file: str) -> tuple[int, int, int]:
    """ Compte le nombre de clauses de 1, 2 et 3 variables dans un fichier DIMACS. """
    nb_1var = 0
    nb_2var = 0
    nb_3var = 0

    with open(dimacs_file, "r") as f:
        for line in f:
            line = line.strip()
            if line.startswith("p") or line.startswith("c") or not line:
                continue 

            literals = line.split()[:-1]  # on enlève le 0 de fin de clause

            if len(literals) == 1:
                nb_1var += 1
            elif len(literals) == 2:
                nb_2var += 1
            elif len(literals) == 3:
                nb_3var += 1

    return nb_1var, nb_2var, nb_3var



def random_3SAT_generator(two_vars_clause: int, three_vars_clause: int):
    
    pass