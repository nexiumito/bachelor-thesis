# Benchmark — 20260509_102338

**Repo** : `origin` à `2348ee642a9e` (branche `main`)
**Machine** : racer (x86_64, 384 threads, 755.4 GiB RAM)
**Durée totale** : 1h50m
**Configuration** : passe A 64 jobs, passe B sur CPU 0,8,16,24, 3 répétitions

---

## Vue globale

| Famille | Total | OK | Timeout | Crash | Disabled |
|---|---:|---:|---:|---:|---:|
| type1 | 31 | 24 | 0 | 1 | 0 |
| type2 | 39 | 21 | 3 | 0 | 0 |
| type3 | 43 | 28 | 1 | 2 | 4 |
| random | 47 | 25 | 0 | 4 | 0 |
| tseytin | 15 | 10 | 1 | 4 | 4 |
| factorization | 4 | 2 | 1 | 1 | 0 |
| misc | 0 | 0 | 0 | 0 | 0 |
| **Total** | **179** | **110** | **6** | **12** | **8** |

## Invariants

| Invariant | OK | FAIL (hard) | WARN (soft) | Skipped |
|---|---:|---:|---:|---:|
| `dnnf_count_match` | 164 | 0 | 0 | 69 |
| `dag_within_bcms_bound` | 161 | 0 | 0 | 72 |
| `maxsat_match_z3` | 164 | 0 | 0 | 69 |
| `psw_within_family_bound` | 51 | 0 | 33 | 149 |
| `consistency_match_dp_z3` | 164 | 0 | 0 | 69 |
| `phase_times_nonneg` | 164 | 0 | 0 | 69 |
| `entails_match_z3` | 54 | 0 | 0 | 0 |
| `enumerate_count_match` | 15 | 0 | 0 | 39 |



## Top 10 instances les plus lentes (DP, mode greedy)

> **Convention `Ratio Z3/DP`** : valeur = temps Z3 / temps DP.
> `> 1.00x` ⇒ DP plus rapide que Z3. `< 1.00x` ⇒ Z3 plus rapide que DP.
> `0.00x` ⇒ Z3 vastement plus rapide (DP au-delà de 4 ordres de grandeur).

| Instance | Famille | n | m | psw | Temps DP (ms) | Temps Z3 (ms) | Ratio Z3/DP |
|---|---|---:|---:|---:|---:|---:|---:|
| `type1_v200_c250` | type1 | 200 | 213 | 21000 | 456391.9 | 5.0 | 0.00x |
| `tseytin_4x4` | tseytin | 72 | 220 | 8192 | 167890.6 | 2.2 | 0.00x |
| `tseytin_pure_4x4` | tseytin | 72 | 212 | 8192 | 152834.2 | 1.9 | 0.00x |
| `type3_n1000_t5_s3` | type3 | 1000 | 5328 | 60 | 56004.0 | 57.7 | 0.00x |
| `type3_n5000_t3_s2` | type3 | 5000 | 10000 | 8 | 29732.5 | 127.1 | 0.00x |
| `type1_v150_c180` | type1 | 150 | 152 | 5616 | 19769.6 | 4.0 | 0.00x |
| `type1_v150_c180_ordered` | type1 | 150 | 179 | 1800 | 15311.1 | 4.0 | 0.00x |
| `random_k3_v30_c60_difficile` | random | 30 | 60 | 7168 | 11829.5 | 1.0 | 0.00x |
| `random_k3_v20_c85_difficile` | random | 20 | 85 | 16400 | 11550.9 | 1.2 | 0.00x |
| `random_k4_v20_c50` | random | 20 | 50 | 3268 | 4573.2 | 1.3 | 0.00x |


## Top 10 instances les plus rapides (DP, mode greedy)

| Instance | Famille | n | m | psw | Temps DP (ms) | Temps Z3 (ms) | Ratio Z3/DP |
|---|---|---:|---:|---:|---:|---:|---:|
| `type1_v20_c25` | type1 | 20 | 24 | 12 | 0.6 | 3.6 | 6.04x |
| `random_k3_v8_c20` | random | 8 | 20 | 32 | 0.9 | 0.7 | 0.78x |
| `type3_n30_t3_s2_ordered` | type3 | 30 | 60 | 6 | 1.0 | 1.1 | 1.09x |
| `type1_v30_c40` | type1 | 30 | 27 | 16 | 1.3 | 0.7 | 0.57x |
| `type1_v20_c25_ordered` | type1 | 20 | 25 | 24 | 1.4 | 0.8 | 0.56x |
| `type3_n30_t3_s2` | type3 | 30 | 60 | 8 | 1.4 | 1.5 | 1.07x |
| `type2_v25_c100_t4` | type2 | 25 | 100 | 14 | 1.8 | 1.6 | 0.91x |
| `type2_v25_c100_t3_ordered` | type2 | 25 | 88 | 28 | 1.9 | 1.4 | 0.74x |
| `type1_v30_c40_ordered` | type1 | 30 | 38 | 18 | 2.3 | 1.0 | 0.43x |
| `type1_v50_c60` | type1 | 50 | 49 | 56 | 2.3 | 0.9 | 0.38x |


## Comparaison DP vs Z3 par famille

| Famille | Médiane DP (ms) | Médiane Z3 (ms) | Médiane Z3/DP | DP > Z3 | DP < Z3 |
|---|---:|---:|---:|---:|---:|
| type1 | 11.9 | 1.4 | 0.09x | 10 | 1 |
| type2 | 125.7 | 3.3 | 0.05x | 13 | 0 |
| type3 | 143.1 | 4.5 | 0.03x | 17 | 2 |
| random | 42.3 | 1.0 | 0.04x | 11 | 0 |
| tseytin | 76526.1 | 1.6 | 0.00x | 4 | 0 |



## Bench des requêtes sur DAG compilé (passe C)

> Mesure du temps d'exécution des requêtes CO/VA/CT/CE/IM/ME (1 modèle / énumération
> partielle) sur DAG **déjà compilé** (in-process, médiane sur 5 répétitions, 1ère
> exécution jetée pour warm-up). Cette section supporte l'argument
> d'amortissement multi-requêtes (cf. plot break-even N).

### Médiane par requête (toutes familles confondues)

| Requête | Médiane (µs) | Médiane µs/arête | Speedup vs Z3 (médiane) |
|---|---:|---:|---:|
| CO | 42.80 | 0.0106 | 16.58x |
| VA | 43.99 | 0.0105 | 17.33x |
| CT | 42.58 | 0.0105 | 17.42x |
| ME-1 | 50.00 | 0.0112 | 15.62x |
| CE | 295.60 | 0.0669 | 1.80x |
| IM | 155.38 | 0.1612 | 2.17x |
| ME-multi (1er) | 3.90 | 0.0007 | 163.28x |



### Break-even N

- Médiane N* (sur instances avec N* fini) : **120**
- 36 / 39 instances ont N* fini
- Lecture : si l'utilisateur prévoit **≥ 120 requêtes par
  formule**, le DP+queries est en moyenne plus rentable que N appels Z3 froids.


### Note méthodologique

Le scénario "cache cold vs warm" (1ère requête vs 5ème) n'est volontairement
pas mesuré : le warm-up structurel (1ère exécution jetée) masque l'effet pour
rester focus sur le break-even.


## Figures

- [time_vs_pswidth.pdf](figures/time_vs_pswidth.pdf) — temps DP vs ps-width par famille
- [dp_vs_z3_maxsat.pdf](figures/dp_vs_z3_maxsat.pdf) — comparaison directe avec diagonale y=x
- [dag_size_vs_bound.pdf](figures/dag_size_vs_bound.pdf) — taille du DAG / borne BCMS
- [greedy_vs_linear.pdf](figures/greedy_vs_linear.pdf) — impact du mode d'arbre
- [z3_conflicts_vs_pswidth.pdf](figures/z3_conflicts_vs_pswidth.pdf) — corrélation conflicts Z3 / ps-width DP
- [phase_breakdown.pdf](figures/phase_breakdown.pdf) — profil temporel des 4 phases
- [query_cost.pdf](figures/query_cost.pdf) — coût des requêtes sur DAG (v1, proxy)
- [pswidth_vs_theory.pdf](figures/pswidth_vs_theory.pdf) — vérification des bornes théoriques

- [breakeven_n.pdf](figures/breakeven_n.pdf) — courbe de coût et distribution des N* (passe C)
- [query_vs_z3.pdf](figures/query_vs_z3.pdf) — speedup par requête × famille
- [query_per_edge.pdf](figures/query_per_edge.pdf) — coût empirique µs/arête par requête
- [co_vs_z3_per_instance.pdf](figures/co_vs_z3_per_instance.pdf) — comparaison rigoureuse CO vs Z3 SAT (même question)


## Fichiers de données

- [structure.csv](structure.csv) — toutes les exécutions de la passe A (DP)
- [z3.csv](z3.csv) — toutes les exécutions Z3
- [timings.csv](timings.csv) — médianes de la passe B
- [invariants.csv](invariants.csv) — vérifications théoriques
- [failures.log](failures.log) — détail des échecs
- [progress.log](progress.log) — heartbeat du run
- [env.txt](env.txt) — environnement matériel/logiciel
- [git_info.txt](git_info.txt) — état du repo

## Reproduction

```bash
ssh ebussod@racer
tmux new -s bench-repro
cd ~/bachelor-thesis
git checkout 2348ee642a9e
make -C benchmarks bench
```

---

*Généré automatiquement par `benchmarks/orchestrator.py` v0.1.0.*