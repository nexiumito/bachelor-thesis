# Benchmark — 20260503_071607

**Repo** : `origin` à `249ddd5d8a76` (branche `main`)
**Machine** : racer (x86_64, 384 threads, 755.4 GiB RAM)
**Durée totale** : 2h00m
**Configuration** : passe A 64 jobs, passe B sur CPU 0,8,16,24, 3 répétitions

---

## Vue globale

| Famille | Total | OK | Timeout | Crash | Disabled |
|---|---:|---:|---:|---:|---:|
| type1 | 31 | 24 | 0 | 1 | 0 |
| type2 | 39 | 21 | 1 | 0 | 0 |
| type3 | 43 | 28 | 1 | 2 | 4 |
| random | 47 | 25 | 0 | 4 | 0 |
| tseytin | 15 | 10 | 2 | 3 | 4 |
| factorization | 4 | 2 | 2 | 0 | 0 |
| misc | 0 | 0 | 0 | 0 | 0 |
| **Total** | **179** | **110** | **6** | **10** | **8** |

## Invariants

| Invariant | OK | FAIL (hard) | WARN (soft) | Skipped |
|---|---:|---:|---:|---:|
| `dnnf_count_match` | 110 | 0 | 0 | 69 |
| `dag_within_bcms_bound` | 107 | 0 | 0 | 72 |
| `maxsat_match_z3` | 110 | 0 | 0 | 69 |
| `psw_within_family_bound` | 26 | 0 | 17 | 136 |
| `consistency_match_dp_z3` | 110 | 0 | 0 | 69 |
| `phase_times_nonneg` | 110 | 0 | 0 | 69 |



## Top 10 instances les plus lentes (DP, mode greedy)

> **Convention `Ratio Z3/DP`** : valeur = temps Z3 / temps DP.
> `> 1.00x` ⇒ DP plus rapide que Z3. `< 1.00x` ⇒ Z3 plus rapide que DP.
> `0.00x` ⇒ Z3 vastement plus rapide (DP au-delà de 4 ordres de grandeur).

| Instance | Famille | n | m | psw | Temps DP (ms) | Temps Z3 (ms) | Ratio Z3/DP |
|---|---|---:|---:|---:|---:|---:|---:|
| `type1_v200_c250` | type1 | 200 | 213 | 21000 | 706799.4 | 7.9 | 0.00x |
| `tseytin_4x4` | tseytin | 72 | 220 | 8192 | 259787.0 | 4.1 | 0.00x |
| `tseytin_pure_4x4` | tseytin | 72 | 212 | 8192 | 238752.7 | 4.3 | 0.00x |
| `type3_n1000_t5_s3` | type3 | 1000 | 5328 | 60 | 86477.2 | 92.0 | 0.00x |
| `type3_n5000_t3_s2` | type3 | 5000 | 10000 | 8 | 45994.2 | 257.0 | 0.01x |
| `type1_v150_c180` | type1 | 150 | 152 | 5616 | 31491.1 | 7.3 | 0.00x |
| `type1_v150_c180_ordered` | type1 | 150 | 179 | 1800 | 24063.2 | 6.9 | 0.00x |
| `random_k3_v30_c60_difficile` | random | 30 | 60 | 7168 | 19364.2 | 3.2 | 0.00x |
| `random_k3_v20_c85_difficile` | random | 20 | 85 | 16400 | 18602.3 | 2.2 | 0.00x |
| `random_k4_v20_c50` | random | 20 | 50 | 3268 | 7312.4 | 2.0 | 0.00x |


## Top 10 instances les plus rapides (DP, mode greedy)

| Instance | Famille | n | m | psw | Temps DP (ms) | Temps Z3 (ms) | Ratio Z3/DP |
|---|---|---:|---:|---:|---:|---:|---:|
| `type1_v20_c25` | type1 | 20 | 24 | 12 | 1.0 | 3.9 | 3.82x |
| `random_k3_v8_c20` | random | 8 | 20 | 32 | 1.6 | 1.2 | 0.72x |
| `type3_n30_t3_s2_ordered` | type3 | 30 | 60 | 6 | 1.7 | 1.6 | 0.95x |
| `type1_v30_c40` | type1 | 30 | 27 | 16 | 2.3 | 1.9 | 0.80x |
| `type3_n30_t3_s2` | type3 | 30 | 60 | 8 | 2.4 | 2.8 | 1.18x |
| `type1_v20_c25_ordered` | type1 | 20 | 25 | 24 | 2.5 | 1.5 | 0.58x |
| `type2_v25_c100_t4` | type2 | 25 | 100 | 14 | 2.9 | 2.7 | 0.94x |
| `type1_v30_c40_ordered` | type1 | 30 | 38 | 18 | 4.2 | 2.0 | 0.48x |
| `type1_v50_c60` | type1 | 50 | 49 | 56 | 4.2 | 1.6 | 0.38x |
| `type3_n60_t3_s2_ordered` | type3 | 60 | 120 | 6 | 4.9 | 1.9 | 0.40x |


## Comparaison DP vs Z3 par famille

| Famille | Médiane DP (ms) | Médiane Z3 (ms) | Médiane Z3/DP | DP > Z3 | DP < Z3 |
|---|---:|---:|---:|---:|---:|
| type1 | 18.8 | 2.5 | 0.11x | 10 | 1 |
| type2 | 215.0 | 5.5 | 0.04x | 13 | 0 |
| type3 | 223.4 | 8.6 | 0.04x | 18 | 1 |
| random | 70.5 | 1.9 | 0.04x | 11 | 0 |
| tseytin | 119550.1 | 3.3 | 0.00x | 4 | 0 |


## Figures

- [time_vs_pswidth.pdf](figures/time_vs_pswidth.pdf) — temps DP vs ps-width par famille
- [dp_vs_z3_maxsat.pdf](figures/dp_vs_z3_maxsat.pdf) — comparaison directe avec diagonale y=x
- [dag_size_vs_bound.pdf](figures/dag_size_vs_bound.pdf) — taille du DAG / borne BCMS
- [greedy_vs_linear.pdf](figures/greedy_vs_linear.pdf) — impact du mode d'arbre
- [z3_conflicts_vs_pswidth.pdf](figures/z3_conflicts_vs_pswidth.pdf) — corrélation conflicts Z3 / ps-width DP
- [phase_breakdown.pdf](figures/phase_breakdown.pdf) — profil temporel des 4 phases
- [query_cost.pdf](figures/query_cost.pdf) — coût des requêtes sur DAG (v1, proxy)
- [pswidth_vs_theory.pdf](figures/pswidth_vs_theory.pdf) — vérification des bornes théoriques

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
git checkout 249ddd5d8a76
make -C benchmarks bench
```

---

*Généré automatiquement par `benchmarks/orchestrator.py` v0.1.0.*