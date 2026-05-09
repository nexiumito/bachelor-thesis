# SPECS_QUERY_BENCH — Bench des requêtes sur DAG compilé (run 4)

Document de spécifications pour étendre le harness afin de mesurer le coût des
requêtes (CO/VA/CT/CE/IM/ME) sur un DAG d-DNNF **déjà compilé**, et produire
les figures supportant l'argument d'amortissement multi-requêtes pour la thèse.
Ne contient que des specs, pas de code. Format identique à
`SPECS_FIX_RUN1.md` / `SPECS_FIX_RUN2.md`.

Source : audit du run 3 (`benchmarks/results/20260503_071607/`, commit
`249ddd5`, 110 OK / 0 FAIL hard / 17 WARN soft) + analyse
`figures/ANALYSIS.md` §7 (limite explicite du `query_cost.pdf` actuel : proxy
via `time_phase3_ms`, inclut la construction du DAG) + chapitre "Pistes pour
la suite" du `checkpoint/benchmarks_recap.tex`.

**Prérequis** : `SPECS_QUERIES_PORT.md` (Q1..Q11) **doit être terminé** avant
toute entrée de ce document. Sans CE/IM/ME-multi portés et sans le flag
`--json-with-queries`, aucune mesure n'est possible.

---

## Sommaire

| ID | Titre | Sévérité | Effort | Dépend de |
|---|---|---|---|---|
| Q-B1 | Sanity check : flag `--json-with-queries` opérationnel | bloquant | S | SPECS_QUERIES_PORT |
| Q-B2 | `run_dp.py` — paramètre `with_queries` propage `--json-with-queries` | critique | S | Q-B1 |
| Q-B3 | Nouvelle passe C dans l'orchestrateur (in-process) | critique | L | Q-B2 |
| Q-B4 | Nouveaux invariants : I7 `entails_match_z3`, I8 `enumerate_count_match` | important | M | Q-B3 |
| Q-B5 | Plot **break-even N** (LE plot du run 4) | critique | M | Q-B3, Q-B4 |
| Q-B6 | Plot query-vs-Z3 par requête × famille (boxplot) | important | M | Q-B3 |
| Q-B7 | Plot µs/arête par requête (constantes empiriques) | moyen | S | Q-B3 |
| Q-B8 | Plot cache cold/warm sur requêtes répétées | faible | — | (volontairement non implémenté) |
| Q-B9 | Mise à jour `summary_template.md.j2` | important | M | Q-B5, Q-B6, Q-B7 |
| Q-B10 | Mise à jour `benchmark.yaml` (clé `passe_c`) | important | S | Q-B3 |
| Q-B11 | Mise à jour `START.md` (étape F.4) | important | S | Q-B3, Q-B5..Q-B7 |
| Q-B12 | Reproduction sur racer (notes spécifiques) | important | S | Q-B11 |

Notation effort : **S** = ≤ 30 min, **M** = 1–3 h, **L** = ½–1 jour.

---

## Avertissements préliminaires

### A1. Stabilité du run 3 — rétro-compatibilité impérative

Le run 3 (`commit 249ddd5d8a76`) est un **point de référence stable** (0 FAIL
hard, 17 WARN soft attendus). Toute modification du harness pour le run 4 :

- ne doit pas régresser la compatibilité du `structure.csv` / `z3.csv` /
  `timings.csv` du run 3 (rétro-compatible : ajouts en queue uniquement, comme
  le bug B6 a imposé pour `stderr_tail`) ;
- ne doit pas casser la cible `make -C benchmarks bench` actuelle ; la nouvelle
  phase est **opt-in** via `cfg.passe_c.enabled` (cf. Q-B10) ;
- doit conserver la même infrastructure d'isolation (RLIMIT_AS 50 GiB, nice 19,
  taskset CCDs, tmux, notifications Discord).

### A2. Décision architecturale clé : in-process vs cold start

Pour mesurer "le temps d'une requête sur DAG déjà compilé", deux options :

| Option | Description | Avantages | Inconvénients |
|---|---|---|---|
| **(a) In-process** | Un seul appel `./sat_solver <inst> greedy --json-with-queries` qui : (1) compile le DAG ; (2) chronomètre toutes les requêtes en mémoire avec `clock_gettime` ; (3) émet les timings dans le JSON. | Pas de coût de désérialisation. Mesures consistantes (même DAG en RAM). Aligné sur le scénario "compile une fois, interroge plein de fois". | Ne mesure pas le coût réel d'un usage offline (sérialiser le DAG, le recharger plus tard). |
| **(b) Cold start** | Phase 1 : `./sat_solver <inst> greedy --export-nnf out.nnf` ; Phase 2 : `./sat_query out.nnf consistency --json` (binaire séparé qui charge le NNF et chronomètre). | Mesure réaliste d'un workload "compile aujourd'hui, interroge demain" (cas KC classique). | Demande un parser NNF + un binaire `sat_query` à écrire. La désérialisation doit être chronométrée séparément (et son coût peut dominer les requêtes courtes). |

**Décision retenue : (a) in-process pour le run 4.** Justifications :

1. Coût d'implémentation minimal (Q8 du `SPECS_QUERIES_PORT.md` ajoute déjà
   tous les timings au JSON via `--json-with-queries`).
2. Le scénario d'usage le plus pertinent pour défendre la thèse est "le DAG est
   en RAM, on lui pose N requêtes". Le scénario cold start mesure quelque
   chose de différent (coût IO de désérialisation), pertinent mais distinct.
3. Le break-even N (Q-B5) n'a pas de sens en cold start : il faudrait amortir
   aussi le coût de désérialisation, brouillant le message.
4. Si pertinent en run 5, l'option (b) reste accessible : `dnnf_export_nnf`
   existe déjà ; il suffirait d'écrire `sat_query` (~200 lignes C).

### A3. Anti-biais démarrage process

Tous les timings de Q-B sont mesurés **après warm-up** :

- Pour les requêtes individuelles (`query_co_ms` etc.) : la 1ère exécution est
  jetée, médiane sur 4 exécutions suivantes (configurable via
  `cfg.passe_c.query_repetitions`, défaut 5).
- Pinning CPU sur les 4 CCDs distincts (héritage passe B).

### A4. Z3 baseline réutilisé

La comparaison query-vs-Z3 (Q-B5, Q-B6) requiert un appel Z3 `Solver` froid
pour chaque mesure. Le `z3.csv` du run 3 contient déjà ces données
(`z3_solve_ms`, `z3_total_ms`) ; on les réutilise. Pas besoin de relancer Z3
si on travaille sur les mêmes 110 instances OK du run 3.

---

## Q-B1 — Sanity check : flag `--json-with-queries` opérationnel

- **Sévérité** : bloquant
- **Effort** : S
- **Fichiers concernés** :
  - `bachelor-thesis/src/main.c` (déjà spécifié dans `SPECS_QUERIES_PORT.md` Q8.2)

### Diagnostic

Tout le bench query repose sur ce flag.

### Spec du fix

1. Vérifier que `SPECS_QUERIES_PORT.md` Q1..Q10 sont done.
2. Sanity check :
   ```bash
   cd src
   ./sat_solver ../data/exemple1.cnf greedy --json-with-queries | python3 -m json.tool
   ```
   - Doit produire un JSON contenant `query_co_ms`, `query_va_ms`,
     `query_ct_ms`, `query_me_ms`, `query_ce_ms`, `query_im_ms`,
     `query_enum_first_ms`, `query_enum_all_ms`, `query_enum_count`,
     `query_repetitions`, `query_enum_all_skipped`.

### Validation

```bash
cd src
./sat_solver ../data/exemple1.cnf greedy --json-with-queries \
  | python3 -m json.tool | grep -c "^    \"query_"
# Attendu : >= 14 (10 timings + 4 results + count + count_skipped + repetitions)
```

### Dépendances

`SPECS_QUERIES_PORT.md` complet.

---

## Q-B2 — `run_dp.py` : paramètre `with_queries` propage `--json-with-queries`

- **Sévérité** : critique
- **Effort** : S
- **Fichiers concernés** :
  - `bachelor-thesis/benchmarks/runners/run_dp.py` (ligne 91 actuellement
    `cmd_parts += [f"./{Path(binary_path).name}", instance.path, mode, "--json"]`)

### Diagnostic

Le runner actuel construit la commande `./sat_solver <path> <mode> --json`. Il
faut un mode optionnel `with_queries=True` qui passe `--json-with-queries` à la
place.

### Spec du fix

1. Ajouter un paramètre `with_queries: bool = False` à la signature de
   `run_dp(...)` (après `repo_root: str = "."`).
2. Dans la construction de `cmd_parts` (ligne 91), remplacer le dernier élément :
   ```python
   json_flag = "--json-with-queries" if with_queries else "--json"
   cmd_parts += [f"./{Path(binary_path).name}", instance.path, mode, json_flag]
   ```
3. Le dict retourné contient automatiquement les nouveaux champs (le code
   parse `data = json.loads(json_lines[-1])` et fait `enriched = dict(data)`).
4. Documenter dans la docstring : "Si with_queries=True, le solveur compile le
   DAG ET chronomètre chaque requête (CO/VA/CT/CE/IM/ME/enumerate) en interne.
   Les timings sont retournés dans les champs `query_*_ms`. Coût supplémentaire
   par appel : O(query_repetitions × |D|) par requête. À utiliser uniquement
   pour la passe query-bench (Q-B3)."

### Validation

```bash
cd benchmarks
PYTHONPATH=. python3 -c "
from runners.run_dp import run_dp, _MinimalInstance
r = run_dp(_MinimalInstance(id='exemple1', path='../data/exemple1.cnf'),
           'greedy', 0, 60, 50*1024**3, with_queries=True, repo_root='..')
print({k:v for k,v in r.items() if k.startswith('query_')})
"
# Attendu : dict des query_*_ms et query_*_result non vides
```

### Dépendances

Q-B1.

---

## Q-B3 — Nouvelle passe C dans l'orchestrateur

- **Sévérité** : critique (cœur du run 4)
- **Effort** : L (nouvelle phase complète : task builder, worker, append CSV, dispatch CLI, heartbeat)
- **Fichiers concernés** :
  - `bachelor-thesis/benchmarks/orchestrator.py`
    - `STRUCTURE_CSV_COLS` (ligne 169) — ajout en queue
    - nouvelle fonction `run_passe_c` (calquée sur `run_passe_b` ligne 921)
    - `main()` (ligne 1152+) — dispatch
  - `bachelor-thesis/benchmarks/config/benchmark.yaml` (Q-B10)

### Diagnostic

Le pipeline actuel a passes A (parallèle, structurel) et B (4 CCDs, timings
purs DP+phases). Pour mesurer le coût des requêtes sur DAG compilé in-process
(décision A2.a), on ajoute une **passe C** dédiée :

- **Scope** : mode `greedy` uniquement (le seul mode dont le DAG est
  compétitif). Pas de `linear` (génère des DAGs gigantesques inutilisables en
  bench query).
- **Subset d'instances** : on filtre celles dont la passe A a OK + dont
  `dnnf_nodes < cfg.passe_c.dnnf_nodes_max` (configurable, défaut 1e6).
- **Isolation** : 4 CCDs distincts (héritage passe B), 5 répétitions internes
  par requête (anti-biais A3 ; le solveur fait déjà ses 5 répétitions au sein
  du JSON, l'orchestrateur ne fait qu'une exécution par instance).

### Spec du fix

1. Ajouter dans `STRUCTURE_CSV_COLS` (orchestrator.py:169) **en queue**
   (rétro-compat impérative, B6 reference) les colonnes :
   ```python
   "query_co_ms", "query_va_ms", "query_ct_ms", "query_me_ms",
   "query_ce_ms", "query_im_ms", "query_enum_first_ms", "query_enum_all_ms",
   "query_enum_count", "query_co_result", "query_va_result",
   "query_ce_result", "query_im_result", "query_ct_count",
   "query_repetitions", "query_enum_all_skipped",
   ```
   Note : `query_*` reste **vide** pour les lignes des passes A et B
   (rétro-compat). Seules les lignes de la passe C les renseignent.
2. Créer une nouvelle fonction
   `run_passe_c(instances, cfg, repo_root, output_dir, state, shutdown_event, inst_by_id)`
   qui :
   - Filtre les instances : seules celles dont la passe A est OK avec mode
     greedy ET `dnnf_nodes < cfg.passe_c.dnnf_nodes_max` (lecture de
     `structure.csv` déjà écrit par passe A).
   - Lance pour chaque instance un appel
     `run_dp(instance, mode='greedy', seed=0, with_queries=True, ...)` sur
     l'un des 4 CCDs (round-robin, comme passe B).
   - Append le résultat à `structure.csv` avec `runner='dp_query'` (nouveau
     marqueur pour distinguer les lignes passe C des lignes passe A).
   - Heartbeat propre, notifier Discord en cas de FAIL hard.
3. Ajouter une nouvelle entrée CLI `--only-passe-c` (uniformément avec
   `--only-passe-a`, `--only-passe-b`) dans `main()`.
4. Le dispatch global (`main()` ligne 1152+) appelle `run_passe_c` après
   `run_passe_b`, conditionné à `cfg.passe_c.enabled` (défaut `true` quand la
   config existe).
5. **Cible Makefile** : ajouter à `benchmarks/Makefile` une nouvelle cible
   `bench-query` qui appelle `python3 orchestrator.py --only-passe-c
   --output benchmarks/results/<LATEST>` (parallèle aux cibles existantes
   `bench`, `bench-smoke`, `bench-resume`).

### Validation

```bash
# Apres une passe A OK :
python3 benchmarks/orchestrator.py --only-passe-c \
        --output benchmarks/results/<TIMESTAMP>

# Verification : structure.csv contient des lignes runner='dp_query' avec
# tous les query_*_ms renseignes.
awk -F, 'NR==1 {for (i=1;i<=NF;i++) if ($i=="runner") rcol=i; \
                for (i=1;i<=NF;i++) if ($i=="query_co_ms") qcol=i} \
         NR>1 && $rcol=="dp_query" {print $1, $2, $qcol}' \
    benchmarks/results/<TIMESTAMP>/structure.csv | head -5

# bench-smoke etendu : ajouter une verif que la passe C tourne sur exemple1
# + 2 instances < 50 ms.
make -C benchmarks bench-smoke
```

### Dépendances

Q-B2.

---

## Q-B4 — Nouveaux invariants : I7 `entails_match_z3`, I8 `enumerate_count_match`

- **Sévérité** : important (filet de sécurité contre régression silencieuse)
- **Effort** : M
- **Fichiers concernés** :
  - `bachelor-thesis/benchmarks/runners/invariants.py` (lignes 1–32 pour la
    docstring + `INVARIANTS_CSV_COLS`, ajouter 2 fonctions `_check_*`)

### Diagnostic

Le run 3 a 6 invariants I1..I6. Avec les nouvelles requêtes, deux propriétés
théoriques deviennent vérifiables empiriquement :

- **I7 — `entails_match_z3`** : pour les lignes `runner='dp_query'`, vérifier
  que `query_ce_result == 1` (YES). Justification : par construction, `CE` est
  exécuté dans le solveur sur la 1ère clause de F (cf. `SPECS_QUERIES_PORT.md`
  Q8.2), et F entraîne trivialement chacune de ses propres clauses. Si I7
  FAIL, c'est un bug de `dnnf_entails`.
- **I8 — `enumerate_count_match`** : `query_enum_count == dnnf_count_recomputed`
  quand `query_enum_all_skipped == False`. Hard. Vérifie l'invariant Lemme A.3
  Darwiche-Marquis (ME ↔ CT sur DAG lisse). Si la vérification échoue, c'est
  un signal fort que le DAG n'est pas lisse ou que `enumerate` a un bug.

### Spec du fix

1. Mettre à jour la docstring (lignes 1–15) pour mentionner I7 et I8.
2. Ajouter deux fonctions :
   ```python
   def _check_enumerate_count_match(row: dict[str, Any]) -> _Check:
       # Skip si pas de bench query (passes A/B sans query_*)
       if not row.get("query_enum_count"):
           return _Check(..., status="SKIPPED", message="pas de bench query")
       skipped = _parse_bool(row.get("query_enum_all_skipped"))
       if skipped:
           return _Check(..., status="SKIPPED",
                         message="enumerate cape (>1e6 modeles)")
       enum_count = _parse_int(row["query_enum_count"])
       dnnf_count = _parse_int(row.get("dnnf_count_recomputed"))
       if enum_count is None or dnnf_count is None:
           return _Check(..., status="SKIPPED", message="parse error")
       if enum_count == dnnf_count:
           return _Check(..., status="OK", expected=str(dnnf_count),
                         observed=str(enum_count), message="")
       return _Check(..., status="FAIL", severity="hard",
                     expected=str(dnnf_count), observed=str(enum_count),
                     message=f"enumerate produit {enum_count} modeles vs "
                             f"dnnf_count={dnnf_count}")

   def _check_entails_match_z3(row: dict[str, Any], z3_sat_row: dict[str, Any] | None) -> _Check:
       # Skipper sauf si le row vient de la passe C (runner='dp_query').
       # Pour I7, on sait que le solveur a interroge CE sur la 1ere clause
       # de F (cf. SPECS_QUERIES_PORT.md Q8.2 (3)). F entraine trivialement
       # ses propres clauses, donc query_ce_result doit etre 1 (YES).
       if row.get("runner") != "dp_query":
           return _Check(..., status="SKIPPED", message="passe non-query")
       ce = _parse_int(row.get("query_ce_result"))
       if ce is None:
           return _Check(..., status="SKIPPED", message="pas de query_ce_result")
       if ce == 1:
           return _Check(..., status="OK", expected="1 (YES)",
                         observed="1", message="")
       return _Check(..., status="FAIL", severity="hard",
                     expected="1 (YES, F entraine sa propre 1ere clause)",
                     observed=str(ce),
                     message="contradiction theorique : F n'entraine pas sa propre 1ere clause")
   ```
3. Brancher dans la boucle `_run_all_invariants` :
   ```python
   _check_enumerate_count_match(row),
   _check_entails_match_z3(row, z3_sat_row),
   ```

### Validation

```bash
# Apres une passe C complete, invariants.csv contient des lignes pour I7 et I8.
grep -c "entails_match_z3\|enumerate_count_match" \
    benchmarks/results/<TIMESTAMP>/invariants.csv
# Attendu : > 0

# Sur les 110 instances OK du run 3 (a reproduire sur run 4) :
# I8 doit etre 100% OK (sauf SKIPPED pour les instances cap par
#    query_enum_all_skipped).
# I7 doit etre 100% OK trivialement (F entraine ses propres clauses).
awk -F, '$4=="entails_match_z3" {print $6}' \
    benchmarks/results/<TIMESTAMP>/invariants.csv | sort | uniq -c
# Attendu : que des OK et SKIPPED, pas de FAIL.
```

### Dépendances

Q-B3.

---

## Q-B5 — Plot **break-even N** (LE plot du run 4)

- **Sévérité** : critique (argument scientifique principal)
- **Effort** : M
- **Fichiers concernés** :
  - `bachelor-thesis/benchmarks/plots/breakeven_n.py` (à créer)
  - `bachelor-thesis/benchmarks/plots/make_all_plots.py` (ajouter l'appel)
  - `bachelor-thesis/benchmarks/plots/summary_template.md.j2` (Q-B9)

### Diagnostic

Argument central pour défendre le DP : il existe un seuil $N^*$ tel que pour
$N \geq N^*$ requêtes sur la même formule, la stratégie DP+queries devient
plus rapide que $N$ appels Z3 froids. Cette valeur quantifie l'amortissement.

### Spec du fix

#### Q-B5.1 — Modèle de coût

Pour une instance donnée, on compare :

- **Stratégie DP** : coût total = `time_total_ms` (passe B, médiane greedy ;
  inclut compile DAG = phases 0+1+2+3) + $N \times T_{\text{query}}$ où
  $T_{\text{query}}$ = temps médian d'une requête.
- **Stratégie Z3** : coût total = $N \times$ `z3_solve_ms` (passe A, médiane
  Z3 froide).

L'intersection donne :
$$
N^* = \left\lceil \frac{\text{time\_total\_ms}_{\text{DP}}}{\text{z3\_solve\_ms} - T_{\text{query}}} \right\rceil
\quad \text{si } \text{z3\_solve\_ms} > T_{\text{query}}
$$

Si `z3_solve_ms < T_query` (Z3 plus rapide qu'une requête sur DAG, cas
pathologique des grands DAG), $N^* = \infty$ (la stratégie DP n'est jamais
rentable). À reporter.

#### Q-B5.2 — Variantes de plot

Produire un seul fichier PDF `breakeven_n.pdf` avec **2 sous-figures côte à
côte** :

- **Sous-figure 1 — Courbe de coût pour 5 instances types** (1 par famille :
  type1, type2, type3, random, tseytin) :
  - Axes : $N$ (1 à 1000, log) en x, coût total ms en y (log).
  - 2 courbes par instance : DP+queries (croissante en $N$, pente =
    $T_{\text{query}}$) et $N \times$ Z3 (linéaire en $N$, pente =
    `z3_solve_ms`).
  - Marqueur sur le point d'intersection ($N^*$).
  - Choix de $T_{\text{query}}$ : `query_co_ms` (la requête la plus rapide à
    mesurer, et la plus représentative d'un usage SAT decision).

- **Sous-figure 2 — Distribution des $N^*$ sur toutes les instances OK** :
  - Histogramme cumulatif (ECDF) : pour quel pourcentage des instances le DP
    devient-il rentable à $N=1, 10, 100, 1000$ requêtes.
  - Stats annotées : médiane, quartiles, % d'instances avec $N^* < 100$,
    % avec $N^* = \infty$.

#### Q-B5.3 — Détails techniques d'implémentation

```python
# benchmarks/plots/breakeven_n.py
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from . import _common

def make(input_dir, output_dir, config):
    structure = _common.load_csv(input_dir / "structure.csv")
    z3 = _common.load_csv(input_dir / "z3.csv")

    # Lignes de la passe C (runner='dp_query', mode='greedy')
    dp_q = structure[(structure["runner"] == "dp_query") &
                     (structure["status"] == "ok") &
                     (structure["mode"] == "greedy")].copy()

    # Médiane DP de la passe B (timings purs)
    timings = _common.load_csv(input_dir / "timings.csv")
    dp_b = timings[(timings["runner"] == "dp") &
                   (timings["mode"] == "greedy")][
                   ["instance_id", "time_total_ms_median"]]

    # Z3 froid de la passe A
    z3_sat = z3[z3["z3_kind"] == "sat"][["instance_id", "z3_solve_ms"]]

    df = dp_q.merge(dp_b, on="instance_id").merge(z3_sat, on="instance_id")
    df["t_query"]    = pd.to_numeric(df["query_co_ms"], errors="coerce")
    df["dp_compile"] = pd.to_numeric(df["time_total_ms_median"], errors="coerce")
    df["z3_solve"]   = pd.to_numeric(df["z3_solve_ms"], errors="coerce")

    df["n_star"] = np.where(
        df["z3_solve"] > df["t_query"],
        np.ceil(df["dp_compile"] / (df["z3_solve"] - df["t_query"])),
        np.inf
    )

    # Sous-figure 1 : 5 instances types (1 par famille).
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(11, 4.5))
    for fam in ["type1", "type2", "type3", "random", "tseytin"]:
        sub = df[df["family"] == fam]
        if sub.empty: continue
        med_row = sub.iloc[len(sub) // 2]   # mediane representative
        N = np.geomspace(1, 1000, 100)
        cost_dp = med_row["dp_compile"] + N * med_row["t_query"]
        cost_z3 = N * med_row["z3_solve"]
        ax1.plot(N, cost_dp, "-", color=_common.color_for(fam), label=f"DP {fam}")
        ax1.plot(N, cost_z3, "--", color=_common.color_for(fam), alpha=0.6)
        if med_row["n_star"] != np.inf:
            ax1.axvline(med_row["n_star"], color=_common.color_for(fam),
                        alpha=0.3, linestyle=":")
    ax1.set_xscale("log"); ax1.set_yscale("log")
    ax1.set_xlabel("N (nombre de requetes)")
    ax1.set_ylabel("Cout total (ms)")
    ax1.set_title("Cout total : DP+queries vs N x Z3")
    ax1.legend()

    # Sous-figure 2 : ECDF des N*.
    finite = df[df["n_star"] != np.inf]["n_star"].sort_values()
    ax2.step(finite, np.arange(1, len(finite)+1) / len(df), where="post")
    ax2.set_xscale("log")
    ax2.set_xlabel("N*")
    ax2.set_ylabel("Proportion d'instances avec N break-even <= x")
    ax2.set_title(f"Distribution des N* "
                  f"(mediane={finite.median():.0f}, "
                  f"{(df['n_star']!=np.inf).sum()}/{len(df)} finies)")

    _common.save_plot(fig, output_dir, "breakeven_n",
                      formats=tuple(config.get("formats", ["pdf", "png"])),
                      dpi_png=int(config.get("dpi_png", 200)))
```

### Validation

```bash
# Apres run 4 OK :
ls benchmarks/results/<RUN4>/figures/breakeven_n.pdf
# Attendu : fichier present, lisible (open ou xdg-open).

# Sanity manuel : la mediane des N* finis doit etre dans [10, 1000] sur les
# instances type1/type2/type3 ou le DP est lent a compiler mais ses requetes
# sont rapides. Pour tseytin (DAG enorme, requete lente), N* peut etre infini
# partout — c'est attendu et c'est l'argument inverse (le DP n'est pas
# magique, sur tseytin il ne passe jamais devant Z3).
```

### Dépendances

Q-B3, Q-B4 (validation que les données sont fiables avant de produire le plot).

---

## Q-B6 — Plot query-vs-Z3 par requête × famille (boxplot)

- **Sévérité** : important (lecture directe du speedup)
- **Effort** : M
- **Fichiers concernés** :
  - `bachelor-thesis/benchmarks/plots/query_vs_z3.py` (à créer)
  - `bachelor-thesis/benchmarks/plots/make_all_plots.py` (ajouter l'appel)

### Diagnostic

Pour chaque requête CO/VA/CT/CE/IM/ME (premier modèle), quel est le speedup
`z3_solve_ms / query_*_ms` ? Boxplot par famille pour visualiser la
distribution.

### Spec du fix

1. Boxplot horizontal :
   - Axe x : ratio `z3_solve_ms / query_X_ms` (log).
   - Axe y : combinaisons (requête, famille) — 6 requêtes × 5 familles = 30
     lignes max (selon données disponibles).
   - Couleur par famille (réutilise `_common.color_for`).
   - Ligne verticale `x=1` annotée : "égalité Z3 = query".
   - Au-dessus de `x=1` → query plus rapide (DP gagne) ; en-dessous → Z3 plus
     rapide même sur DAG compilé.
2. Annotation : pourcentage d'instances dans chaque cas
   (DP gagne / égal / Z3 gagne) par requête.
3. Sortie : `figures/query_vs_z3.pdf`.

```python
# benchmarks/plots/query_vs_z3.py
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from . import _common

def make(input_dir, output_dir, config):
    structure = _common.load_csv(input_dir / "structure.csv")
    z3 = _common.load_csv(input_dir / "z3.csv")

    dp_q = structure[(structure["runner"] == "dp_query") &
                     (structure["status"] == "ok")]
    z3_sat = z3[z3["z3_kind"] == "sat"][["instance_id", "z3_solve_ms"]]
    df = dp_q.merge(z3_sat, on="instance_id")

    QUERIES = ["co", "va", "ct", "me", "ce", "im"]
    rows = []
    for q in QUERIES:
        for family in df["family"].unique():
            sub = df[df["family"] == family].copy()
            sub["t_q"] = pd.to_numeric(sub[f"query_{q}_ms"], errors="coerce")
            sub["t_z3"] = pd.to_numeric(sub["z3_solve_ms"], errors="coerce")
            sub = sub.dropna(subset=["t_q", "t_z3"])
            sub = sub[sub["t_q"] > 0]
            for r in (sub["t_z3"] / sub["t_q"]).values:
                rows.append({"query": q.upper(), "family": family, "ratio": r})

    plot_df = pd.DataFrame(rows)
    fig, ax = plt.subplots(figsize=(8, 8))
    plot_df["label"] = plot_df["query"] + " - " + plot_df["family"]
    plot_df.boxplot(column="ratio", by="label", ax=ax, vert=False)
    ax.axvline(1, color="red", linestyle="--", alpha=0.7,
               label="Z3 = query (egalite)")
    ax.set_xscale("log")
    ax.set_xlabel("Ratio z3_solve_ms / query_ms (>1 = DP plus rapide)")
    ax.set_title("Speedup par requete x famille")
    ax.legend()

    _common.save_plot(fig, output_dir, "query_vs_z3",
                      formats=tuple(config.get("formats", ["pdf", "png"])))
```

### Validation

```bash
ls benchmarks/results/<RUN4>/figures/query_vs_z3.pdf
# Attendu : fichier present, lisible.
# Lecture qualitative attendue : sur type1/type3 petits, ratio > 100 (DP gagne
# largement). Sur tseytin et grandes random difficiles, ratio < 1 (Z3 gagne
# meme contre une requete sur DAG, car DAG enorme).
```

### Dépendances

Q-B3.

---

## Q-B7 — Plot µs/arête par requête (constantes empiriques)

- **Sévérité** : moyenne (intéressant pour la thèse, pas central)
- **Effort** : S
- **Fichiers concernés** :
  - `bachelor-thesis/benchmarks/plots/query_per_edge.py` (à créer)
  - `bachelor-thesis/benchmarks/plots/make_all_plots.py` (ajouter l'appel)

### Diagnostic

Question explicitement posée par l'utilisateur : « regarder le temps que met
chaque requête à s'exécuter sur un DAG compilé, pour voir si y'en a des plus
rapides que d'autres (car on sait que tout est en O(|D|), mais c'est
intéressant si on voit que CO est plus rapide que IM par exemple) ». Réponse
attendue qualitativement : OUI, IM (≈ smooth + condition + count = 3 passes)
> CE (condition + count = 2 passes) > CT/CO/VA (1 passe count) > ME 1-modèle
(1 passe count + 1 passe descend = 2 passes mais avec mémoïsation).

### Spec du fix

1. Pour chaque requête, calculer `T_query / |D|` (microsecondes par arête) sur
   toutes les instances OK.
2. Bar chart horizontal :
   - Axe y : 7 requêtes (CO, VA, CT, ME-find, CE, IM, ME-enum-first).
   - Axe x : médiane de `T_query / |D|` en µs/arête (log).
   - Whiskers : Q1, Q3.
3. Annotation : pour chaque requête, le facteur multiplicatif vs CO
   (= référence "1×").
4. Sortie : `figures/query_per_edge.pdf`.

### Validation

```bash
ls benchmarks/results/<RUN4>/figures/query_per_edge.pdf
# Attendu : fichier present.
# L'ordre attendu (du plus rapide au plus lent) : CO ≈ CT ≈ VA < ME-find <
# CE < IM. Si l'ordre observe est tres different, deboguer (probable cause :
# dnnf_count_table allocation dans find_model/enumerate — cout memoire
# dominant).
```

### Dépendances

Q-B3.

---

## Q-B8 — Plot cache cold/warm sur requêtes répétées (volontairement non implémenté)

- **Sévérité** : faible
- **Effort** : — (décision : NON implémenté en run 4)
- **Fichiers concernés** : N/A

### Diagnostic

L'utilisateur a évoqué la possibilité d'un plot cache cold/warm. Justification
théorique : la 1ère requête sur un DAG fraîchement compilé peut être plus lente
(cache CPU froid) que la 5ème répétition (cache chaud). Si l'écart est
significatif, cela informe le scénario d'usage (mode batch vs interactif).

### Spec du fix

**Décision : NON implémenté en run 4.** Justifications :

- Le mode `--json-with-queries` (Q8.2 de `SPECS_QUERIES_PORT.md`) jette déjà la
  1ère exécution et garde la médiane des 4 suivantes. L'effet cache cold est
  **structurellement masqué** par le warm-up.
- Pour le mesurer, il faudrait modifier le solveur pour exposer
  `query_co_first_ms` (1ère exécution) **en plus de** `query_co_ms` (médiane
  des 4 suivantes). C'est une modification non triviale du JSON et du code C.
- Mentionner explicitement dans le SUMMARY.md (Q-B9) que ce point est
  volontairement écarté pour rester focus sur le break-even (argument
  principal). À envisager en run 5 si le run 4 démontre que l'argument
  query-amortissement est solide.

### Validation

N/A.

### Dépendances

N/A.

---

## Q-B9 — Mise à jour du `summary_template.md.j2`

- **Sévérité** : important (lisibilité du rapport généré)
- **Effort** : M
- **Fichiers concernés** :
  - `bachelor-thesis/benchmarks/plots/summary_template.md.j2`
  - `bachelor-thesis/benchmarks/plots/make_summary.py` (passer les nouvelles
    données au template)

### Diagnostic

Le SUMMARY.md actuel n'a aucune section sur les requêtes. À étendre pour
rendre lisible le verdict du run 4.

### Spec du fix

1. Ajouter au template Jinja2, **après** la section "Comparaison DP vs Z3 par
   famille" (vers ligne 60) :
   ```jinja2
   ## Bench des requêtes sur DAG compilé (passe C)

   > Mesure du temps d'exécution des requêtes CO/VA/CT/CE/IM/ME (1 modèle /
   > énumération complète) sur DAG **déjà compilé** (in-process, médiane sur
   > 5 répétitions). Ce bench supporte l'argument d'amortissement
   > multi-requêtes (cf. plot break-even N).

   ### Médiane par requête (toutes familles confondues)

   | Requête | Médiane (µs) | Médiane µs/arête | Speedup vs Z3 (médiane) |
   |---|---:|---:|---:|
   {% for q in query_stats -%}
   | {{ q.name }} | {{ q.median_us }} | {{ q.median_us_per_edge }} | {{ q.speedup_vs_z3 }} |
   {% endfor %}

   ### Break-even N

   - Médiane N* (sur instances avec N* fini) : **{{ breakeven_median }}**
   - {{ breakeven_finite_count }} / {{ total_instances_query }} instances ont N* fini
   - Recommandation pratique : si l'utilisateur prévoit
     ≥ {{ breakeven_median }} requêtes par formule, le DP est rentable face à
     Z3 (en moyenne).

   ### Note méthodologique

   Le scénario "cache cold vs warm" (1ère requête vs 5ème) n'est pas mesuré
   dans ce run : le warm-up structurel (1ère exécution jetée) masque
   volontairement l'effet pour rester focus sur le break-even. À envisager
   en run 5 si pertinent.
   ```

2. Étendre la section "Figures" avec :
   ```
   - [breakeven_n.pdf](figures/breakeven_n.pdf) — courbe de coût et distribution des N*
   - [query_vs_z3.pdf](figures/query_vs_z3.pdf) — speedup par requête × famille
   - [query_per_edge.pdf](figures/query_per_edge.pdf) — coût empirique µs/arête par requête
   ```

3. Étendre `make_summary.py` pour calculer `query_stats`, `breakeven_median`,
   `breakeven_finite_count`, `total_instances_query` à partir de
   `structure.csv` (lignes `runner='dp_query'`).

### Validation

```bash
python3 benchmarks/plots/make_summary.py \
        --input benchmarks/results/<RUN4>
# Attendu : SUMMARY.md contient les nouvelles sections, valeurs coherentes
# avec les CSV.

grep -c "breakeven\|Bench des requetes" \
    benchmarks/results/<RUN4>/SUMMARY.md
# Attendu : >= 2
```

### Dépendances

Q-B5, Q-B6, Q-B7.

---

## Q-B10 — Mise à jour de `benchmark.yaml`

- **Sévérité** : important (configuration opt-in)
- **Effort** : S
- **Fichiers concernés** :
  - `bachelor-thesis/benchmarks/config/benchmark.yaml`

### Diagnostic

La nouvelle passe C doit être configurable (enabled, dnnf_nodes_max,
repetitions internes, cap d'énumération).

### Spec du fix

1. Ajouter à `benchmark.yaml` une nouvelle section :
   ```yaml
   # ----------------------------------------------------------------------------
   # Passe C : bench des requetes sur DAG compile (in-process, run 4+)
   # ----------------------------------------------------------------------------
   passe_c:
     enabled: true
     # Filtre instances : seules celles dont le DAG est < cette taille (au-dela,
     # les requetes prennent trop de temps et le bench derive).
     dnnf_nodes_max: 1000000
     # Nombre de repetitions internes par requete (le solveur jette la 1ere et
     # garde la mediane des 4 suivantes par defaut).
     query_repetitions: 5
     # Cap sur dnnf_enumerate(all) : si dnnf_count > ce seuil, skip enumerate(all)
     # (sinon le binaire mettrait des heures sur des instances a 2^n modeles).
     enumerate_count_cap: 1000000
     default_timeout_s: 600
     # passe_c_taskset_cpus n'est pas duplique : reuse machine.passe_b_taskset_cpus.
   ```
2. Ne pas dupliquer `passe_b_taskset_cpus` ; lire la liste depuis
   `machine.passe_b_taskset_cpus` dans l'orchestrateur.

### Validation

```bash
python3 -c "import yaml; yaml.safe_load(open('benchmarks/config/benchmark.yaml'))"
# Attendu : succes (pas d'erreur YAML).

# L'orchestrateur (Q-B3) lit cfg.passe_c.enabled et cfg.passe_c.dnnf_nodes_max
# correctement (verifier via --dry-run).
python3 benchmarks/orchestrator.py --only-passe-c --dry-run \
        --output /tmp/dry-test
```

### Dépendances

Q-B3.

---

## Q-B11 — Mise à jour de `START.md` (étape F.4)

- **Sévérité** : important
- **Effort** : S
- **Fichiers concernés** :
  - `bachelor-thesis/benchmarks/START.md`

### Diagnostic

`START.md` documente la procédure de lancement sur racer. À étendre pour
mentionner la passe C (entre les étapes F lancement et G récupération).

### Spec du fix

1. Ajouter une nouvelle étape **F.4 — Passe C (bench query)** :
   ```markdown
   ### F.4 — Passe C (bench des requêtes sur DAG compilé)

   La passe C est lancée automatiquement après les passes A et B si
   `passe_c.enabled: true` dans `benchmark.yaml` (défaut). Pour la lancer
   isolément :

       python3 benchmarks/orchestrator.py --only-passe-c \
               --output benchmarks/results/<TIMESTAMP>

   Durée typique : ~30 min sur racer (110 instances OK greedy filtrées par
   `dnnf_nodes_max=1e6` ; 1 appel solveur par instance, 5 répétitions internes
   par requête).

   Ressources : utilise les 4 CCDs (CPU 0/8/16/24 par défaut, hérite de
   `machine.passe_b_taskset_cpus`). RLIMIT_AS 50 GiB par process (idem passe B).

   Cible Make alternative : `make -C benchmarks bench-query`.
   ```
2. Mettre à jour la section H (post-run) pour mentionner les 3 nouveaux plots
   et la nouvelle section SUMMARY.md "Bench des requêtes sur DAG compilé".

### Validation

Lecture humaine : la procédure de lancement complète (A → H) couvre maintenant
les 3 passes A/B/C sans ambiguïté.

```bash
grep -c "passe_c\|passe C\|F.4" benchmarks/START.md
# Attendu : >= 3
```

### Dépendances

Q-B3, Q-B5..Q-B7.

---

## Q-B12 — Reproduction sur racer (notes spécifiques)

- **Sévérité** : important (reproductibilité)
- **Effort** : S
- **Fichiers concernés** :
  - (mémo opérationnel — pas de fichier dédié, à intégrer dans START.md F.4)

### Diagnostic

La passe C ne doit pas surcharger racer ni interférer avec les autres
utilisateurs. Hériter strictement des contraintes existantes
(`benchmarks_recap.tex` chap. "Infrastructure") : tmux, RLIMIT_AS 50 GiB,
nice 19, taskset CCDs distincts.

### Spec du fix

1. Procédure exacte de lancement du run 4 sur racer :
   ```bash
   ssh ebussod@racer
   tmux new -s bench-run4
   cd ~/bachelor/bachelor-thesis
   git pull   # contient SPECS_QUERIES_PORT.md applique
   make -C src rebuild
   source .venv-bench/bin/activate
   make -C benchmarks bench   # passes A + B + C en cascade
   # Ctrl-b puis d pour detacher
   ```
2. Surveillance : heartbeat Discord toutes les 30 min reste actif (B7 du run 1
   a éliminé le rate-limit 429).
3. Récupération sur la machine de dev :
   ```bash
   LATEST=$(ssh ebussod@racer "ls -t bachelor/bachelor-thesis/benchmarks/results/" | head -1)
   mkdir -p benchmarks/results/$LATEST
   rsync -av --progress \
       ebussod@racer:bachelor/bachelor-thesis/benchmarks/results/$LATEST/ \
       benchmarks/results/$LATEST/
   xdg-open benchmarks/results/$LATEST/SUMMARY.md
   ```
4. Audit en priorité :
   - Section "Bench des requêtes" du SUMMARY.md non vide.
   - `figures/breakeven_n.pdf` produit, médiane $N^*$ dans une plage
     raisonnable (10..1000 attendu).
   - `invariants.csv` : I7 et I8 (nouveaux, Q-B4) à 100% OK.
   - Aucun nouveau FAIL hard sur I1..I6 (régression interdite).

### Validation

```bash
# Run 4 OK :
# - 0 FAIL hard
# - SUMMARY.md complet (sections "Vue globale", "Invariants", ..., "Bench des
#   requetes sur DAG compile")
# - 3 nouveaux plots dans figures/
# - Mediane N* annoncee dans le SUMMARY

# Comparaison avec run 3 :
python3 -c "
import csv
def count_ok(path):
    with open(path) as f:
        return sum(1 for r in csv.DictReader(f)
                   if r['status'] == 'OK' and r['severity'] == 'hard')
old = count_ok('benchmarks/results/20260503_071607/20260503_071607/invariants.csv')
new = count_ok('benchmarks/results/<RUN4>/invariants.csv')
print(f'Hard OK : {old} -> {new} (delta={new-old})')
# Attendu : delta >= 0 (au moins egal, idealement +N car I7+I8 ajoutent des OK)
"
```

### Dépendances

Q-B11.

---

## DAG des dépendances

```
SPECS_QUERIES_PORT.md (complet)
 └─→ Q-B1 (sanity --json-with-queries)
      └─→ Q-B2 (run_dp.py with_queries)
           └─→ Q-B3 (passe C dans orchestrator)
                ├─→ Q-B4 (invariants I7, I8)
                ├─→ Q-B5 (plot breakeven)
                ├─→ Q-B6 (plot query vs z3)
                ├─→ Q-B7 (plot µs/arete)
                └─→ Q-B10 (benchmark.yaml)
                     └─→ Q-B9 (summary template)
                          └─→ Q-B11 (START.md)
                               └─→ Q-B12 (reproduction)

Q-B8 : volontairement non implementee (cf. justification).
```

Ordre d'exécution recommandé :
**Q-B1 → Q-B2 → Q-B3 → Q-B4 → Q-B10 → (Q-B5 ∥ Q-B6 ∥ Q-B7) → Q-B9 → Q-B11 → Q-B12**.

Q-B5/Q-B6/Q-B7 indépendants entre eux (3 plots distincts), parallélisables
après Q-B3.

---

## Annexe : ce que le run 4 ajoute par rapport au run 3 (vue d'ensemble)

| Élément | Run 3 (état actuel) | Run 4 (cible) |
|---|---|---|
| Requêtes implémentées | CO, VA, CT, ME-find_model | + CE, IM, ME-enumerate (via `SPECS_QUERIES_PORT.md`) |
| Mesure du coût des requêtes | Proxy `time_phase3_ms` (inclut compile) | Mesure directe `query_*_ms` sur DAG compilé in-process |
| Argument scientifique principal | "DP n'est pas compétitif one-shot" | + "DP rentre dans ses frais à partir de N=N* requêtes" |
| Plots | 9 (`time_vs_pswidth`, `dp_vs_z3_maxsat`, `dag_size_vs_bound`, `greedy_vs_linear*`, `phase_breakdown`, `query_cost`, `pswidth_vs_theory`, `z3_conflicts_vs_pswidth`) | 9 + 3 nouveaux : `breakeven_n.pdf` (LE plot), `query_vs_z3.pdf`, `query_per_edge.pdf` |
| Invariants | I1..I6 (6) | I1..I8 (8) : + `entails_match_z3`, + `enumerate_count_match` |
| Schémas CSV | `STRUCTURE_CSV_COLS` (32 colonnes) | + 16 colonnes `query_*` en queue (rétro-compat) |
| Passes | A (parallèle 64) + B (4 CCDs, 3 rep) | + C (bench query, 4 CCDs, 5 rep internes) |

**Argument-clé pour la thèse** (à retrouver dans `figures/breakeven_n.pdf`
après run 4) : « Sur la médiane des instances étudiées, le DP devient rentable
face à Z3 dès $N^*$ requêtes par formule, où $N^*$ a une valeur typique de
l'ordre de 10–100 selon la famille. Cette propriété d'amortissement est la
justification quantitative principale du DP face à un solveur SAT industriel. »
