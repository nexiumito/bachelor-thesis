import random
import os

def generate_random_ksat(num_vars, num_clauses, k=3):
    """
    Génère une formule k-SAT totalement aléatoire.
    """
    clauses = []
    variables = list(range(1, num_vars + 1))
    
    for _ in range(num_clauses):
        # sélectionne k variables distinctes
        chosen_vars = random.sample(variables, k)
        
        clause = []
        for var in chosen_vars:
            # 50% de chance d'être un littéral négatif
            sign = 1 if random.random() < 0.5 else -1
            clause.append(sign * var)
        clauses.append(clause)
        
    return clauses

def generate_bounded_pswidth(num_vars, num_clauses, num_blocks, k=3):
    """
    Génère une formule k-SAT avec une ps-width bornée.
    Les variables sont réparties en blocs. Une clause ne lie que des variables d'un bloc i et/ou de son voisin i+1.
    """
    clauses = []
    variables = list(range(1, num_vars + 1))
    
    # partitionner les variables en blocs CONTIGUS (ex: 1,2,3 puis 4,5,6)
    blocks = [[] for _ in range(num_blocks)]
    vars_per_block = max(1, num_vars // num_blocks)
    
    for i, var in enumerate(variables):
        # on calcule l'index du bloc pour que les variables se suivent
        block_idx = min(i // vars_per_block, num_blocks - 1)
        blocks[block_idx].append(var)
        
    for _ in range(num_clauses):
        # choisir un bloc de départ i (entre 0 et num_blocks - 2)
        # si num_blocks == 1, on retombe sur du Type 1
        if num_blocks > 1:
            i = random.randint(0, num_blocks - 2)
            # le pool de variables autorisées est B_i union B_{i+1}
            allowed_vars = blocks[i] + blocks[i+1]
        else:
            allowed_vars = variables
            
        # s'assurer qu'on a assez de variables dans le pool pour faire une clause de taille k
        if len(allowed_vars) < k:
            allowed_vars = variables # fallback de sécurité
            
        chosen_vars = random.sample(allowed_vars, k)
        
        clause = []
        for var in chosen_vars:
            sign = 1 if random.random() < 0.5 else -1
            clause.append(sign * var)
        clauses.append(clause)
        
    return clauses

def write_dimacs(num_vars, clauses, filename):
    """
    Exporte la formule au format standard DIMACS CNF.
    """
    with open(filename, 'w') as f:
        f.write(f"c Formule géneree pour le test du solveur DP-SAT\n")
        f.write(f"p cnf {num_vars} {len(clauses)}\n")
        for clause in clauses:
            f.write(" ".join(map(str, clause)) + " 0\n")

if __name__ == "__main__":
    # crée un dossier dédié pour éviter de polluer le dossier script
    output_dir = "instances_test"
    os.makedirs(output_dir, exist_ok=True)

    #  définit les cibles qu'on veut tester (Nb_Clauses, K)
    configs = [
        (20, 3),    # Minuscule (3-SAT)
        (50, 3),    # Petit (3-SAT)
        (100, 3),   # Moyen (3-SAT)
        (200, 3),   # Grand (3-SAT)
        (50, 4),    # Petit (4-SAT) 
        (100, 4)    # Moyen (4-SAT)
    ]

    print("=== GÉNÉRATION DE LA BATTERIE DE TESTS ===")

    for clauses, k in configs:
        # calcul automatique des paramètres optimaux (Ratio 2.5)
        # au moins 'k' variables pour ne pas faire planter la génération
        vars_target = max(k, int(clauses / 2.5))
        # blocs de taille 2 ou 3 maximum
        blocks = max(1, vars_target // 2) 

        # --- GÉNÉRATION Aléatoire pur ---
        filename_t1 = f"{output_dir}/type1_k{k}_v{vars_target}_c{clauses}.cnf"
        clauses_t1 = generate_random_ksat(vars_target, clauses, k)
        write_dimacs(vars_target, clauses_t1, filename_t1)
        print(f"[OK] {filename_t1}")

        # --- GÉNÉRATION PS-Width bornée ---
        filename_t3 = f"{output_dir}/type3_k{k}_v{vars_target}_c{clauses}_b{blocks}.cnf"
        clauses_t3 = generate_bounded_pswidth(vars_target, clauses, blocks, k)
        write_dimacs(vars_target, clauses_t3, filename_t3)
        print(f"[OK] {filename_t3}")

    print("==========================================")