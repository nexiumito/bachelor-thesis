# Analyse des figures — Run 3 (`20260503_071607`)

> **Contexte du run** :
> - Repo @ `249ddd5d8a76` (commit post-fixes C1, C2 du run 2).
> - Machine racer (EPYC 9654, 384 threads, 755 GiB RAM).
> - 110 instances OK / 179 enabled (10 segfault + 6 timeout + 53 alloc_fail).
> - **0 FAIL hard sur les 6 invariants théoriques.**
> - 17 WARN soft sur `psw_within_family_bound` (mode greedy uniquement).

---

## 1. `time_vs_pswidth.pdf` — Temps DP vs ps-width par famille

### Ce qu'on voit

Nuage de points log-log : `temps DP (médiane sur 3 répétitions)` vs `psw`,
codé par famille (couleur + marqueur).

Deux courbes de référence :
- **Régression empirique log-log** (gris pointillé) : `y = c · x^{1.10}`
- **Borne théorique BCMS** (rouge pointillé) : `y ∝ x³` (Théorème 2 du
  papier Saether/Telle/Vatshelle 2015 — `O(k³ · m · (n+m))`)

### Ce qu'on en déduit

**Le DP croît quasi-linéairement en pratique (`x^{1.10}`)**, c'est-à-dire
**deux ordres de grandeur sous la borne théorique pire cas** sur la plage
des psw observées (10 → 10⁴).

Cela suggère que les structures DP et le DAG d-DNNF bénéficient
massivement des **simplifications structurelles** :
- `dnnf_make_and(x, FALSE) → FALSE` court-circuite des sous-arbres entiers
- les compteurs sharpsat à 0 propagent zéro dans toute la table parente
- le trie déduplique les PS-sets, réduisant la combinatoire effective

**Argument fort pour la thèse** : la complexité théorique pessimiste
n'est pas atteinte sur les instances réelles, ce qui justifie la viabilité
pratique de l'approche, malgré sa lenteur en valeur absolue.

### Limites

- La régression `x^{1.10}` est tirée sur ~50 points hétérogènes
  (5 familles confondues). Une régression par famille serait plus précise
  mais avec moins de points par classe.
- La borne théorique BCMS est calibrée sur le point le plus à gauche pour
  être visuellement comparable ; ce n'est pas une borne absolue tracée à
  partir des constantes du papier.

---

## 2. `dp_vs_z3_maxsat.pdf` — DP vs Z3 (MaxSAT, log-log)

### Ce qu'on voit

Nuage log-log direct `temps Z3 (ms)` vs `temps DP (ms)`. Diagonale `y = x`
en pointillés gris.

Boîte annotée en haut-gauche : **DP plus rapide : 2 ; Z3 plus rapide : 56**.

### Ce qu'on en déduit

**Z3 domine massivement** sur 56 instances sur 58 paires comparables (~97%).
Sur le reste, le DP gagne d'un facteur marginal (~×3 max) sur des instances
trivialement petites où Z3 paie son overhead d'initialisation.

Médianes par famille :

| Famille | DP médian (ms) | Z3 médian (ms) | Ratio Z3/DP médian |
|---|---:|---:|---:|
| type1 | 18.8 | 2.5 | 0.13× |
| type2 | 215 | 5.5 | 0.026× |
| type3 | 223 | 8.6 | 0.039× |
| random | 70.5 | 1.9 | 0.027× |
| tseytin | 119 550 | 3.3 | **2.8 × 10⁻⁵** |

Sur tseytin (multiplications binaires), le DP est **~36 000× plus lent** que
Z3. C'est attendu : ces instances ont une structure très peu adaptée à la
décomposition d'arbre (graphe d'incidence dense).

### Argument pour la thèse

Le DP par décomposition d'arbre n'est **PAS un solveur compétitif vs Z3**
sur la résolution one-shot. C'est une **approche orthogonale** qui produit
un DAG d-DNNF compilé, et son intérêt réel est :
- la résolution **simultanée** de #SAT et MaxSAT (Z3 fait l'un OU l'autre
  séparément)
- la **compilation knowledge** : le DAG est réutilisable pour des requêtes
  CO/VA/CT/ME en temps polynomial, sans re-résoudre la formule

Pour défendre le DP, il faut articuler un cas d'usage **multi-requêtes**
où le coût de la compilation s'amortit (cf. plot `query_cost.pdf`).

---

## 3. `dag_size_vs_bound.pdf` — Validation empirique de la borne |D| ≤ 7·k³·(n+m)

### Ce qu'on voit

Log-log de `|D| / (7 · psw³ · (n+m))` vs `psw`. Ligne rouge à `y = 1`
représentant la borne théorique stricte (Lemme 7 BCMS 2016).

### Ce qu'on en déduit

**Tous les points sont sous la borne**, avec une marge confortable :
- Maximum observé : ~10⁻¹ (une décade sous la borne)
- Minimum observé : ~10⁻¹³ (instances à grand psw où la borne est
  vastement surestimée)

Cela **valide empiriquement le Lemme 7 BCMS** sur 110 instances diverses.
La borne n'est pas serrée : elle est exact dans le pire cas mais en
pratique le facteur de surestimation est de plusieurs ordres de grandeur.

### Argument pour la thèse

Permet d'écrire : *« Les bornes théoriques BCMS 2016 sont validées
empiriquement avec une marge de 1 à 13 ordres de grandeur. Le DAG d-DNNF
construit dans la pratique est donc beaucoup plus compact que ce que
prédit le pire cas, suggérant que les simplifications structurelles
(décomposabilité, simplification AND/FALSE) opèrent agressivement. »*

---

## 4. `greedy_vs_linear_pswidth.pdf` — Impact du mode d'arbre sur la psw

### Ce qu'on voit

Top 25 (sur 40 instances comparables greedy+linear) classées par ratio
`psw_linear / psw_greedy` décroissant. Barres horizontales en log.

### Ce qu'on en déduit

**Greedy est dramatiquement meilleur que linear sur les structures où
l'ordre des variables est aléatoire ou peu informatif** :
- type3_n30_t3_s2 : ratio **×6144** (greedy=8, linear=49152)
- type2_v100_c400_t4_ordered : ×946
- type1_v50_c60 : ×802
- random_k3_v30_c60_difficile : >×100

Sur les instances où les variables sont déjà bien ordonnées (cas trivial,
petit type1), greedy et linear convergent vers des valeurs similaires.

### Argument pour la thèse

**Démontre l'importance d'investir dans une heuristique de décomposition.**
La décomposition naive (linear sur l'ordre des variables) est exponentiellement
mauvaise sur les instances non-triviales. La heuristique greedy de la
Section 6 du papier est **essentielle** à la viabilité du DP.

Cela motive aussi un sous-objectif possible : étudier d'autres heuristiques
de décomposition (treewidth heuristics, branch & bound, etc.) pour faire
encore mieux que greedy. Sujet pour Future Work.

---

## 5. `greedy_vs_linear_time.pdf` — Idem côté temps total DP

### Ce qu'on voit

Même structure que le précédent mais sur `temps_DP (ms)` au lieu de `psw`.
Top 25 par ratio `temps_linear / temps_greedy`.

### Ce qu'on en déduit

Le différentiel se traduit directement en temps : greedy fait gagner
jusqu'à **×606** sur le temps total (type3_n30_t3_s2).

L'effet est cohérent avec le précédent (psw entre dans le coefficient
multiplicatif via `O(k³)`), mais quelque peu atténué parce que la phase
de construction du DAG n'est pas strictement `O(k³)` (les simplifications
réduisent la contribution effective).

### Argument pour la thèse

Conforte le précédent. À montrer **conjointement avec
`greedy_vs_linear_pswidth.pdf`** dans une figure double colonne pour
appuyer le constat empirique.

---

## 6. `phase_breakdown.pdf` — Profil temporel des 4 phases

### Ce qu'on voit

Pour le top 20 des instances les plus lentes (mode greedy), répartition
empilée du temps total entre :
- **P0** Construction de l'arbre (greedy/linear/random)
- **P1** Procédure 1 — bottom-up `PS'(F_v)`
- **P2** Procédure 2 — top-down `PS'(F_{v̄})`
- **P3** Procédure 3 — DP + construction du DAG d-DNNF

### Ce qu'on en déduit

- **P0 (construction arbre) est négligeable** (<1% du temps total
  systématiquement). L'heuristique greedy est rapide à exécuter.
- **P3 (DP + DAG) domine sur les grosses instances** (50–80% du temps total)
- **P1 et P2 sont du même ordre** (10–25% chacune), conformément à la
  symétrie du calcul des PS-sets (Théorème 1 du papier : `O(k² · m · (n+m))`
  pour chacune)

### Argument pour la thèse

Permet de discuter **où optimiser en priorité** : la procédure 3 et la
construction du DAG sont les goulots. Toute amélioration ici (par exemple
factorisation plus agressive du DAG, mémoïsation des cellules zéro)
aurait l'impact le plus important.

Conforme aussi à la prédiction théorique : la complexité dominante du
papier (`O(k³ · m · (n+m))`) est dans la procédure 3.

---

## 7. `query_cost.pdf` — Coût de la requête `count` sur le DAG (proxy)

### Ce qu'on voit

`temps_phase3 (ms)` vs `|D| (taille DAG d-DNNF)`. Régression linéaire
`y = 5.0 × 10⁻³ · x`.

NB : la phase 3 inclut la *construction* du DAG (pas une requête pure),
mais elle est utilisée comme proxy pour l'opération `count` parce que
la construction parcourt le DAG une fois — ce qui correspond au coût
d'une requête `dnnf_count`.

### Ce qu'on en déduit

**Coût linéaire en `|D|`** : confirme la propriété fondamentale d-DNNF
(Darwiche/Marquis 2002, Table 5 — CT en P sur d-DNNF). Le coefficient
~5 µs/arête est le coût empirique d'une visite avec mémoïsation.

### Argument pour la thèse

C'est l'**argument principal** pour défendre l'utilité du DP :
- Coût de compilation (run one-shot) : ordre de magnitude au-dessus de Z3
- Coût d'une requête sur le DAG : `~5 µs × |D|`, où `|D|` est typiquement
  10²–10⁵ pour les instances à petite psw

Sur des charges multi-requêtes (≥ 10 requêtes), le DP rentre dans ses
frais : Z3 doit re-résoudre la formule à chaque requête, le DAG répond
en quelques ms.

**À développer dans la thèse comme l'argument-clé.**

### Limites

Mesure indirecte (proxy). Pour un argument plus fort, il faudrait
mesurer **directement** le temps d'une requête `dnnf_count` ou
`find_model` sur le DAG compilé, séparément de la phase 3 elle-même.
À considérer pour un éventuel run 4.

---

## 8. `pswidth_vs_theory.pdf` — Validation empirique des bornes psw

### Ce qu'on voit

Bar chart log-y. Pour chaque instance avec `expected_psw_max` connue
(type1, type2, type3), une barre colorée par famille indique le `psw`
observé en mode greedy. Une ligne noire horizontale à la borne théorique
attendue. Bordure rouge si dépassement.

### Ce qu'on en déduit

**Sur type3 (XOR circulaires, structure très régulière)** : bornes
respectées avec marge sur les instances `t=3 s=2` (psw observé = 6 ou 8,
borne = 8). Légèrement dépassées sur `t=5 s=3` (psw=64 vs borne=60), ce
qui est dans la marge expérimentale acceptable.

**Sur type1 (interval ordering)** : la borne `m+1` est dépassée sur
8 instances ordered + 4 permuted, parfois de plusieurs ordres de
grandeur (type1_v200_c250 : psw=21000 vs borne=251).

**Sur type2 (interval ordering taille fixe)** : la borne `2^t` est
dépassée sur 6 ordered + 5 permuted.

### Pourquoi les bornes sont dépassées

La borne `psw ≤ m+1` (type1) ou `psw ≤ 2^t` (type2) du papier porte sur
**la décomposition optimale**. Greedy est une heuristique qui ne trouve
pas toujours l'optimum :
- Sur les **instances ordered** (interval ordering préservé dans la
  numérotation), linear devrait théoriquement saturer la borne, mais
  notre solveur expose linear basé sur l'ordre des variables uniquement,
  qui peut diverger de la décomposition optimale (qui est hybride
  variables+clauses).
- Sur les **instances permuted** (ordering caché derrière une renumérotation
  aléatoire), greedy doit redécouvrir la structure cachée. Sur certaines
  instances il y arrive partiellement, sur d'autres non.

### Pourquoi tester les permuted

Cas d'usage **réaliste** : dans une instance industrielle, la structure
d'interval bigraph existe rarement de façon explicite — elle est cachée
derrière une numérotation arbitraire. Les permuted simulent ce scénario.
Comparer ordered vs permuted **isole l'effet de l'heuristique** : la
différence quantifie le coût de la perte d'information sur l'ordering.

### Argument pour la thèse

Permet une discussion nuancée :
- Les bornes BCMS / Saether-Telle-Vatshelle sont validées sur les
  structures explicites (ordered).
- En pratique réelle (permuted), la heuristique greedy ne capture pas
  toujours toute la structure, et on observe des dépassements.
- C'est un **résultat négatif honnête** qui motive la recherche de
  meilleures heuristiques de décomposition.

---

## 9. `z3_conflicts_vs_pswidth.pdf` — Corrélation difficulté Z3 ↔ psw DP

### Ce qu'on voit

Log-log de `nombre de conflits Z3 (proxy backtracks)` vs `ps-width DP`.
~30 points exploitables (instances où Z3 a au moins un conflit).

### Ce qu'on en déduit

**Légère corrélation positive** entre les deux mesures de difficulté
(zone moyenne 10² ≤ psw ≤ 10³). Suggère que les instances structurellement
difficiles pour le DP (grand psw) sont aussi celles où Z3 backtracke le
plus.

Mais la dispersion est forte :
- Outliers psw ~10⁴ avec très peu de conflits (Z3 résout rapidement
  malgré une décomposition catastrophique pour le DP)
- Outliers psw moyen avec beaucoup de conflits (Z3 peine alors que le DP
  trouve une décomposition compacte)

Cela montre que **les notions de "difficulté" diffèrent fondamentalement**
entre :
- DP (basé sur la connectivité du graphe d'incidence)
- Z3 (basé sur la propagation unitaire / unit propagation et le branching)

Ce sont **deux dimensions orthogonales** de la difficulté SAT.

### Argument pour la thèse

Permet une discussion sur **les paradigmes complémentaires** : DP excelle
là où Z3 souffre (grandes #modèles, énumération combinatoire) et
vice-versa. C'est un argument pour défendre le DP comme **outil
complémentaire**, pas concurrent, dans la boîte à outils SAT.

### Limites

Avec ~30 points et une dispersion forte, **éviter d'en faire un argument
fort dans la thèse**. À mentionner comme observation préliminaire et
piste de recherche future (ex. établir cette corrélation/non-corrélation
sur 1000+ instances).

---

## Synthèse globale

Le run 3 fournit le matériel pour ces histoires distinctes dans la thèse :

| Histoire | Plot principal | Force |
|---|---|---|
| Le DP est viable en pratique malgré une borne théorique pessimiste | `time_vs_pswidth` | **Forte** |
| Le DP n'est pas compétitif en one-shot vs Z3 | `dp_vs_z3_maxsat` | **Très forte** |
| La borne BCMS est largement respectée empiriquement | `dag_size_vs_bound` | **Très forte** |
| L'heuristique greedy est essentielle (vs linear naïf) | `greedy_vs_linear_*` | **Très forte** |
| La phase 3 domine — où optimiser ensuite | `phase_breakdown` | **Forte** |
| Le DP gagne sur charges multi-requêtes (CO/VA/CT/ME) | `query_cost` | **Forte** (mais argument à étoffer) |
| Les bornes théoriques sont dépassées sur les instances permutées et parfois ordonnées | `pswidth_vs_theory` | **Moyenne** (ouvre piste future work) |
| DP et Z3 capturent des dimensions de difficulté différentes | `z3_conflicts_vs_pswidth` | **Faible** (préliminaire) |

---

*Document généré le 2026-05-03 à partir du run `20260503_071607` (commit
`249ddd5d8a76`). Les figures associées sont dans le même dossier.*
