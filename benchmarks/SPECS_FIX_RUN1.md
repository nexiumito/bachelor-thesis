# SPECS_FIX_RUN1 — corrections identifiées par l'audit du run `20260502_111329`

Document de spécifications pour les correctifs à apporter avant le re-bench
complet sur racer. Ne contient que des specs, pas de code. Chaque entrée
décrit le périmètre, le diagnostic, le comportement attendu après fix, la
procédure de validation et les dépendances.

Source de l'audit : conversation Claude du 2026-05-02 (run
`benchmarks/results/20260502_111329`, 2h00 sur racer, 113 OK / 61 crashes /
5 timeouts / 23 FAIL hard d'invariants / 45 WARN soft).

---

## Sommaire

| ID | Titre | Sévérité | Effort | Dépend de |
|---|---|---|---|---|
| B1 | Générateur — clauses vides en type1/type2 | critique | M | — |
| B2 | Z3 runner — empty clauses silencieusement skippées | critique | S | — |
| B3 | trie.c — `exit(EXIT_FAILURE)` au lieu de `return NULL` | critique | M | — |
| B4 | C — overflow sharpsat non détecté (sticky flag) | critique | L | — |
| B5 | C — overflow `dnnf_bound_7k3nm` non détecté | critique | S | — |
| B6 | Orchestrator — capturer `stderr_tail` dans structure.csv | important | S | — |
| B7 | Notifier — coalescing des FAIL hard / backoff sur 429 | important | M | — |
| B8 | Z3 runner — `sat propagations 2ary/nary` | cosmétique | S | — |
| B9 | Invariants — skipper `psw_within_family_bound` hors greedy | cosmétique | S | — |
| P1 | plot `time_vs_pswidth` — label slope vs courbe x³ contradictoires | cosmétique | S | — |
| P2 | plot `dag_size_vs_bound` — passer en log-x | cosmétique | S | — |
| P3 | plot `greedy_vs_linear` — layout cassé, illisible | important | M | — |
| P4 | plot `pswidth_vs_theory` — log-y obligatoire | cosmétique | S | — |
| P5 | plot `z3_conflicts_vs_pswidth` — passer en log-x | cosmétique | S | — |

Notation effort : **S** = ≤ 30 min, **M** = 1–3 h, **L** = ½–1 jour.

---

## B1 — Générateur produit des clauses vides

- **Sévérité** : critique
- **Effort** : M (incluant la régénération de tous les fichiers)
- **Fichiers concernés** :
  - `src/data/script/generator.py` :
    - `generate_type1_interval` lignes 88–182 (boucle de construction lignes 164–174)
    - `generate_type2_interval_fixed_size` lignes 189–311 (boucle 295–307)
  - **18 fichiers DIMACS contaminés** (à régénérer) :
    - `src/data/type1/*.cnf` : tous les 14 fichiers
    - `src/data/type2/{type2_v25_c100_t3, type2_v25_c100_t3_ordered, type2_v500_c2000_t3_ordered, type2_v50_c200_t4}.cnf`

### Diagnostic

Dans `generate_type1_interval`, pour chaque clause `c_id`, la liste de ses
littéraux est construite en itérant sur les variables et en filtrant celles
dont l'intervalle chevauche celui de la clause. Quand **aucune** variable
n'est en vie pendant l'intervalle de la clause, la liste reste vide et est
quand même `clauses.append(clause)`. Le DIMACS résultant contient une ligne
avec uniquement ` 0`, qui par convention DIMACS est **la clause vide ⊥** —
toujours fausse — rendant la formule entière UNSAT.

Preuve : audit a compté jusqu'à **97 clauses vides sur 600** dans
`type1_v500_c600_ordered.cnf`. Idem `type2_v500_c2000_t3_ordered.cnf`
(84/2000), `type2_v50_c200_t4.cnf` (68/200), etc.

### Spec du fix

1. **Garde anti-clause-vide dans `generate_type1_interval`** : avant
   `clauses.append(clause)`, si `len(clause) == 0`, ignorer cette clause
   (ne pas l'ajouter). Conserver les autres clauses non-vides telles
   qu'elles. Le compteur effectif de clauses peut donc devenir < `m`.
2. **Idem pour `generate_type2_interval_fixed_size`** : la boucle ligne
   295–307 émet 4 clauses identiques pour chaque intervalle. Si
   `vars_in_clause` est vide, on saute les 4 émissions correspondantes.
3. **Documenter le décalage `m_demandé` vs `m_effectif`** : le nom de
   fichier garde le `m` paramétré, mais l'en-tête DIMACS doit refléter le
   nombre réel de clauses (ce que `write_dimacs` fait déjà via
   `len(clauses)`). Ajouter dans le commentaire DIMACS la ligne
   `c m_requested=<X>, m_effective=<Y>` quand `Y < X` (purement informatif,
   ne change pas la sémantique parser).
4. **Renommer le pattern de fichier (option à débattre)** : si une
   contraction significative survient (ex. v500_c600 devient v500_c503),
   garder le nom `type1_v500_c600.cnf` car le `c600` reflète la commande
   de génération, pas le nombre réel. Voir validation pour le check.
5. **Régénération** : commande unique
   `cd src/data/script && python3 generator.py`. Cela écrase tous les
   fichiers `data/type1/*.cnf`, `data/type2/*.cnf`, `data/type3/*.cnf`,
   `data/random/*.cnf`. **Décision recommandée** : versionner les `.cnf`
   régénérés dans le commit (cohérent avec le repo actuel qui les
   versionne déjà). Documenter la commande dans `data/README.md` pour
   rappel.
6. **Effet sur `instances.yaml`** : les `n_vars`/`n_clauses` annoncés
   peuvent être légèrement faux après régé. Spec : le harness lit en
   priorité l'en-tête DIMACS (déjà le cas dans `run_z3.parse_dimacs` et
   le solveur C), donc rien à changer dans `instances.yaml` — sauf si le
   `m` change drastiquement, auquel cas remettre à jour à la main.

### Validation

```bash
# Avant le fix : compter les clauses vides
for f in src/data/type1/*.cnf src/data/type2/*.cnf; do
  n=$(awk '!/^c/ && !/^p/ {gsub(/^[[:space:]]+|[[:space:]]+$/,""); if ($0 == "0" || $0 == "") print}' "$f" | wc -l)
  [ "$n" -gt 0 ] && echo "$f : $n vides"
done
# Attendu après fix + régénération : aucune sortie.

# Cohérence avec Z3 sur une instance qui FAIL hard avant
cd src && ./sat_solver ../data/type1/type1_v20_c25_ordered.cnf greedy --json | python3 -m json.tool | grep -E "sharpsat|maxsat"
# Si cette instance est devenue SAT après régé : sharpsat > 0, maxsat = num_clauses_effectif
```

### Dépendances

Aucune. Indépendant de tout autre fix. **À faire en premier** car les
fichiers régénérés conditionnent la validité de tous les autres tests.

---

## B2 — Z3 runner skippe silencieusement les clauses vides

- **Sévérité** : critique
- **Effort** : S
- **Fichiers concernés** :
  - `benchmarks/runners/run_z3.py` lignes 55–78 (`parse_dimacs`),
    lignes 121–220 (`run_z3_maxsat`), lignes 226–297 (`run_z3_sat`).

### Diagnostic

`parse_dimacs` filtre les littéraux nuls puis `if lits: clauses.append(lits)`,
ce qui **drop silencieusement** toute clause vide. Z3 voit donc une formule
strictement plus permissive que le solveur C → invariant
`consistency_match_dp_z3` faussement déclenché en FAIL hard (22 cas dans le
run audité, tous expliqués par ce bug + B1). Même après régénération B1, ce
bug doit être corrigé par défense en profondeur : un futur générateur buggé
re-produira le même symptôme.

### Spec du fix

Choix d'approche : **court-circuit en UNSAT au parse**, raison :

- Sémantiquement strict : une clause vide rend la formule UNSAT, on n'a
  donc pas besoin d'appeler Z3.
- Plus rapide : on évite de construire un solveur Z3 pour une formule
  trivialement UNSAT.
- Plus robuste : pas de dépendance à `BoolVal(False)` qui peut surprendre
  le futur lecteur.

Comportement attendu :

1. `parse_dimacs(path)` retourne `(n_vars, n_clauses, clauses,
   has_empty_clause: bool)`. Quatrième élément ajouté au tuple.
2. Une clause `lits == []` est conservée dans `clauses` comme `[]` (au lieu
   d'être ignorée), et `has_empty_clause` est mis à `True` à sa première
   occurrence. Cela permet de garder la trace dans le compte de clauses.
3. `run_z3_maxsat` : si `has_empty_clause`, retourner directement
   ```python
   {**base, "z3_status": "unsat", "z3_maxsat": <m sans la clause vide>,
    "z3_solve_ms": 0.0, "z3_total_ms": <wall>, "z3_n_vars": n,
    "z3_n_clauses": m, "z3_stats_keys_available": "",
    "z3_short_circuit_empty_clause": True}
   ```
   Le champ booléen est nouveau, à ajouter aussi à `Z3_CSV_COLS` dans
   `orchestrator.py` (cf. dépendance avec B6 ci-dessous).
4. `run_z3_sat` : idem mais `z3_status = "unsat"` et
   `z3_sat_status = "unsat"`.
5. **Note importante sur la sémantique de `z3_maxsat`** : pour MaxSAT, la
   clause vide est intrinsèquement UNSAT donc *jamais* satisfiable. Donc
   `z3_maxsat` est au plus `m - n_empty`. On peut soit retourner
   `m - n_empty` directement (UNSAT mais on connaît la borne sup), soit
   retourner None pour indiquer "non calculé". Choix : retourner
   `m - n_empty` (utile pour les invariants : DP doit aussi maxer à
   `m - n_empty`).

### Validation

```bash
# Mock test rapide : créer une instance avec clause vide
echo -e "p cnf 3 3\n1 0\n 0\n2 3 0" > /tmp/empty.cnf
cd benchmarks
PYTHONPATH=. python3 -c "
from runners.run_z3 import run_z3_maxsat, run_z3_sat
class I: id='t'; path='/tmp/empty.cnf'
print(run_z3_maxsat(I(), 30, repo_root='.'))
print(run_z3_sat(I(), 30, repo_root='.'))
"
# Attendu : z3_status=unsat, z3_short_circuit_empty_clause=True dans les deux.
```

### Dépendances

- B6 (ajout colonne CSV) si on veut le booléen `z3_short_circuit_empty_clause`
  persisté ; sinon ce booléen reste interne au runner.

---

## B3 — `trie.c` utilise `exit(EXIT_FAILURE)` au lieu de propager NULL

- **Sévérité** : critique
- **Effort** : M
- **Fichiers concernés** :
  - `src/utils/trie.c` lignes 16–32 (`allocate_trie_node`), lignes 53–71
    (`create_trie`)
  - `src/utils/trie.h` (signature de `allocate_trie_node` et invariants)
  - `src/main.c` lignes 700–720 (chemin JSON après `create_trie`)
  - Callers de `allocate_trie_node` : à grep dans `src/utils/trie.c` et
    éventuellement dans `src/utils/ps_set.c`, `src/algo/procedure*.c` si
    importé indirectement.
  - **Audit défensif** : `src/utils/dnnf.c` lignes 30–53
    (`allocate_raw_node`, `pool_register_node`, `create_dnnf_pool`) ne
    checkent pas non plus les `malloc/realloc`. À traiter dans le même
    commit pour cohérence.

### Diagnostic

Quand le trie n'arrive pas à grandir au-delà de la capacité initiale
(pression mémoire, `RLIMIT_AS` atteint, instance trop grosse), `realloc`
retourne NULL, et `trie.c:22` appelle `exit(EXIT_FAILURE)`. Le process
quitte avec code 1 **sans** passer par `print_json_error()` → stdout vide
→ le runner Python classifie en `status=crash`. Reproduit localement sur
`type3_n5000_t3_s2 linear` :
```
Erreur : Impossible d'allouer de la mémoire pour le Trie Binaire.
---EXIT CODE: 1---
```
Cause de **35 crashes** sur 61 dans le run.

### Spec du fix

1. **Signature `allocate_trie_node`** : passer de `static int` à
   `static int` (inchangée) mais retourner **`-1` en cas d'échec**
   (au lieu d'`exit`). Caller doit propager.
2. **`create_trie`** : retourner `NULL` en cas d'échec d'allocation au
   lieu d'`exit`. Inclus le `malloc(sizeof(BinaryTrie))` non checké
   ligne 54.
3. **Callers** : tout endroit qui appelle `allocate_trie_node` ou
   `insert_or_get_ps_set` doit gérer le retour `-1`. Probable point
   chaud : `insert_or_get_ps_set` dans `trie.c` lui-même. Stratégie de
   propagation : la fonction `insert_or_get_ps_set` doit aussi pouvoir
   retourner `-1`. Cette propagation atteint `compute_ps_prime_bottom_up`
   dans `procedure1.c`, `compute_ps_bar_top_down` dans `procedure2.c`, et
   la boucle de `solve_dp_recursive` dans `procedure3.c` (lignes 215, 230,
   239 où `insert_or_get_ps_set` est appelé). Ces appels actuels stockent
   le résultat dans `int id_Cv`, `id_c1b`, `id_c2b` — il suffit de
   propager `-1` jusqu'au caller principal `solve_dp` qui retourne un
   `DPResult` avec un nouveau champ `alloc_failed: int`.
4. **`DPResult`** (dans `procedure3.h`) : ajouter `int alloc_failed;`
   initialisé à 0 ; mis à 1 si une allocation a échoué.
5. **`main.c` `solve_formula_json`** : après `solve_dp(...)`, tester
   `dp_result.alloc_failed`. Si 1 :
   ```c
   print_json_error(filename, mode_str, "alloc_fail",
                    "trie ou structure DP n'a pas pu grandir (RLIMIT_AS ?)");
   // cleanup puis return 1
   ```
6. **Cohérence dnnf.c** :
   - `allocate_raw_node` (ligne 30) : retourner NULL si malloc échoue.
   - `pool_register_node` (ligne 45) : retourner -1 si realloc échoue ;
     transformer en `static int` au lieu de `void` ; appelants doivent
     gérer.
   - `create_dnnf_pool` (ligne 55) : retourner NULL si la création des
     singletons TRUE/FALSE échoue.
   - `dnnf_make_*` factories : retourner NULL si pool en échec.
   - Ces NULL doivent remonter jusqu'à `solve_dp_recursive` qui doit
     mettre `dp_result.alloc_failed = 1` et arrêter la récursion proprement.
7. **Garde-fou ASan** : la modification doit passer `make asan` sans
   nouveau warning.

### Validation

```bash
# Avant : reproduire le crash
cd src && make rebuild
# Limiter mémoire artificiellement pour forcer l'échec :
ulimit -v 2000000  # 2 GiB
./sat_solver ../data/type3/type3_n5000_t3_s2.cnf linear --json
# Attendu APRÈS fix : ligne JSON avec status="alloc_fail", error_message
# explicite, pas de stderr "Impossible d'allouer".

# Smoke test classique
make rebuild && ./sat_solver ../data/exemple1.cnf greedy --json | python3 -m json.tool
# Attendu : status=ok, dnnf_count_match=true (comportement inchangé sur
# instances qui ne saturent pas la mémoire).

# ASan
make asan && ./sat_solver ../data/exemple1.cnf manual --json 2>&1 | grep -i sanitizer
# Attendu : aucune sortie.
```

### Dépendances

Aucune. **Bloquante pour la validité du run** car sans elle, on ne saura
pas distinguer un vrai crash d'un OOM légitime.

---

## B4 — Détection d'overflow sharpsat insuffisante

- **Sévérité** : critique
- **Effort** : L
- **Fichiers concernés** :
  - `src/main.c` lignes 826–828 (détection finale, à supprimer/remplacer)
    et lignes 798–828 (sérialisation JSON)
  - `src/algo/procedure3.c` lignes 260–262 (multiplication + accumulation
    sharpsat dans `solve_dp_recursive`)
  - `src/utils/dnnf.c` (à examiner pour `dnnf_count` qui accumule aussi)
  - `src/utils/dp_table.c` / `src/utils/dp_table.h` (champ sharpsat de la
    DPTable, voir si on stocke un drapeau par cellule)
  - `src/algo/procedure3.h` (`DPResult` à étendre)

### Diagnostic

`main.c:826-828` :
```c
int sharpsat_overflow = (sharpsat > (long long)1e18 ||
                         sharpsat < -(long long)1e18) ? 1 : 0;
```
Cette détection est *post-hoc* sur la valeur finale. En arithmétique
`long long` (signed wrap-around indéfini en C, mais GCC -fwrapv en pratique
wrap), une valeur qui a débordé plusieurs fois pendant l'accumulation peut
atterrir près de 0 ou même à exactement 0. Pour type3_n200_t3_s2 (n=200),
sharpsat théorique ≈ 2^100 ≫ 2^63 = INT64_MAX. La cellule racine wrap
plusieurs fois ; valeur finale observée = **0**. La détection ne se
déclenche pas. L'invariant `consistency_match_dp_z3` voit alors
`maxsat=m=400` (correct, formule SAT) mais `sharpsat=0` (incorrect) →
incohérence interne du DP.

### Spec du fix

Drapeau "sticky overflow" propagé via `__builtin_mul_overflow` et
`__builtin_add_overflow` :

1. **Champ sharpsat de DPTable** : introduire un *parallèle* `long long*
   sharpsat` + `unsigned char* sharpsat_overflow` (un octet par cellule).
   Alternative plus économe : un seul drapeau global au niveau de la
   DPTable (`int overflow;`). Recommandation : **flag global sticky par
   DPTable**, simple et suffisant car une seule cellule overflow contamine
   tout le DAG de toute façon.
2. **Multiplication ligne 261** :
   ```c
   long long prod;
   if (__builtin_mul_overflow(tab_c1->sharpsat[cell_c1],
                              tab_c2->sharpsat[cell_c2], &prod)) {
       tab_v->overflow = 1;
       prod = LLONG_MAX;  // valeur saturée pour garder la branche maxsat
                          // cohérente, mais le drapeau dit "ne pas s'y fier"
   }
   long long sum;
   if (__builtin_add_overflow(tab_v->sharpsat[cell_v], prod, &sum)) {
       tab_v->overflow = 1;
       sum = LLONG_MAX;
   }
   tab_v->sharpsat[cell_v] = sum;
   ```
3. **Propagation parent←enfants** : avant la libération des tables enfants
   (lignes 292–293), faire `tab_v->overflow |= tab_c1->overflow |
   tab_c2->overflow;`. Une fois levé, le drapeau est sticky.
4. **DPResult** : ajouter `int sharpsat_overflow_flag;` mis à
   `tab_root->overflow` à la fin de `solve_dp`.
5. **`dnnf_count` dans dnnf.c** : même traitement pour le recompte. La
   fonction doit accepter un pointeur `int* overflow_out` ou retourner un
   tuple via une struct. Recommandation : ajouter `int*
   overflow_out` paramètre OUT, NULL-safe. Si overflow détecté, le caller
   doit savoir.
6. **Sérialisation JSON dans main.c** :
   - Remplacer `sharpsat_overflow = (sharpsat > 1e18 …)` par
     `sharpsat_overflow = dp_result.sharpsat_overflow_flag`.
   - Remplacer `dnnf_cnt_overflow = (dnnf_cnt > 1e18 …)` par le drapeau
     remonté depuis `dnnf_count`.
   - Ajouter explicitement les drapeaux `"sharpsat_overflow":true|false`
     et `"dnnf_count_overflow":true|false` (en plus du sentinel string
     `"overflow"` quand vrai), pour que les invariants Python puissent
     lire un booléen propre.
7. **Spec invariants** : dans `benchmarks/runners/invariants.py`
   `_check_dnnf_count_match` et `_check_consistency_match`, traiter
   `sharpsat_overflow_flag = true` comme "SAT" (sharpsat > 0 par
   définition d'un overflow). Déjà partiellement géré via `_is_overflow`
   sur la string ; il faut s'assurer que c'est cohérent avec le nouveau
   booléen.

### Validation

```bash
cd src && make rebuild
# Cas overflow connu :
./sat_solver ../data/type3/type3_n200_t3_s2.cnf greedy --json | python3 -m json.tool
# Attendu : "sharpsat":"overflow", "sharpsat_overflow":true, et dnnf_count
# aussi, ET "dnnf_count_match":true (les deux concordent en disant
# overflow).

# Cas sans overflow :
./sat_solver ../data/exemple1.cnf greedy --json | python3 -m json.tool
# Attendu : "sharpsat":17, "sharpsat_overflow":false, dnnf_count=17.

# Cas frontière :
./sat_solver ../data/type3/type3_n100_t3_s2.cnf greedy --json | python3 -m json.tool
# Attendu : sharpsat ≈ 2^50 = 1125899906842624, sharpsat_overflow=false.
```

### Dépendances

- **B3 doit être fait avant** car la propagation NULL/alloc_failed traverse
  les mêmes chemins ; faire les deux en un seul commit C minimise les
  risques.
- B6 (capture stderr) recommandé pour faciliter le debug si on découvre un
  cas où le drapeau ne se propage pas correctement.

---

## B5 — Détection d'overflow `dnnf_bound_7k3nm` insuffisante

- **Sévérité** : critique
- **Effort** : S
- **Fichiers concernés** :
  - `src/main.c` lignes 811–822

### Diagnostic

```c
if (ps_width > 2642245) bound_overflow = 1;
else bound = 7LL * psw_cube * ((long long)f.num_vars + (long long)f.num_clauses);
```
Le seuil 2642245 ≈ ∛(INT64_MAX) ne couvre que le cube isolé. Il manque
le facteur `7 * (n+m)`. Pour `psw=757024`, `psw³ = 4.34e17 < 9.22e18 =
INT64_MAX`, mais `7 * 4.34e17 * 180 = 5.46e20` overflow. Preuve dans la
sortie : `dnnf_bound_7k3nm = -6764736377788170240` (négatif).

### Spec du fix

Remplacer le calcul par une chaîne de `__builtin_mul_overflow` :

```c
long long bound;
int bound_overflow = 0;
long long psw_sq;
if (__builtin_mul_overflow((long long)ps_width, (long long)ps_width, &psw_sq)) {
    bound_overflow = 1;
} else {
    long long psw_cube;
    if (__builtin_mul_overflow(psw_sq, (long long)ps_width, &psw_cube)) {
        bound_overflow = 1;
    } else {
        long long t1;
        if (__builtin_mul_overflow(7LL, psw_cube, &t1)) {
            bound_overflow = 1;
        } else {
            long long nm = (long long)f.num_vars + (long long)f.num_clauses;
            if (__builtin_mul_overflow(t1, nm, &bound)) {
                bound_overflow = 1;
            }
        }
    }
}
```

Si `bound_overflow == 1`, sérialiser comme `"dnnf_bound_7k3nm":"overflow"`.
Supprimer le seuil magique 2642245.

### Validation

```bash
cd src && make rebuild
./sat_solver ../data/type1/type1_v80_c100.cnf linear --json | python3 -m json.tool | grep -E "ps_width|dnnf_bound|bound_overflow"
# Attendu : ps_width très grand, "dnnf_bound_7k3nm":"overflow",
# "bound_overflow":true.
```

### Dépendances

Aucune. Indépendant. Peut être fait dans le même commit que B4 (touche le
même bloc de sérialisation JSON dans main.c).

---

## B6 — Capturer `stderr_tail` dans structure.csv

- **Sévérité** : important
- **Effort** : S
- **Fichiers concernés** :
  - `benchmarks/orchestrator.py` lignes 169–178 (constante
    `STRUCTURE_CSV_COLS`), lignes 596–628 (`_row_for_dp`)
  - `benchmarks/runners/run_dp.py` (déjà capture `stderr` dans le dict
    retourné, ligne 159)

### Diagnostic

Le runner DP capture déjà les 4 derniers KB de stderr et les met dans
`result["stderr"]`, mais l'orchestrateur ne sérialise pas ce champ dans
`structure.csv`. Conséquence : on a perdu l'info "Erreur : Impossible
d'allouer de la mémoire pour le Trie Binaire." pendant l'audit ; il a
fallu reproduire localement pour identifier B3. Coût d'audit évité ≈ 1 h.

### Spec du fix

1. **Ajouter colonne `stderr_tail`** à la fin de `STRUCTURE_CSV_COLS` (ne
   pas insérer au milieu pour préserver la compatibilité de relecture des
   anciens CSV).
2. **`_row_for_dp`** : ajouter `"stderr_tail": result.get("stderr", "")`.
3. **Tronquer à 1024 caractères** au moment de l'écriture pour éviter de
   gonfler le CSV (4 KB en cellule fait des CSV illisibles). Ajouter cette
   troncature dans `CsvAppender.append` ou directement dans `_row_for_dp`.
4. **Échappement CSV** : le module `csv` Python gère déjà
   `quoting=QUOTE_MINIMAL`, mais s'assurer que les newlines internes (le
   stderr peut en contenir) ne cassent pas le parsing :
   `result.get("stderr", "").replace("\n", " | ")`.
5. **Pendant qu'on y est** : ajouter aussi `"z3_short_circuit_empty_clause"`
   à `Z3_CSV_COLS` (cf. B2), au même endroit (fin de la liste).

### Validation

```bash
make -C benchmarks bench-smoke
LATEST=$(ls -t benchmarks/results/ | head -1)
head -1 benchmarks/results/$LATEST/structure.csv | tr ',' '\n' | grep stderr
# Attendu : stderr_tail
awk -F, 'NR>1 && $7!="ok" {print $30}' benchmarks/results/$LATEST/structure.csv
# Attendu : pour les rows en crash, on voit le stderr (au moins 1 row si
# l'instance a stderr-é).
```

### Dépendances

Aucune. Indépendant.

---

## B7 — Notifier Discord — coalescing FAIL hard / backoff sur 429

- **Sévérité** : important
- **Effort** : M
- **Fichiers concernés** :
  - `benchmarks/notifier.py` (`_DiscordBackend`, `Notifier.fail_hard`)

### Diagnostic

Discord webhook limite à ~5 requêtes / 5 s. Le run audité a généré 23 FAIL
hard quasi-simultanés à 14:14 ; rate-limit 429 déclenché ; messages
ultérieurs perdus. Cosmétique pour le bench (rien de critique) mais on
manque le récap final si plusieurs FAIL en rafale.

### Spec du fix

Stratégie : **buffer + flush périodique côté `fail_hard`** + **backoff
exponentiel sur 429**.

1. **Buffer interne dans `Notifier`** : nouvelle attribut
   `_fail_hard_buffer: list[str]` (thread-safe via `threading.Lock`).
2. **`fail_hard(...)`** : ne plus envoyer immédiatement. À la place,
   `self._fail_hard_buffer.append(line)`.
3. **Thread daemon de flush** : nouveau thread démarré dans `__init__`,
   qui tourne avec `interval = 5s` et fait :
   ```python
   while not stop_event.is_set():
       stop_event.wait(5.0)
       with self._lock:
           batch, self._fail_hard_buffer = self._fail_hard_buffer, []
       if batch:
           # Discord cap 2000 chars, on coupe en morceaux
           message = f"{len(batch)} FAIL hard :\n" + "\n".join(batch)
           self._send_chunked(message)
   ```
4. **Méthode `stop()`** : ajouter une méthode qui flush le buffer et arrête
   le thread proprement. À appeler en fin de `orchestrator.main()`
   (idéalement avant `notifier.finished(...)`).
5. **Backoff sur 429** : dans `_DiscordBackend.send`, sur HTTP 429, lire le
   header `Retry-After` (en secondes), `time.sleep(min(retry, 10))` puis
   réessayer une fois. Si encore 429, abandonner et logger warning.
6. **`heartbeat` non concerné** : déjà cadencé à 30 min, pas de risque de
   rate limit. Pas de changement.

### Validation

Test manuel sur racer : forcer 30 FAIL hard rapides via une instance
synthétique trompeuse, vérifier que Discord reçoit 1 message agrégé toutes
les 5 s au lieu de 30 messages individuels.

### Dépendances

Aucune. Indépendant.

---

## B8 — Z3 propagations key non capturée

- **Sévérité** : cosmétique
- **Effort** : S
- **Fichiers concernés** :
  - `benchmarks/runners/run_z3.py` lignes 84–115 (`_extract_stats`,
    dictionnaire `aliases`)

### Diagnostic

Sur racer, Z3 émet les stats sous les clés `sat propagations 2ary` et
`sat propagations nary`, séparément. La table `aliases` actuelle cherche
`["sat propagations", "propagations", "binary propagations"]` — aucune ne
matche. Donc `z3_propagations` reste à `None` pour TOUTES les instances
sur racer. Ce n'est pas utilisé dans les plots (le plot
`z3_conflicts_vs_pswidth` se base sur `z3_conflicts` qui marche), donc
purement cosmétique pour l'instant.

### Spec du fix

Remplacer la valeur d'`aliases["z3_propagations"]` par une logique
sommatoire :

```python
# Au lieu de chercher une seule clé, sommer toutes les clés qui
# correspondent au pattern "sat propagations*" + "propagations".
def _sum_propagations(stats_obj, keys_seen):
    total = None
    for k in keys_seen:
        kl = k.lower().strip()
        if kl in ("sat propagations", "propagations", "binary propagations") \
           or kl.startswith("sat propagations "):  # 2ary, nary, ternary, etc.
            try:
                v = stats_obj.get_key_value(k) if hasattr(stats_obj, "get_key_value") else stats_obj[k]
                total = (total or 0) + int(v)
            except Exception:
                pass
    return total
```

Et dans `_extract_stats`, appeler `_sum_propagations(stats_obj, keys_seen)`
pour `z3_propagations`. Si aucune clé ne matche, retourner None
(comportement actuel).

### Validation

```bash
cd ~/bachelor/bachelor-thesis
PYTHONPATH=benchmarks python3 -c "
from runners.run_z3 import run_z3_maxsat
class I: id='t'; path='../data/random/random_k3_v15_c64_difficile.cnf'
print(run_z3_maxsat(I(), 30, repo_root='.').get('z3_propagations'))
"
# Attendu : un entier non-None (somme 2ary + nary).
```

### Dépendances

Aucune. Indépendant.

---

## B9 — `psw_within_family_bound` : skipper hors greedy

- **Sévérité** : cosmétique
- **Effort** : S
- **Fichiers concernés** :
  - `benchmarks/runners/invariants.py` lignes 178–207 (`_check_psw_bound`)

### Diagnostic

Après le fix précédent (downgrade hard→soft hors greedy), on a 45 WARN
soft dans le run, parce que la borne `psw ≤ X` ne s'applique structurellement
qu'à la décomposition optimale, approximée par greedy. linear/random
n'ont AUCUNE garantie. Conserver des WARN pollue
`invariants.csv` et le SUMMARY. Plus rigoureux : SKIPPER complètement.

### Spec du fix

Dans `_check_psw_bound`, après calcul de `mode = row["mode"]`, ajouter en
tête :

```python
if mode != "greedy":
    return _Check(**base, status="SKIPPED",
                   expected="-", observed=str(row.get("ps_width")),
                   message=f"borne psw definie pour decompo optimale (greedy uniquement)")
```

Conséquence : on n'a plus le WARN, on a un SKIPPED avec message explicite.
Le `severity` reste `effective_severity` (cosmétique car SKIPPED).

### Validation

```bash
make -C benchmarks bench-smoke
LATEST=$(ls -t benchmarks/results/ | head -1)
grep "psw_within_family_bound" benchmarks/results/$LATEST/invariants.csv | awk -F, '{print $4":"$6}' | sort | uniq -c
# Attendu : aucun WARN sur les modes linear/random ; uniquement OK ou SKIPPED.
```

### Dépendances

Aucune. Indépendant.

---

## P1 — `time_vs_pswidth` : label slope vs courbe x³ contradictoires

- **Sévérité** : cosmétique
- **Effort** : S
- **Fichiers concernés** :
  - `benchmarks/plots/time_vs_pswidth.py` lignes 65–78

### Diagnostic

Le code fait une régression log-log et calcule une `slope` observée, puis
trace `yy = exp(intercept) * xx ** 3.0` (courbe forcée à exposant 3) tout
en affichant `slope obs.={slope:.2f}` dans la légende. Sur le run audité,
slope=1.08, mais la courbe tracée est x³, pente visuelle=3 → label
visuellement faux. Risque de confusion en thèse.

### Spec du fix

Tracer **deux** courbes distinctes :

1. **Courbe fittée empirique** : `yy_fit = exp(intercept) * xx ** slope`
   en pointillés gris foncé. Légende : `f"y = c·x^{{{slope:.2f}}} (régression log-log)"`.
2. **Courbe de référence théorique** : `yy_theory = c_calibre * xx ** 3`
   en pointillés rouges. La constante `c_calibre` doit être choisie
   pour que la courbe x³ passe par le point empirique le plus à gauche
   (calibration low-end), histoire de rendre la comparaison visuelle
   honnête. Légende : `"y ∝ x³ (borne théorique Théorème 2 papier)"`.

Si moins de 5 points → ne tracer ni l'une ni l'autre (comportement actuel).

### Validation

Regarder le PDF généré ; les deux courbes doivent être visuellement
distinctes (couleur + style), légendes claires, et la courbe rouge doit
clairement dominer la courbe grise quand slope_obs ≪ 3.

### Dépendances

Aucune.

---

## P2 — `dag_size_vs_bound` : axe x linéaire avec outlier

- **Sévérité** : cosmétique
- **Effort** : S
- **Fichiers concernés** :
  - `benchmarks/plots/dag_size_vs_bound.py` lignes 42–56

### Diagnostic

L'axe y est déjà en log (ligne 52). L'axe x reste linéaire et un outlier
psw≈32000 écrase tous les autres points dans la zone gauche. Inutilisable.

### Spec du fix

Ajouter `ax.set_xscale("log")` après le `set_yscale("log")`. C'est le seul
changement nécessaire. Vérifier que tous les ps_width > 0 (déjà filtré par
le `ps_width` dropna précédent).

### Validation

Visuel : les points doivent maintenant s'étaler sur 4-5 décades en x au
lieu d'être agglutinés à gauche.

### Dépendances

Aucune.

---

## P3 — `greedy_vs_linear` : layout cassé, illisible

- **Sévérité** : important
- **Effort** : M
- **Fichiers concernés** :
  - `benchmarks/plots/greedy_vs_linear.py`

### Diagnostic

Le plot actuel met **toutes les instances** en x (≥30), figure
`figsize=(11, 4.2)` trop petite, labels rotés à 45° qui se chevauchent et
restent illisibles. Les valeurs sont sur 6 ordres de grandeur (psw=8 vs
psw=10⁶) sur une échelle linéaire → la plupart des barres sont à 0.

### Spec du fix

Refonte complète du layout :

1. **2 figures séparées** au lieu de subplots côte-à-côte. Fichiers :
   - `greedy_vs_linear_pswidth.{pdf,png}` : barres horizontales (rotation
     plus naturelle pour les noms d'instances) avec deux séries (greedy,
     linear), tri par n_vars croissant.
   - `greedy_vs_linear_time.{pdf,png}` : idem mais pour le temps.
2. **Axe valeurs en log** (à la fois pour psw et pour temps), car les
   ratios greedy/linear peuvent atteindre 10⁵ sur les hard cases.
3. **`figsize` adaptatif** : `(8, max(6, 0.3 * n_instances))`.
4. **Labels y** = noms d'instances en horizontal (lisible).
5. **Sélection top-N optionnel** : si `n_instances > 40`, afficher les
   25 instances avec le plus grand ratio `linear/greedy`, et noter dans
   le titre `"Top 25 / N total instances par ratio linear/greedy"`. Sinon
   tout afficher.
6. **Annotation** : pour chaque paire de barres, afficher le ratio
   linear/greedy à droite (texte court : `×42.7` etc.).
7. **Légende** : à part, en haut-droit hors plot.

Adapter `make_all_plots.py` si besoin pour gérer le fait que ce module
peut maintenant produire 2 plots (le module `greedy_vs_linear` peut
s'appeler 2 fois, ou exporter une fonction `make` qui génère les deux).

### Validation

Visuel : ouvrir les 2 PDF, vérifier que chaque instance a un label
lisible, que les valeurs s'étalent correctement en log, et que le ratio
linear/greedy est facilement comparable d'une ligne à l'autre.

### Dépendances

Aucune. Indépendant des autres fixes plot (ils touchent d'autres modules).

---

## P4 — `pswidth_vs_theory` : log-y obligatoire

- **Sévérité** : cosmétique
- **Effort** : S
- **Fichiers concernés** :
  - `benchmarks/plots/pswidth_vs_theory.py` lignes 70–86

### Diagnostic

Y axis linéaire ; un type2 à psw≈32000 écrase tout. Toutes les autres
barres (psw < 100 typiquement) deviennent invisibles. La représentation
hlines pour la borne théorique reste lisible mais perd toute valeur
informative.

### Spec du fix

1. `ax.set_yscale("log")` après `set_ylabel`.
2. **Vérifier que `psw > 0`** : log-y plante sur 0. Filtrer les rows avec
   `psw == 0` (pas attendu en pratique, mais défense en profondeur).
3. **Étendre les hlines** : sur log-y, `hlines(exp_psw, …)` doit toujours
   matcher visuellement la borne. Vérifier que tous les `expected_psw_max`
   sont > 0.
4. **Légende** : ajouter une entrée pour les hlines noires `"borne
   théorique (par instance)"`. Actuellement absente.
5. **Couleur des barres dépassant** : conserver le edgecolor rouge
   `linewidth=2`, on le voit mieux en log-y.

### Validation

Visuel : sur le PDF, on doit voir clairement les barres `psw=8` (type3)
ET la barre `psw=32000` (type2 outlier) sans que l'une écrase l'autre.

### Dépendances

Aucune.

---

## P5 — `z3_conflicts_vs_pswidth` : log-x

- **Sévérité** : cosmétique
- **Effort** : S
- **Fichiers concernés** :
  - `benchmarks/plots/z3_conflicts_vs_pswidth.py` lignes 46–69

### Diagnostic

Y est déjà en log conditionnel (ratio max/min > 100). X reste linéaire et
même outlier que P2/P4 (un type2 à psw≈32000) → illisible.

### Spec du fix

`ax.set_xscale("log")` inconditionnel après `set_xlabel`. Considérer aussi
de forcer y en log même si ratio < 100, pour cohérence avec time_vs_pswidth
et dag_size_vs_bound.

### Validation

Visuel : étalement correct des points sur 4-5 décades en x.

### Dépendances

Aucune.

---

## Plan d'exécution

### Ordre recommandé

```
Étape 1 — Fix générateur + Z3 parser  (B1, B2)        → 1 commit
Étape 2 — Régénération données        (post-B1)       → 1 commit (.cnf)
Étape 3 — Fixes solveur C             (B3, B4, B5)    → 1 commit (cohérence
                                                        des chemins JSON)
Étape 4 — Smoke test + ASan local                     → validation
Étape 5 — Fixes harness                (B6, B7, B8, B9)→ 1 commit
Étape 6 — Fixes plots                  (P1..P5)        → 1 commit
Étape 7 — Smoke test final + lancement bench long
```

### Fixes commitables séparément

- **B1** : commit dédié pour le fix générateur, avec son hunk de doc.
- **Régénération .cnf** (post-B1) : commit séparé contenant uniquement les
  fichiers `data/type1/*.cnf` et `data/type2/*.cnf` régénérés. Permet de
  reverter l'un sans l'autre si besoin.
- **B2** : commit dédié pour `run_z3.py`. Indépendant de B1 sur le plan
  code.
- **B3 + B4 + B5** : commit C unifié, car ils touchent les mêmes chemins
  JSON dans `main.c` et la même structure `DPResult`. Faire les 3 d'un
  coup garantit la cohérence (sinon un commit intermédiaire compile mais
  peut produire un binaire dans un état hybride).
- **B6, B8, B9** : commit harness (3 petits fixes).
- **B7** : commit dédié notifier (logique threadée plus délicate, isoler).
- **P1, P2, P5** : commit "petits fixes plots" (3 changements
  d'1-2 lignes).
- **P3, P4** : commits dédiés (refonte plus substantielle).

### Effort total

| Catégorie | Effort cumulé estimé |
|---|---|
| Petits S (B5, B6, B8, B9, P1, P2, P4, P5) | ~3 h |
| Moyens M (B1, B3, B7, P3) | ~6–10 h |
| Gros L (B4) | ~1 jour |
| Régénération + validation | ~2 h |
| **Total** | **~2-3 jours de travail** |

### Check-list pré-bench (à valider avant `make -C benchmarks bench`)

- [ ] **B1 validé** : `find src/data/{type1,type2} -name '*.cnf'
      -exec sh -c 'awk "..." "$1" | grep -c ^ ' _ {} \;` retourne 0
      partout.
- [ ] **Régénération .cnf** committée et pushée.
- [ ] **B2 validé** : test unitaire avec `/tmp/empty.cnf` retourne unsat
      avec `z3_short_circuit_empty_clause: true`.
- [ ] **Build C clean** : `make -C src clean && make -C src` sans
      warning autre que le `%llu` cosmétique connu.
- [ ] **ASan** : `make -C src asan && ./sat_solver ../data/exemple1.cnf
      manual --json 2>&1 | grep -i sanitizer` ne renvoie rien.
- [ ] **B3/B4/B5 testés** : 3 commandes JSON de la section validation
      respective, chacune retourne une ligne JSON valide même en cas
      d'échec d'allocation ou d'overflow.
- [ ] **Smoke test** : `make -C benchmarks bench-smoke` → 0 FAIL hard,
      `dnnf_count_match=true` partout, 8 plots OK, SUMMARY.md sain.
- [ ] **Smoke résultats** : visualiser les 8 plots du smoke et vérifier
      qu'aucun n'est cassé visuellement (P1..P5 effectivement appliqués).
- [ ] **Test reprise** : `make -C benchmarks bench-resume` après un
      `kill -INT` à mi-chemin du smoke, doit logger `skip N déjà faites`
      et compléter le run.
- [ ] **Notifier** : envoyer 5+ FAIL hard simulés, vérifier qu'ils sont
      coalescés en 1 message Discord.
- [ ] **Mapping CCD** : `lscpu --extended | head -30` confirme toujours
      que CPUs 0/8/16/24 sont sur 4 CCDs distincts (vérification déjà
      faite, juste s'assurer que rien n'a bougé).
- [ ] **Disque** : `df -h ~/bachelor/bachelor-thesis/benchmarks/results/`
      ≥ 5 GiB libre (orchestrateur check 5 GiB minimum mais run complet
      consomme ~500 MiB).
- [ ] **`git status`** : repo propre, pas de fichier non-tracké, branche
      `main` à jour avec origin.
- [ ] **Tag pré-bench** : `git tag -a bench-run-2 -m "Pre-bench run 2,
      post-fixes B1-B9 + P1-P5"` puis `git push --tags` pour pouvoir
      référencer ce point dans le rapport de thèse et reproduire.

### Rappels post-bench

- Comparer le SUMMARY du nouveau run avec celui du run 1
  (`20260502_111329`) : on doit voir
  - 0 FAIL hard sur `consistency_match_dp_z3` (B1+B2)
  - 0 ou 1 FAIL hard sur `dag_within_bcms_bound` (B5 ; reste possible si
    nouvelle instance trouve un cas oublié)
  - sharpsat correctement marqué `"overflow"` sur les grandes type3 (B4)
  - aucun crash `exit_code=1 stdout vide` (B3)
  - WARN soft `psw_within_family_bound` = 0 (B9)
- Archiver le tag git et la commande `make bench` pour reproduction
  exacte. Mettre à jour `thesis/` avec les nouvelles figures si publiables.

---

*Document généré le 2026-05-02 à partir de l'audit du run
`20260502_111329`. Référence externe : conversation Claude associée.*
