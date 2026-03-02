from SATTools import *

# Générer les fichiers et récupérer les stats
def var_clause_analyse(primes_per_bits: dict):
    data = {
        'bit_size': [],
        'vars_simplified': [],
        'vars_unsimplified': [],
        'clauses_simplified': [],
        'clauses_unsimplified': [],
    }
    for bit_size, primes in primes_per_bits.items():
        x, y = primes[:2]

        for simplifie in [False, True]:
            ts, _, _, _ = SAT_producer(x, y, simplifie=simplifie, repository="../dimacsAnalyse/varClauseCompare")

            nb_vars, nb_clauses = get_varClause(ts.dimacs)

            if simplifie:
                data['vars_simplified'].append(nb_vars)
                data['clauses_simplified'].append(nb_clauses)
            else:
                data['vars_unsimplified'].append(nb_vars)
                data['clauses_unsimplified'].append(nb_clauses)

        data['bit_size'].append(bit_size)
    return data