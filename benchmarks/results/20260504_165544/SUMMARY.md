# Benchmark — 20260504_165544

**Repo** : `origin` à `ff91e05e1ab3` (branche `main`)
**Machine** : MacBook-Pro-de-Elie-21.local (arm, 8 threads, 8.0 GiB RAM)
**Durée totale** : 1h55m
**Configuration** : passe A 64 jobs, passe B sur CPU 0,8,16,24, 3 répétitions

---

## Vue globale

| Famille | Total | OK | Timeout | Crash | Disabled |
|---|---:|---:|---:|---:|---:|
| type1 | 31 | 24 | 0 | 1 | 0 |
| type2 | 39 | 21 | 0 | 0 | 0 |
| type3 | 43 | 28 | 1 | 2 | 4 |
| random | 47 | 25 | 0 | 4 | 0 |
| tseytin | 15 | 10 | 2 | 3 | 4 |
| factorization | 4 | 2 | 2 | 0 | 0 |
| misc | 0 | 0 | 0 | 0 | 0 |
| **Total** | **179** | **110** | **5** | **10** | **8** |

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
| `type1_v200_c250` | type1 | 200 | 213 | 21000 | 696802.5 | 8.8 | 0.00x |
| `tseytin_4x4` | tseytin | 72 | 220 | 8192 | 258546.9 | 5.0 | 0.00x |
| `tseytin_pure_4x4` | tseytin | 72 | 212 | 8192 | 239657.6 | 7.9 | 0.00x |
| `type3_n1000_t5_s3` | type3 | 1000 | 5328 | 60 | 84597.5 | 101.4 | 0.00x |
| `type3_n5000_t3_s2` | type3 | 5000 | 10000 | 8 | 45771.2 | 186.7 | 0.00x |
| `type1_v150_c180` | type1 | 150 | 152 | 5616 | 31154.3 | 7.1 | 0.00x |
| `type1_v150_c180_ordered` | type1 | 150 | 179 | 1800 | 23758.6 | 9.2 | 0.00x |
| `random_k3_v30_c60_difficile` | random | 30 | 60 | 7168 | 19227.4 | 1.1 | 0.00x |
| `random_k3_v20_c85_difficile` | random | 20 | 85 | 16400 | 16904.7 | 1.5 | 0.00x |
| `random_k4_v20_c50` | random | 20 | 50 | 3268 | 7525.3 | 3.4 | 0.00x |


## Top 10 instances les plus rapides (DP, mode greedy)

| Instance | Famille | n | m | psw | Temps DP (ms) | Temps Z3 (ms) | Ratio Z3/DP |
|---|---|---:|---:|---:|---:|---:|---:|
| `type1_v20_c25` | type1 | 20 | 24 | 12 | 0.9 | 3.9 | 4.25x |
| `random_k3_v8_c20` | random | 8 | 20 | 32 | 1.6 | 1.0 | 0.61x |
| `type3_n30_t3_s2_ordered` | type3 | 30 | 60 | 6 | 1.7 | 1.6 | 0.98x |
| `type2_v25_c100_t4` | type2 | 25 | 100 | 14 | 2.1 | 2.9 | 1.37x |
| `type1_v30_c40` | type1 | 30 | 27 | 16 | 2.3 | 1.3 | 0.57x |
| `type3_n30_t3_s2` | type3 | 30 | 60 | 8 | 2.4 | 3.3 | 1.38x |
| `type1_v20_c25_ordered` | type1 | 20 | 25 | 24 | 2.4 | 1.4 | 0.59x |
| `type2_v25_c100_t3_ordered` | type2 | 25 | 88 | 28 | 2.5 | 2.7 | 1.09x |
| `type1_v50_c60` | type1 | 50 | 49 | 56 | 3.9 | 1.6 | 0.42x |
| `type1_v30_c40_ordered` | type1 | 30 | 38 | 18 | 3.9 | 1.8 | 0.46x |


## Comparaison DP vs Z3 par famille

| Famille | Médiane DP (ms) | Médiane Z3 (ms) | Médiane Z3/DP | DP > Z3 | DP < Z3 |
|---|---:|---:|---:|---:|---:|
| type1 | 15.5 | 2.6 | 0.13x | 10 | 1 |
| type2 | 215.4 | 5.2 | 0.05x | 11 | 2 |
| type3 | 222.7 | 6.6 | 0.03x | 18 | 1 |
| random | 57.0 | 1.5 | 0.03x | 11 | 0 |
| tseytin | 120055.5 | 4.0 | 0.00x | 4 | 0 |



## Bench des requêtes sur DAG compilé (passe C)

> Mesure du temps d'exécution des requêtes CO/VA/CT/CE/IM/ME (1 modèle / énumération
> partielle) sur DAG **déjà compilé** (in-process, médiane sur 5 répétitions, 1ère
> exécution jetée pour warm-up). Cette section supporte l'argument
> d'amortissement multi-requêtes (cf. plot break-even N).

### Médiane par requête (toutes familles confondues)

| Requête | Médiane (µs) | Médiane µs/arête | Speedup vs Z3 (médiane) |
|---|---:|---:|---:|
| CO | 64.74 | 0.0167 | 18.63x |
| VA | 62.50 | 0.0163 | 17.67x |
| CT | 64.38 | 0.0165 | 18.42x |
| ME-1 | 75.30 | 0.0166 | 18.17x |
| CE | 564.08 | 0.1221 | 1.58x |
| IM | 327.07 | 0.3897 | 2.01x |
| ME-multi (1er) | 5.81 | 0.0011 | 212.69x |



### Break-even N

- Médiane N* (sur instances avec N* fini) : **161**
- 37 / 39 instances ont N* fini
- Lecture : si l'utilisateur prévoit **≥ 161 requêtes par
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
git checkout ff91e05e1ab3
make -C benchmarks bench
```

---

*Généré automatiquement par `benchmarks/orchestrator.py` v0.1.0.*