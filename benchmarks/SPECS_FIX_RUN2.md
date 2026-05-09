# SPECS_FIX_RUN2 — corrections identifiées par l'audit du run `20260502_162229`

Document de spécifications pour les correctifs à apporter avant le run 3.
Ne contient que des specs, pas de code. Format identique à
`SPECS_FIX_RUN1.md`.

Source : audit du run 2 (113 OK / 49 alloc_fail / 7 segfault / 9 timeout
/ 2 FAIL hard).

---

## Sommaire

| ID | Titre | Sévérité | Effort | Dépend de |
|---|---|---|---|---|
| C1 | Duplication `<repo>/data/` vs `<repo>/src/data/` (Option B) | critique | M | — |
| C2 | Saturation `LLONG_MAX` introduit FAIL hard fictif | important | S | C1 |
| C3 | 7 segfaults sur instances spécifiques | important | M-L | C1 |
| C4 | Vérification post-fix : plus de `z3_short_circuit_empty_clause` sur familles régénérées | docu | S | C1 |
| C5 | Vérification post-fix : `z3_conflicts_vs_pswidth` plus fourni | docu | S | C1 |

Notation effort : **S** = ≤ 30 min, **M** = 1–3 h, **L** = ½–1 jour.

---

## C1 — Déduplication `<repo>/data/` (Option B canonique)

- **Sévérité** : critique
- **Effort** : M (suppression + adaptations + validation)
- **Fichiers concernés** :
  - `<repo>/data/` (à supprimer entièrement, 96 fichiers tracked)
  - `benchmarks/config/instances.yaml` (réécrire les paths)
  - `benchmarks/config/benchmark.yaml` (vérifier `dp.cwd`)
  - `src/data/script/generator.py` (rendre `data_root` explicite)
  - `src/data/README.md` (déjà à la bonne place — vérifier)

### Diagnostic

Le repo contient deux dossiers `data/` indépendants, tous deux trackés
en git :

- `<repo>/data/` — legacy contaminé (586 clauses vides au total dans
  18 fichiers type1+type2)
- `<repo>/src/data/` — version régénérée par B1 du run précédent, propre

L'orchestrateur lance le solveur depuis `cwd=src/` avec
`instance.path=../data/...`, ce qui résout vers `<repo>/data/`. Donc
les régénérations B1 (qui touchent `src/data/`) **n'ont aucun effet** sur
le bench. Vérifié par hash MD5 : `data/type1/type1_v20_c25.cnf` (header
`p cnf 20 25`) ≠ `src/data/type1/type1_v20_c25.cnf` (header `p cnf 20 24`).

### Spec du fix

Convention choisie : la source de vérité est `<repo>/src/data/`. Le
solveur reste lancé depuis `cwd=src/` (binaire dans `src/`, donc cohérent),
mais les paths d'instances passent de `../data/...` à `data/...` (relatifs
à `cwd=src/`, donc résolus en `<repo>/src/data/...`).

1. **Supprimer `<repo>/data/`** : `git rm -r data/` (96 fichiers, dossier
   compris). Vérifier que `git status` ne montre plus rien à la racine.
2. **`benchmarks/config/instances.yaml`** : remplacer toutes les
   occurrences de `path: ../data/` par `path: data/` (90 lignes au total).
3. **`benchmarks/config/benchmark.yaml`** : `dp.cwd` reste `src` (pas de
   changement). `dp.binary` reste `src/sat_solver` (relatif au repo root,
   utilisé par `make`, pas par le solveur lui-même).
4. **`src/data/script/generator.py`** : remplacer la résolution
   `os.path.join(script_dir, "..")` par une résolution explicite et
   défensive :
   ```python
   script_dir = os.path.dirname(os.path.abspath(__file__))
   data_root = os.path.dirname(script_dir)  # parent = src/data/
   # Garde-fou : doit etre <repo>/src/data/.
   assert os.path.basename(data_root) == "data" and \
          os.path.basename(os.path.dirname(data_root)) == "src", \
          f"Layout inattendu : {data_root}"
   ```
   Cela ne change pas le comportement par hasard ; ça l'écrit explicitement.
5. **`src/data/README.md`** : déjà à la bonne place (vérifié).
   Le `<repo>/data/README.md` partira avec le `git rm -r data/`.

### Validation

```bash
# 1. Plus de <repo>/data/ après le fix
ls -d data 2>&1 | grep -E "Aucun|cannot" || echo "ERREUR : data/ encore present"

# 2. Le solveur lit src/data/ (verifier via n_clauses du JSON)
cd src
./sat_solver data/type1/type1_v20_c25.cnf greedy --json | python3 -m json.tool | grep n_clauses
# Attendu : n_clauses correspond au header DIMACS du fichier regenere
# (24 pour type1_v20_c25, PAS 25)

# 3. Tous les paths de instances.yaml resolvent
cd ..
PYTHONPATH=benchmarks python3 -c "
import yaml, os
inst = yaml.safe_load(open('benchmarks/config/instances.yaml').read())
missing = []
for entry in inst['instances']:
    p = os.path.join('src', entry['path'])
    if not os.path.exists(p):
        missing.append(entry['id'])
print(f'Instances manquantes : {len(missing)}')
"
# Attendu : 0 instance manquante

# 4. parse_dimacs ne detecte plus de clauses vides sur type1/type2
PYTHONPATH=benchmarks python3 -c "
from runners.run_z3 import parse_dimacs
import yaml
inst = yaml.safe_load(open('benchmarks/config/instances.yaml').read())
dirty = []
for entry in inst['instances']:
    fam = entry['family']
    if fam not in ('type1', 'type2', 'type3', 'random'):
        continue  # tseytin/factorization peuvent legitimement avoir clauses vides
    p = 'src/' + entry['path']
    n, m, clauses, has_empty = parse_dimacs(p)
    if has_empty:
        dirty.append((entry['id'], n, m))
print(f'Instances avec clause vide : {len(dirty)}')
for d in dirty: print(' ', d)
"
# Attendu : 0
```

### Dépendances

Aucune. **Bloquante pour C2, C3** (sans cleaning, on opère sur les
mauvais fichiers).

---

## C2 — Saturation `LLONG_MAX` introduit FAIL hard fictif

- **Sévérité** : important
- **Effort** : S
- **Fichiers concernés** :
  - `benchmarks/runners/invariants.py` (`_check_dnnf_count_match`,
    `_check_consistency_match`)

### Diagnostic

Sur `type2_v500_c2000_t3_ordered greedy` (run 2) :

| Champ | Valeur |
|---|---|
| `sharpsat` | `"overflow"` (saturé à LLONG_MAX en DP) |
| `dnnf_count_recomputed` | `0` (DAG simplifié à FALSE) |
| `dnnf_count_match` (C) | `false` |
| → invariant `dnnf_count_match` | FAIL hard |
| → invariant `consistency_match_dp_z3` | FAIL hard (dp_sat=True via overflow, z3_sat=False) |

Cause : la saturation à `LLONG_MAX` dans `procedure3.c` produit une
"valeur fausse mais visible" qui ne reflète pas la sémantique
mathématique du sous-arbre DP. Le DAG, lui, est correctement simplifié
par `dnnf_make_and(_, FALSE) → FALSE`, et son recompte donne 0.

Le DAG est la **source de vérité fiable** ici, parce qu'il bénéficie
des simplifications structurelles que la table DP ne propage pas.

Note : ce cas est apparu sur une instance contaminée du run 2, mais il
peut se produire sur des formules propres aussi (overflow intermédiaire
puis annulation par sous-arbre FALSE).

### Spec du fix

Approche choisie : **option (c) Python**. Faire confiance à
`dnnf_count_recomputed` quand `sharpsat` a overflow mais pas `dnnf_count`.
Plus chirurgical, ne touche pas le code C.

1. **`_check_dnnf_count_match`** : table de décision basée sur les flags
   d'overflow détectés via `_is_overflow(v)` :
   ```
   sharpsat_ovf | dnnf_ovf | match attendu
   ─────────────┼──────────┼────────────────────────────────
   T            | T        | OK (les deux sont saturés à >> 0,
                |          |    cohérent par convention)
   T            | F        | OK avec note "DP saturated, DAG
                |          |    simplifié — DAG = source vérité"
   F            | T        | FAIL (anormal : DP devrait overflow
                |          |    si le recompte overflow)
   F            | F        | OK ssi sharpsat == dnnf_count
   ```
   Le check ne fait plus confiance au flag `dnnf_count_match` du C
   (qui était calculé par stricte égalité), il re-dérive.

2. **`_check_consistency_match`** : recalculer `dp_sat` :
   - Si `sharpsat` overflow ET `dnnf_count` non-overflow :
     `dp_sat = (dnnf_count > 0)` (DAG = vérité)
   - Si `sharpsat` overflow ET `dnnf_count` overflow :
     `dp_sat = True` (les deux saturés, formule a beaucoup de modèles)
   - Sinon : `dp_sat = (sharpsat > 0)` (comportement actuel)
   - Comparaison avec `z3_sat` inchangée

3. **Aucun changement côté C**. Les drapeaux d'overflow restent
   sticky pour rester traçables, mais leur interprétation est rectifiée
   en Python.

### Validation

Après C1, re-tester localement :
```bash
cd src
./sat_solver data/type2/type2_v500_c2000_t3_ordered.cnf greedy --json | \
    python3 -m json.tool | grep -E "sharpsat|dnnf_count|maxsat"
# Note : avec C1 fait, la formule régénérée n'a PLUS de clauses vides
# donc le cas pathologique pourrait disparaître. Mais on construit le
# fix Python pour qu'il gère ce cas s'il revient.
```

Test unitaire synthétique : créer un row avec `sharpsat="overflow"`,
`dnnf_count_recomputed=0`, vérifier que `_check_dnnf_count_match`
retourne OK et `_check_consistency_match` traite `dp_sat=False`.

### Dépendances

C1 (cleaning des fichiers) ne strictement bloquant mais utile pour
valider sur le cas réel.

---

## C3 — 7 segfaults sur instances spécifiques

- **Sévérité** : important
- **Effort** : M-L (selon profondeur du bug)
- **Fichiers concernés** : à déterminer (probablement
  `procedure1.c`, `procedure2.c`, `procedure3.c`, `ps_set.c`, `trie.c`,
  `dnnf.c`)

### Diagnostic

7 instances ont segfault dans le run 2 :

| Instance | Mode | Famille | n | m |
|---|---|---|---|---|
| random_k3_v30_c128_difficile | greedy | random | 30 | 128 |
| random_k3_v40_c100 | greedy | random | 40 | 100 |
| random_k3_v50_c100_difficile | greedy | random | 50 | 100 |
| random_k4_v40_c100 | greedy | random | 40 | 100 |
| type3_n10000_t3_s2 | greedy | type3 | 10000 | 20000 |
| type3_n10000_t3_s2 | linear | type3 | 10000 | 20000 |
| tseytin_3x3 | random seed=2 | tseytin | — | — |

Pattern :
- Mode greedy domine (5 sur 7)
- random k=3 ou k=4 sur tailles modérées
- type3_n10000 (très grand) dans les deux modes
- tseytin_3x3 sur seed=2 du random sample

Sur les random et type3 contaminés, ces fichiers ne contiennent **pas
de clauses vides** dans `<repo>/data/` (l'ancien) — la contamination
B1 n'affecte que type1+type2. Donc le bug est probablement réel et
indépendant de la duplication data/.

### Spec du fix

Étape 1 — **reproduction locale** :
```bash
cd src
make asan
for inst in \
  random_k3_v30_c128_difficile \
  random_k3_v40_c100 \
  random_k3_v50_c100_difficile \
  random_k4_v40_c100 \
  type3_n10000_t3_s2 \
  tseytin_3x3 ; do
  echo "=== $inst greedy (ASan) ==="
  ./sat_solver data/random/${inst}.cnf greedy --json 2>&1 | head -50
  # Si pas dans random/, ajuster sous-dossier
done
```

Étape 2 — **identifier la cause** : ASan donne stack trace, on remonte
au caller. Hypothèses possibles à confirmer :
- Buffer overflow dans `ps_set` quand le nombre de PS-sets dépasse une
  limite implicite
- `seen_array` du trie pas resize correctement pour un cas particulier
- Recursion stack overflow sur n=10000 (procedure1/2/3 récursives)
- Allocation manquante check NULL après B3 (un endroit qu'on a oublié
  d'auditer)
- `__builtin_*_overflow` dans procedure3 non câblé partout

Étape 3 — **patcher** chaque cause identifiée. Garder les changements
minimaux et testés.

Étape 4 — **re-test ASan** pour confirmer 0 segfault sur les 7
instances + smoke complet.

### Validation

```bash
cd src && make asan
# Re-test des 7 instances : doivent toutes terminer (ok / alloc_fail /
# overflow), aucune ne doit segfault.
for inst in <les 7 instances>; do
    ./sat_solver data/<sous-dir>/${inst}.cnf <mode> --json 2>&1 | \
        grep -E "ERROR|sanitizer|segfault" && echo "FAIL: $inst"
done
echo "Si pas de FAIL ci-dessus : OK"
make rebuild
```

### Dépendances

C1 (pour avoir les bons paths). Ne bloque pas C2.

### Statut au moment de l'implementation (2026-05-03)

**Reproduction locale infructueuse en temps raisonnable.** Avec un timeout
de 60s en mode release sur ma machine de dev, les 4 instances `random/`
ne crashent pas — elles sont simplement lentes (atteignent le timeout
avant de produire une sortie). Les fichiers `random/*.cnf` sont
**identiques** entre l'ancien `<repo>/data/` et le nouveau
`<repo>/src/data/` (vérifié par hash MD5), donc C1 ne change rien à
ces instances.

**Décision pragmatique** : ne pas désactiver les 7 instances.

  - On laisse tourner au run 3 ; si elles segfault encore, on aura le
    `stderr_tail` capturé par B6 du run précédent pour creuser
    précisément.
  - Sur racer (EPYC, gcc -O3, runtime ~30 min) le segfault apparait
    peut-être après un long calcul ou dépend de l'ordonnancement OS.
  - Hypothèses techniques restant à explorer si le run 3 confirme :
    - Stack overflow sur type3_n10000 (récursion ~10k niveaux dans
      compute_ps_prime_bottom_up et solve_dp_recursive). Solution :
      `ulimit -s unlimited` côté harness, ou réécrire en itératif.
    - Buffer overrun dans `seen_array` du trie quand le ps_id dépasse
      la capacité initiale et que le realloc n'a pas eu lieu (cf.
      `add_to_node_ps_set` ligne 46 de ps_set.c).
    - Conditions de course mémoire dans le pool DNNF sur grands DAGs.

  - Si segfault persiste au run 3 sur ces mêmes instances, ouvrir un
    SPECS_FIX_RUN3.md dédié.

---

## C4 — Vérification post-fix : plus de `z3_short_circuit_empty_clause`

- **Sévérité** : documentation
- **Effort** : S (vérification au smoke test)
- **Fichiers concernés** : aucun (vérification uniquement)

### Diagnostic

Dans le run 2, **toutes les instances type1+type2** ont déclenché le
court-circuit Z3 (`z3_short_circuit_empty_clause: true` dans z3.csv)
parce qu'elles contenaient des clauses vides. Une fois C1 fait, ce
court-circuit ne devrait plus se déclencher pour ces familles.

### Spec du fix

Aucun changement de code. Vérification après C1 :

```bash
# Smoke local
cd benchmarks
make bench-smoke
LATEST=$(ls -t results/ | head -1)
grep -E ',true$' results/$LATEST/z3.csv
# Attendu : aucune sortie (sauf si tseytin/factorization ont vraiment
# des clauses vides, ce qu'on doit vérifier).
```

### Dépendances

C1.

---

## C5 — Vérification post-fix : `z3_conflicts_vs_pswidth` plus fourni

- **Sévérité** : documentation
- **Effort** : S (visuel)

### Diagnostic

Dans le run 2, le plot `z3_conflicts_vs_pswidth.png` n'avait que ~25
points exploitables (Z3 court-circuitait sur les clauses vides → pas de
conflits sur ces instances). Une fois C1 fait, Z3 devra vraiment
résoudre les formules type1/type2, donc générer des conflits, donc
plus de points.

### Spec du fix

Aucun changement de code. Vérification visuelle au smoke ou au run 3 :
le plot doit afficher significativement plus de points (~80+ après
filtre `z3_conflicts > 0`).

### Dépendances

C1.

---

## Plan d'exécution

### Ordre recommandé

```
Étape 1 — C1 (data/ dedup)        → 1 commit (suppression + paths
                                            + generator.py)
Étape 2 — C2 (saturation Python)  → 1 commit (invariants.py)
Étape 3 — C3 (segfaults)          → 1 commit par bug, ou 1 commit
                                    récap si plusieurs causes liées
Étape 4 — Validation locale       → smoke + ASan + parse_dimacs
                                    (pas de commit)
Étape 5 — Push + pull racer       → bench complet (run 3)
```

### Commits séparables

- **C1** : commit dédié (`fix(data): supprimer duplication data/, paths
  pointent vers src/data/`).
- **C2** : commit dédié (`fix(invariants): tolere DP overflow vs DAG
  precis`).
- **C3** : commit(s) selon les bugs trouvés.

### Effort total estimé

| Étape | Effort cumulé |
|---|---|
| C1 | 1h |
| C2 | 30 min |
| C3 | 1-4h (variable, dépend des bugs) |
| Validation | 30 min |
| **Total** | **3-6h** |

### Check-list pré-bench (run 3)

- [ ] C1 validé : `<repo>/data/` n'existe plus, `instances.yaml` paths
      à jour, `parse_dimacs` propre sur type1+type2 (0 clause vide).
- [ ] C2 validé : test unitaire ou smoke avec une formule synthétique
      forçant l'overflow vs DAG simplifié.
- [ ] C3 partiel ou complet : les 7 segfaults reproduits localement,
      patchés ou désactivés explicitement avec notes.
- [ ] `make -C src asan` ne renvoie aucune erreur sanitizer sur 5+
      instances variées.
- [ ] `make -C benchmarks bench-smoke` : 0 FAIL hard, 0 segfault, 0
      `z3_short_circuit_empty_clause: true` sur les 3 instances du smoke.
- [ ] `git status` propre, push effectué.
- [ ] Tag git pré-run-3 (e.g. `bench-run-3`).

### Rappels post-bench

Comparer le SUMMARY du run 3 vs run 2 :
- 0 FAIL hard sur `dnnf_count_match` et `consistency_match_dp_z3` (C2)
- 0 ou peu de segfaults (C3)
- 0 `z3_short_circuit_empty_clause: true` sur type1/type2 (C1)
- `z3_conflicts_vs_pswidth.png` plus dense visuellement (C5)
- Plus de variations sharpsat sur type1/type2 (formules propres ≠
  toutes UNSAT)

---

*Document généré le 2026-05-03 à partir de l'audit du run
`20260502_162229`.*
