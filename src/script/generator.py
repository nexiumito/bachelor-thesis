import random
import os


# ===========================================================================
#  Generateur k-SAT aleatoire
# ===========================================================================

def generate_random_ksat(num_vars, num_clauses, k=3):
    """
    Genere une formule k-SAT totalement aleatoire.
    Utile pour observer la transition de phase (ratio m/n approx 4.26 pour 3-SAT).
    """
    clauses = []
    variables = list(range(1, num_vars + 1))
    for _ in range(num_clauses):
        chosen_vars = random.sample(variables, k)
        clause = []
        for var in chosen_vars:
            sign = 1 if random.random() < 0.5 else -1
            clause.append(sign * var)
        clauses.append(clause)
    return clauses


# ===========================================================================
#  Utilitaires
# ===========================================================================

def _permute_formula(n, clauses):
    """Permute aleatoirement les IDs des variables et l'ordre des clauses
    pour cacher la structure sous-jacente (interval ordering, arc circulaire)."""
    if n == 0:
        return clauses
    var_perm = list(range(1, n + 1))
    random.shuffle(var_perm)
    new_clauses = []
    for clause in clauses:
        new_clause = []
        for lit in clause:
            v = abs(lit)
            sign = 1 if lit > 0 else -1
            new_clause.append(sign * var_perm[v - 1])
        new_clauses.append(new_clause)
    random.shuffle(new_clauses)
    return new_clauses


def _xor_to_cnf(literals):
    """
    Transformation standard XOR(l1, ..., lt) = 1 en CNF.

    Pour chaque assignation de parite paire des litteraux (2^(t-1) au total),
    cree une clause bloquante. Retourne 2^(t-1) clauses chacune de taille t.
    """
    t = len(literals)
    clauses = []
    for mask in range(1 << t):
        bits = [(mask >> i) & 1 for i in range(t)]
        if sum(bits) % 2 == 0:
            clause = []
            for i in range(t):
                if bits[i] == 1:
                    clause.append(-literals[i])
                else:
                    clause.append(literals[i])
            clauses.append(clause)
    return clauses

def write_dimacs(num_vars, clauses, filename, comment=None):
    """
    Exporte la formule au format standard DIMACS CNF.
    """
    with open(filename, 'w') as f:
        if comment:
            f.write(f"c {comment}\n")
        else:
            f.write("c Formule generee pour le test du solveur DP-SAT\n")
        f.write(f"p cnf {num_vars} {len(clauses)}\n")
        for clause in clauses:
            f.write(" ".join(map(str, clause)) + " 0\n")


# ===========================================================================
#  Type 1 : Formules avec interval ordering
# ===========================================================================

def generate_type1_interval(n, m, permute=True):
    """
    Type 1 : Formules avec interval ordering.

    Genere n+m intervalles sur la droite reelle en iterant sur les points
    i = 1 .. 2(n+m).  A chaque etape, choisit aleatoirement parmi les cas
    legaux :
      1. Demarrer un intervalle de nouvelle variable (left endpoint = i)
      2. Demarrer un intervalle de nouvelle clause   (left endpoint = i)
      3. Terminer l'intervalle d'une variable active  (right endpoint = i)
      4. Terminer l'intervalle d'une clause active    (right endpoint = i)

    Conditions aux frontieres pour atteindre exactement n variables et m
    clauses.  Pour chaque clause, chaque variable dont l'intervalle chevauche
    est incluse positivement ou negativement (probabilite 1/2).
    L'interval ordering est donne par les right endpoints des intervalles.

    Si permute=True, variables et clauses sont permutees pour cacher l'ordering.
    Si permute=False, les variables sont numerotees selon l'interval ordering
    (right endpoints croissants) pour que le solveur en mode lineaire utilise
    directement la bonne decomposition -> ps-width bornee par min(m+1, 2^t).

    Retourne (num_vars, clauses).
    """
    total_steps = 2 * (n + m)
    var_intervals = {}       # var_id -> [left, right]
    clause_intervals = {}    # clause_id -> [left, right]
    live_vars = [] # intervalles variables ouvert
    live_clauses = [] # intervalles clauses ouvert
    vars_started = 0 # nombre de variables dont l'intervalle a déjà été ouvert jusqu'à ce step
    clauses_started = 0 # nombre de clauses dont l'intervalle a déjà été ouvert jusqu'à ce step

    for step in range(1, total_steps + 1):
        steps_remaining = total_steps - step + 1
        starts_remaining = (n - vars_started) + (m - clauses_started)
        ends_remaining = len(live_vars) + len(live_clauses)
        events_needed = starts_remaining + ends_remaining
        slack = steps_remaining - events_needed

        legal = []
        if vars_started < n and slack > 0:
            legal.append(1)
        if clauses_started < m and slack > 0:
            legal.append(2)
        if live_vars:
            legal.append(3)
        if live_clauses:
            legal.append(4)

        choice = random.choice(legal)

        if choice == 1:
            vars_started += 1
            var_intervals[vars_started] = [step, None]
            live_vars.append(vars_started)
        elif choice == 2:
            clauses_started += 1
            clause_intervals[clauses_started] = [step, None]
            live_clauses.append(clauses_started)
        elif choice == 3:
            v_id = random.choice(live_vars)
            live_vars.remove(v_id)
            var_intervals[v_id][1] = step
        elif choice == 4:
            c_id = random.choice(live_clauses)
            live_clauses.remove(c_id)
            clause_intervals[c_id][1] = step

    # Renumeroter les variables par right endpoint croissant (interval ordering)
    # Variable qui finit en premier reçoit l'id 1, la suivante l'id 2, etc..
    sorted_vars = sorted(var_intervals.keys(),
                         key=lambda v: var_intervals[v][1])
    # old_id -> new_id (1-indexed)
    var_remap = {old: new + 1 for new, old in enumerate(sorted_vars)}

    # Construire les clauses a partir des chevauchements d'intervalles
    clauses = []
    for c_id in range(1, m + 1):
        c_left, c_right = clause_intervals[c_id]
        clause = []
        for v_id in range(1, n + 1):
            v_left, v_right = var_intervals[v_id]
            # Chevauchement : v_left <= c_right ET c_left <= v_right
            if v_left <= c_right and c_left <= v_right:
                sign = 1 if random.random() < 0.5 else -1
                clause.append(sign * var_remap[v_id])
        clauses.append(clause)

    # Trier les clauses par right endpoint (interval ordering)
    clause_order = sorted(range(m), key=lambda i: clause_intervals[i + 1][1])
    clauses = [clauses[i] for i in clause_order]

    if permute:
        clauses = _permute_formula(n, clauses)
    return n, clauses


# ===========================================================================
#  Type 2 : Interval ordering avec taille de clause fixe t
# ===========================================================================

def generate_type2_interval_fixed_size(n, m, t, permute=True):
    """
    Type 2 : Comme Type 1 mais toutes les clauses ont la meme taille t.

    Seule modification par rapport au Type 1 : le cas 4 (fin de clause) n'est
    plus un choix aleatoire mais est force des qu'une clause active a accumule
    exactement t variables chevauchantes.

    Chaque intervalle de clause represente 4 clauses sur le meme ensemble de
    variables mais avec des litteraux choisis aleatoirement, dans le but
    d'augmenter la probabilite que l'instance ne soit pas satisfaisable.

    Si permute=False, les variables sont numerotees selon l'interval ordering
    (right endpoints croissants) -> ps-width bornee par min(m+1, 2^t).

    Retourne (num_vars, clauses) ou clauses contient 4 * m_effectif entrees.
    """
    total_steps = 2 * (n + m)
    var_intervals = {}
    clause_intervals = {}
    live_vars = set()
    live_clauses = set()
    clause_overlap_vars = {}   # clause_id -> set of overlapping var_ids
    vars_started = 0
    clauses_started = 0

    for step in range(1, total_steps + 1):
        # Verifier si une clause doit etre forcee a se terminer (cas 4) 
        forced_end = None
        for c_id in sorted(live_clauses):
            if len(clause_overlap_vars.get(c_id, set())) >= t:
                forced_end = c_id
                break

        if forced_end is not None:
            live_clauses.discard(forced_end)
            clause_intervals[forced_end][1] = step
            continue

        # Choix libre parmi les cas 1, 2, 3 (pas de cas 4 aleatoire)
        steps_remaining = total_steps - step + 1
        starts_remaining = (n - vars_started) + (m - clauses_started)
        ends_remaining = len(live_vars) + len(live_clauses)
        events_needed = starts_remaining + ends_remaining
        slack = steps_remaining - events_needed

        legal = []
        if vars_started < n and slack > 0:
            legal.append(1)
        if clauses_started < m and slack > 0:
            legal.append(2)
        if live_vars:
            legal.append(3)

        if not legal:
            # Cas limite : forcer la fin d'une clause meme sans t variables
            if live_clauses:
                c_id = min(live_clauses)
                live_clauses.discard(c_id)
                clause_intervals[c_id][1] = step
                continue
            break

        choice = random.choice(legal)

        if choice == 1:
            vars_started += 1
            v_id = vars_started
            var_intervals[v_id] = [step, None]
            live_vars.add(v_id)
            # Mettre a jour le chevauchement pour toutes les clauses actives
            for c_id in live_clauses:
                clause_overlap_vars.setdefault(c_id, set()).add(v_id)
        elif choice == 2:
            clauses_started += 1
            c_id = clauses_started
            clause_intervals[c_id] = [step, None]
            live_clauses.add(c_id)
            # La clause chevauche toutes les variables actuellement actives
            clause_overlap_vars[c_id] = set(live_vars)
        elif choice == 3:
            v_id = random.choice(list(live_vars))
            live_vars.discard(v_id)
            var_intervals[v_id][1] = step

    # Fermer les intervalles restants (cas limites)
    extra = total_steps + 1
    for v_id in list(live_vars):
        var_intervals[v_id][1] = extra
        extra += 1
    for c_id in list(live_clauses):
        clause_intervals[c_id][1] = extra
        extra += 1

    actual_n = vars_started

    # Renumeroter les variables par right endpoint croissant (interval ordering)
    sorted_vars = sorted(var_intervals.keys(),
                         key=lambda v: var_intervals[v][1])
    var_remap = {old: new + 1 for new, old in enumerate(sorted_vars)}

    # Construire les clauses : chaque intervalle produit 4 clauses de taille t
    # Trier les intervalles de clauses par right endpoint (interval ordering)
    sorted_clause_ids = sorted(range(1, clauses_started + 1),
                               key=lambda c: clause_intervals[c][1])

    all_clauses = []
    for c_id in sorted_clause_ids:
        vars_in_clause = sorted(clause_overlap_vars.get(c_id, set()))
        # Prendre exactement t variables (ou moins si pas assez)
        if len(vars_in_clause) > t:
            vars_in_clause = sorted(random.sample(vars_in_clause, t))
        # Generer 4 clauses sur les memes variables, signes aleatoires
        for _ in range(4):
            clause = []
            for v_id in vars_in_clause:
                sign = 1 if random.random() < 0.5 else -1
                clause.append(sign * var_remap[v_id])
            all_clauses.append(clause)

    if permute:
        all_clauses = _permute_formula(actual_n, all_clauses)
    return actual_n, all_clauses


# ===========================================================================
#  Type 3 : XOR circulaire (graphe d'arcs circulaires)
# ===========================================================================

def generate_type3_circular_xor(n, t, s, permute=True):
    """
    Type 3 : Formules XOR dont le graphe d'incidence est la bipartisation
    d'un graphe d'arcs circulaires.

    Parametres :
      n : nombre de variables (points successifs 1..n sur un cercle)
      t : taille de chaque fonction XOR (nombre de litteraux)
      s : decalage (step) entre fonctions XOR consecutives

    La i-eme fonction XOR (0-indexee) couvre les variables aux positions
    [(i*s) mod n + 1, ..., (i*s + t - 1) mod n + 1].
    Total de fonctions XOR : n // s.
    Chaque XOR est transforme en CNF standard avec 2^(t-1) clauses.
    Formule resultante : (n // s) * 2^(t-1) clauses.

    Variables choisies positivement ou negativement aleatoirement dans
    chaque XOR. Si permute=False, l'ordering circulaire naturel est
    preserve (variables 1..n dans l'ordre du cercle).

    Retourne (num_vars, clauses).
    """
    num_xors = n // s
    all_clauses = []

    for i in range(num_xors):
        # Variables de ce XOR : positions circulaires
        xor_vars = []
        for j in range(t):
            var_pos = (i * s + j) % n + 1   # 1-indexe
            xor_vars.append(var_pos)

        # Polarite aleatoire pour chaque variable
        literals = []
        for v in xor_vars:
            sign = 1 if random.random() < 0.5 else -1
            literals.append(sign * v)

        # Transformation XOR -> CNF (2^(t-1) clauses)
        all_clauses.extend(_xor_to_cnf(literals))

    if permute:
        all_clauses = _permute_formula(n, all_clauses)
    return n, all_clauses







if __name__ == "__main__":
    script_dir = os.path.dirname(os.path.abspath(__file__))
    output_dir = os.path.join(script_dir, "instances_test")
    os.makedirs(output_dir, exist_ok=True)

    print("=== GENERATION DE LA BATTERIE DE TESTS ===")
    print(f"Repertoire : {output_dir}\n")

    #  Type 1 : Interval ordering
    #  Structure cachee : interval bigraph -> petite ps-width lineaire
    print("--- Type 1 : Interval ordering ---")
    type1_configs = [
        # (n_variables, m_clauses)
        (20, 25),
        (30, 40),
        (50, 60),
        (80, 100),
        (150, 180),
        (200, 250),
        (500, 600),
    ]
    for n_vars, m_clauses in type1_configs:
        # Version permutee (ordering cache)
        fname = os.path.join(output_dir, f"type1_v{n_vars}_c{m_clauses}.cnf")
        nv, cls = generate_type1_interval(n_vars, m_clauses, permute=True)
        write_dimacs(nv, cls, fname,
                     f"Type 1: interval ordering (permuted), n={n_vars}, m={m_clauses}")
        print(f"  [OK] type1_v{n_vars}_c{m_clauses}.cnf "
              f"({nv} vars, {len(cls)} clauses)")
        # Version ordonnee (interval ordering preserve -> petite ps-width)
        fname_ord = os.path.join(output_dir,
                                 f"type1_v{n_vars}_c{m_clauses}_ordered.cnf")
        nv2, cls2 = generate_type1_interval(n_vars, m_clauses, permute=False)
        write_dimacs(nv2, cls2, fname_ord,
                     f"Type 1: interval ordering (ordered), n={n_vars}, m={m_clauses}")
        print(f"  [OK] type1_v{n_vars}_c{m_clauses}_ordered.cnf "
              f"({nv2} vars, {len(cls2)} clauses)")

    #  Type 2 : Interval ordering, taille de clause fixe t
    #  Chaque intervalle de clause -> 4 clauses CNF
    #  Structure cachee : interval bigraph -> petite ps-width lineaire
    print("\n--- Type 2 : Interval ordering, taille fixe ---")
    type2_configs = [
        # (n_approx, m_intervalles, t)
        # n ~ m, clauses reelles = 4 * m
        (25, 25, 3),
        (50, 50, 3),
        (100, 100, 3),
        (200, 200, 3),
        (500, 500, 3),
        (25, 25, 4),
        (50, 50, 4),
        (100, 100, 4),
    ]
    for n_param, m_intervals, t in type2_configs:
        # Version permutee
        nv, cls = generate_type2_interval_fixed_size(n_param, m_intervals, t,
                                                     permute=True)
        actual_c = len(cls)
        fname = os.path.join(output_dir,
                             f"type2_v{nv}_c{actual_c}_t{t}.cnf")
        write_dimacs(nv, cls, fname,
                     f"Type 2: interval ordering (permuted), n={nv}, "
                     f"m_intervals={m_intervals}, t={t}")
        print(f"  [OK] type2_v{nv}_c{actual_c}_t{t}.cnf "
              f"({nv} vars, {actual_c} clauses, t={t})")
        # Version ordonnee
        nv2, cls2 = generate_type2_interval_fixed_size(n_param, m_intervals, t,
                                                       permute=False)
        actual_c2 = len(cls2)
        fname_ord = os.path.join(output_dir,
                                 f"type2_v{nv2}_c{actual_c2}_t{t}_ordered.cnf")
        write_dimacs(nv2, cls2, fname_ord,
                     f"Type 2: interval ordering (ordered), n={nv2}, "
                     f"m_intervals={m_intervals}, t={t}")
        print(f"  [OK] type2_v{nv2}_c{actual_c2}_t{t}_ordered.cnf "
              f"({nv2} vars, {actual_c2} clauses, t={t})")

    #  Type 3 : XOR circulaire
    #  Structure cachee : bipartisation d'un graphe d'arcs circulaires
    #  -> ps-width <= min(m^2+1, 4^t)
    print("\n--- Type 3 : XOR circulaire ---")
    type3_configs = [
        # (n_variables, t_taille_xor, s_decalage)
        (30, 3, 2),
        (60, 3, 2),
        (100, 3, 2),
        (200, 3, 2),
        (60, 5, 3),
        (90, 5, 3),
        (120, 5, 3),
        (150, 5, 3),
    ]
    for n_vars, t, s in type3_configs:
        # Version permutee
        nv, cls = generate_type3_circular_xor(n_vars, t, s, permute=True)
        fname = os.path.join(output_dir,
                             f"type3_n{n_vars}_t{t}_s{s}.cnf")
        write_dimacs(nv, cls, fname,
                     f"Type 3: circular XOR (permuted), n={n_vars}, t={t}, s={s}")
        num_xors = n_vars // s
        print(f"  [OK] type3_n{n_vars}_t{t}_s{s}.cnf "
              f"({nv} vars, {len(cls)} clauses, {num_xors} XORs)")
        # Version ordonnee
        nv2, cls2 = generate_type3_circular_xor(n_vars, t, s, permute=False)
        fname_ord = os.path.join(output_dir,
                                 f"type3_n{n_vars}_t{t}_s{s}_ordered.cnf")
        write_dimacs(nv2, cls2, fname_ord,
                     f"Type 3: circular XOR (ordered), n={n_vars}, t={t}, s={s}")
        print(f"  [OK] type3_n{n_vars}_t{t}_s{s}_ordered.cnf "
              f"({nv2} vars, {len(cls2)} clauses, {num_xors} XORs)")

    # ------------------------------------------------------------------
    #  Random k-SAT
    # ------------------------------------------------------------------
    print("\n--- Random k-SAT ---")
    random_configs = [
        # (num_clauses, num_vars, k)
        (20, 8, 3),
        (50, 10, 3),
        (50, 12, 3),
        (100, 40, 3),
        (200, 80, 3),
        (50, 20, 4),
        (100, 40, 4),
    ]
    for num_clauses, num_vars, k in random_configs:
        fname = os.path.join(output_dir,
                             f"random_k{k}_v{num_vars}_c{num_clauses}.cnf")
        cls = generate_random_ksat(num_vars, num_clauses, k)
        write_dimacs(num_vars, cls, fname,
                     f"Random {k}-SAT, n={num_vars}, m={num_clauses}")
        print(f"  [OK] random_k{k}_v{num_vars}_c{num_clauses}.cnf "
              f"({num_vars} vars, {len(cls)} clauses)")

    # ------------------------------------------------------------------
    #  Formules difficiles pour notre DP
    #
    #  Ces formules sont difficiles car leur graphe d'incidence I(F) est
    #  aleatoire, sans structure d'interval bigraph. La ps-width explose
    #  avec la taille, rendant l'algorithme DP inefficace.
    #
    #  Critere de difficulte pour notre DP (different du critere DPLL) :
    #    - Pas de structure d'intervalle dans I(F) -> pas de petite ps-width
    #    - Ratio m/n ~ 4.26 = transition de phase pour 3-SAT (le plus dur
    #      pour les solveurs DPLL, car la formule est SAT/UNSAT en equilibre)
    #    - Ratio m/n ~ 2.0 = formule presque surement SAT (nombreuses solutions),
    #      mais toujours sans structure -> ps-width grande
    #    - Ratio m/n ~ 8.0 = formule presque surement UNSAT (#SAT=0),
    #      MaxSAT reste un probleme difficile a optimiser
    #
    #  Ces instances servent de baseline pour comparer les performances
    #  avant et apres implementation des backdoors.
    # ------------------------------------------------------------------
    print("\n--- Formules difficiles (3-SAT aleatoire sans structure) ---")
    print("    [Critere : ps-width grande car I(F) non-interval-bigraph]")

    difficile_configs = [
        # (num_vars, num_clauses, k, label_ratio)
        #
        # Transition de phase (ratio ~ 4.26) :
        # formules les plus dures pour DPLL et sans structure pour notre DP
        (15,  64,  3, "critique"),   # ratio 4.27 - faisable, verification possible
        (20,  85,  3, "critique"),   # ratio 4.25 - faisable
        (30,  128, 3, "critique"),   # ratio 4.27 - DP commence a peiner
        (50,  213, 3, "critique"),   # ratio 4.26 - DP probablement trop lent
        (100, 426, 3, "critique"),   # ratio 4.26 - necessite backdoor
        (200, 852, 3, "critique"),   # ratio 4.26 - DP inutilisable sans backdoor
        #
        # Ratio faible (ratio ~ 2.0) : presque surement SAT, mais sans structure
        (30,  60,  3, "sparse"),     # ratio 2.0
        (50,  100, 3, "sparse"),     # ratio 2.0
        (100, 200, 3, "sparse"),     # ratio 2.0
        #
        # Ratio eleve (ratio ~ 8.0) : presque surement UNSAT, MaxSAT interessant
        (30,  240, 3, "dense"),      # ratio 8.0
        (50,  400, 3, "dense"),      # ratio 8.0
    ]

    for num_vars, num_clauses, k, label in difficile_configs:
        ratio = num_clauses / num_vars
        fname = os.path.join(output_dir,
                             f"random_k{k}_v{num_vars}_c{num_clauses}_difficile.cnf")
        cls = generate_random_ksat(num_vars, num_clauses, k)
        write_dimacs(num_vars, cls, fname,
                     f"Difficile ({label}): random {k}-SAT sans structure, "
                     f"n={num_vars}, m={num_clauses}, ratio={ratio:.2f}")
        print(f"  [OK] random_k{k}_v{num_vars}_c{num_clauses}_difficile.cnf "
              f"({num_vars} vars, {num_clauses} clauses, ratio={ratio:.2f}, {label})")

    print("\n==========================================")
