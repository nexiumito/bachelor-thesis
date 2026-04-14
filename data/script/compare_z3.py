#!/usr/bin/env python3
"""
Compare le solveur DP (src/sat_solver) avec Z3 sur MaxSAT.

Z3 est un SMT solver de décision. Pour MaxSAT on utilise Z3 Optimize avec
des contraintes souples (une par clause), puis on compte combien de clauses
sont satisfaites par le modèle retourné.

Le comptage exact de modèles (#SAT) n'est pas traité ici : Z3 ne le fait pas
nativement et il faudra brancher un vrai compteur (sharpSAT, ganak, d4)
dans un script séparé plus tard.

Usage :
    # Comparaison sur une seule formule
    python3 compare_z3.py ../type1/type1_v20_c25.cnf

    # Comparaison sur un dossier entier, avec un budget de 30s par tâche
    python3 compare_z3.py ../type3 --timeout 30

    # Inclure aussi le solveur DP en parallèle
    python3 compare_z3.py ../type1 --dp ../../src/sat_solver --mode greedy

Les chemins sont relatifs au cwd courant.
"""

import argparse
import os
import subprocess
import sys
import time

from z3 import Bool, Not, Or, Optimize


# ---------------------------------------------------------------------------
# Parsing DIMACS
# ---------------------------------------------------------------------------

def parse_dimacs(path):
    """Retourne (num_vars, num_clauses, list[list[int]])."""
    n_vars, n_clauses = 0, 0
    clauses = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("c"):
                continue
            if line.startswith("p cnf"):
                parts = line.split()
                n_vars = int(parts[2])
                n_clauses = int(parts[3])
                continue
            lits = [int(x) for x in line.split() if x and x != "0"]
            if lits:
                clauses.append(lits)
    return n_vars, n_clauses, clauses


# ---------------------------------------------------------------------------
# Z3 — MaxSAT via Optimize + soft constraints
# ---------------------------------------------------------------------------

def z3_maxsat(n_vars, clauses, timeout_s):
    V = [Bool(f"x{i+1}") for i in range(n_vars)]
    opt = Optimize()
    opt.set("timeout", int(timeout_s * 1000))
    for c in clauses:
        lits = [V[abs(l) - 1] if l > 0 else Not(V[abs(l) - 1]) for l in c]
        opt.add_soft(Or(lits))
    t0 = time.perf_counter()
    try:
        opt.check()
    except Exception as e:
        return f"ERR:{e}", time.perf_counter() - t0
    elapsed = time.perf_counter() - t0
    m = opt.model()
    # On compte manuellement le nombre de clauses satisfaites par le modèle.
    satisfied = 0
    for c in clauses:
        for l in c:
            v = V[abs(l) - 1]
            val = m.evaluate(v, model_completion=True)
            if (l > 0 and bool(val)) or (l < 0 and not bool(val)):
                satisfied += 1
                break
    return satisfied, elapsed


# ---------------------------------------------------------------------------
# Runner du solveur DP (optionnel)
# ---------------------------------------------------------------------------

def run_dp_solver(binary, cnf, mode):
    t0 = time.perf_counter()
    try:
        proc = subprocess.run(
            [binary, cnf, mode],
            capture_output=True, text=True, timeout=600,
        )
    except subprocess.TimeoutExpired:
        return "TIMEOUT", time.perf_counter() - t0
    elapsed = time.perf_counter() - t0
    maxsat = None
    for line in proc.stdout.splitlines():
        if line.startswith("MaxSAT"):
            try:
                maxsat = int(line.split(":", 1)[1].strip().split()[0])
            except Exception:
                pass
    return maxsat, elapsed


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("target", help="fichier .cnf ou dossier de .cnf")
    ap.add_argument("--timeout", type=float, default=30.0,
                    help="budget temps (s) pour Z3, défaut 30")
    ap.add_argument("--dp", default=None,
                    help="chemin vers sat_solver pour comparer en parallèle")
    ap.add_argument("--mode", default="greedy",
                    help="mode de l'arbre DP (défaut greedy)")
    args = ap.parse_args()

    # Collecter les fichiers à traiter
    if os.path.isdir(args.target):
        files = sorted(
            os.path.join(args.target, f)
            for f in os.listdir(args.target)
            if f.endswith((".cnf", ".dimacs"))
        )
    elif os.path.isfile(args.target):
        files = [args.target]
    else:
        print(f"Erreur : {args.target} introuvable", file=sys.stderr)
        sys.exit(1)

    if not files:
        print("Aucun fichier .cnf/.dimacs trouvé.", file=sys.stderr)
        sys.exit(1)

    # En-tête du tableau
    header = (f"{'Fichier':<40} {'n':>5} {'m':>6} "
              f"{'Z3_MaxSAT':>10} {'Z3_t(s)':>9}")
    if args.dp:
        header += f" {'DP_MaxSAT':>10} {'DP_t(s)':>9}"
    print(header)
    print("-" * len(header))

    for cnf in files:
        name = os.path.basename(cnf)
        try:
            n, m, clauses = parse_dimacs(cnf)
        except Exception as e:
            print(f"{name:<40}  [PARSE ERR: {e}]")
            continue

        z_max, t_max = z3_maxsat(n, clauses, args.timeout)

        row = (f"{name:<40} {n:>5} {m:>6} "
               f"{str(z_max):>10} {t_max:>9.3f}")

        if args.dp:
            dp_max, dp_t = run_dp_solver(args.dp, cnf, args.mode)
            row += f" {str(dp_max):>10} {dp_t:>9.3f}"

        print(row)


if __name__ == "__main__":
    main()
