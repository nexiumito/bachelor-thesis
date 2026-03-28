from analysis.SATTools import *
from notifications.discordNotification import send_discord_notification
from z3 import *

def flip_variable_in_dimacs(dimacs_file: str, var: int):
    
    flipped_lines = []
    with open(dimacs_file, "r") as f:
        lines = f.readlines()
        
    for line in lines:
        if line.startswith("p cnf") or line.startswith("c") or line.strip() == "":
            flipped_lines.append(line)
            continue

        literals = line.strip().split()
        new_literals = []
        for lit in literals:
            if lit == "0":
                new_literals.append("0")
            elif lit == f"{var}":
                new_literals.append(f"-{var}")
            elif lit == f"-{var}":
                new_literals.append(f"{var}")
            else:
                new_literals.append(lit)

        flipped_lines.append(" ".join(new_literals) + "\n")

    # Réécriture avec changement de variable dans un nouveau fichier
    new_dimacs_file = dimacs_file + f"_{var}_modified"
    with open(new_dimacs_file, "w") as f:
        f.writelines(flipped_lines)

    return new_dimacs_file


def time_evaluation(pairs: dict[int, list[tuple[int, int]]], dimacs_dir: str, simplifie: bool = False, bloat: bool = False) -> dict[int, list[float]]:

    sat_per_bits = {}
    for size_bits, pair in pairs.items():
        sat_per_bits[size_bits] = []
        pair = random.choice(pairs)
        x, y = pair
        ts, _, _, _ = SAT_producer(x, y, simplifie=simplifie, bloat=bloat, repository=dimacs_dir)
        nb_vars, _ = get_varClause(ts.dimacs)
        for var in range(1, nb_vars+1):
            modified_dimacs = flip_variable_in_dimacs(ts.dimacs, var)
            solver = Solver()
            solver.from_file(modified_dimacs)
            satisfaisability = solver.check()
            if satisfaisability == "sat":
                sat_per_bits[size_bits].append("sat")
            else:
                sat_per_bits[size_bits].append("unsat")
            
    return sat_per_bits