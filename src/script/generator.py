import random
import os

def generate_random_ksat(num_vars, num_clauses, k=3):
    """
    Génère une formule k-SAT totalement aléatoire.
    Pour chaque clause, on tire k variables distinctes uniformément parmi les n variables, puis chaque littéral est nié indépendamment avec probabilité 1/2
    Seuil critique m / n = 4.67 en dessous duquel la formule est presque sûrement satisfaisable et au-dessus duquel elle est presque sûrement insatisfaisable (voir Proof of the satisfiability conjecture for large k, Jian Ding, Allan Sly, Nike Sun)
    Ici ratio = 2.5 donc formule satisfaisable en théorie..
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
        filename_t1 = f"{output_dir}/random_k{k}_v{vars_target}_c{clauses}.cnf"
        clauses_t1 = generate_random_ksat(vars_target, clauses, k)
        write_dimacs(vars_target, clauses_t1, filename_t1)
        print(f"[OK] {filename_t1}")

    print("==========================================")