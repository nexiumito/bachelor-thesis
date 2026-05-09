# Benchmark — 20260502_162229

**Repo** : `origin` à `e1e8d83d18b5` (branche `main`)
**Machine** : racer (x86_64, 384 threads, 755.4 GiB RAM)
**Durée totale** : 1h55m
**Configuration** : passe A 64 jobs, passe B sur CPU 0,8,16,24, 3 répétitions

---

## Vue globale

| Famille | Total | OK | Timeout | Crash | Disabled |
|---|---:|---:|---:|---:|---:|
| type1 | 31 | 26 | 1 | 0 | 0 |
| type2 | 39 | 22 | 0 | 0 | 0 |
| type3 | 43 | 28 | 2 | 2 | 4 |
| random | 47 | 25 | 0 | 4 | 0 |
| tseytin | 15 | 10 | 4 | 1 | 4 |
| factorization | 4 | 2 | 2 | 0 | 0 |
| misc | 0 | 0 | 0 | 0 | 0 |
| **Total** | **179** | **113** | **9** | **7** | **8** |

## Invariants

| Invariant | OK | FAIL (hard) | WARN (soft) | Skipped |
|---|---:|---:|---:|---:|
| `dnnf_count_match` | 112 | 1 | 0 | 66 |
| `dag_within_bcms_bound` | 110 | 0 | 0 | 69 |
| `maxsat_match_z3` | 80 | 0 | 0 | 99 |
| `psw_within_family_bound` | 29 | 0 | 16 | 134 |
| `consistency_match_dp_z3` | 112 | 1 | 0 | 66 |
| `phase_times_nonneg` | 113 | 0 | 0 | 66 |


### FAIL critiques

- **dnnf_count_match** sur `type2_v500_c2000_t3_ordered` (mode `greedy`) : overflow detecte et drapeau != true
- **consistency_match_dp_z3** sur `type2_v500_c2000_t3_ordered` (mode `greedy`) : DP sat=True vs Z3 sat=False



## Top 10 instances les plus lentes (DP, mode greedy)

| Instance | Famille | n | m | psw | Temps DP (ms) | Temps Z3 (ms) | Speedup |
|---|---|---:|---:|---:|---:|---:|---:|
| `type2_v100_c400_t4` | type2 | 100 | 400 | 9666 | 559608.8 | 6.8 | 0.00x |
| `type2_v500_c2000_t3_ordered` | type2 | 500 | 2000 | 32768 | 395180.4 | - | - |
| `tseytin_4x4` | tseytin | 72 | 220 | 8192 | 257213.9 | 3.4 | 0.00x |
| `tseytin_pure_4x4` | tseytin | 72 | 212 | 8192 | 236226.5 | 7.1 | 0.00x |
| `type3_n5000_t3_s2` | type3 | 5000 | 10000 | 8 | 86079.7 | 272.3 | 0.00x |
| `type3_n1000_t5_s3` | type3 | 1000 | 5328 | 60 | 83382.4 | 113.9 | 0.00x |
| `type1_v200_c250_ordered` | type1 | 200 | 250 | 5172 | 22778.1 | - | - |
| `random_k3_v30_c60_difficile` | random | 30 | 60 | 7168 | 18733.8 | 2.2 | 0.00x |
| `type1_v150_c180_ordered` | type1 | 150 | 180 | 4608 | 14735.2 | - | - |
| `random_k3_v20_c85_difficile` | random | 20 | 85 | 16400 | 9049.7 | 2.0 | 0.00x |


## Top 10 instances les plus rapides (DP, mode greedy)

| Instance | Famille | n | m | psw | Temps DP (ms) | Temps Z3 (ms) | Speedup |
|---|---|---:|---:|---:|---:|---:|---:|
| `type1_v20_c25` | type1 | 20 | 25 | 8 | 0.6 | - | - |
| `type1_v30_c40` | type1 | 30 | 40 | 14 | 1.0 | - | - |
| `random_k3_v8_c20` | random | 8 | 20 | 32 | 1.4 | 1.0 | 0.69x |
| `type1_v20_c25_ordered` | type1 | 20 | 25 | 18 | 1.4 | - | - |
| `type3_n30_t3_s2_ordered` | type3 | 30 | 60 | 6 | 1.7 | 4.0 | 2.36x |
| `type3_n30_t3_s2` | type3 | 30 | 60 | 8 | 2.4 | 4.1 | 1.74x |
| `type2_v25_c100_t4_ordered` | type2 | 25 | 100 | 11 | 3.3 | 2.3 | 0.70x |
| `type1_v50_c60` | type1 | 50 | 60 | 22 | 3.4 | - | - |
| `random_k3_v7_c50` | random | 7 | 50 | 60 | 6.5 | 2.6 | 0.40x |
| `type1_v30_c40_ordered` | type1 | 30 | 40 | 40 | 7.5 | - | - |


## Comparaison DP vs Z3 par famille

| Famille | Médiane DP (ms) | Médiane Z3 (ms) | Médiane speedup | DP > Z3 | DP < Z3 |
|---|---:|---:|---:|---:|---:|
| type2 | 101.8 | 6.8 | 0.13x | 10 | 0 |
| type3 | 417.7 | 8.2 | 0.02x | 17 | 2 |
| random | 38.7 | 2.0 | 0.05x | 11 | 0 |
| tseytin | 118410.2 | 3.6 | 0.00x | 4 | 0 |


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
git checkout e1e8d83d18b5
make -C benchmarks bench
```

---

*Généré automatiquement par `benchmarks/orchestrator.py` v0.1.0.*