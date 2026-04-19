# Plan d'implémentation — Knowledge Compilation

> Plan en 2 phases sur les semaines 9 et 10, suivi de 4 semaines (S11 à S14)
> pour benchmarks, rédaction et finalisation du rapport de bachelor.
>
> Référence principale : Bova, Capelli, Mengel & Slivovsky (2016),
> *On Compiling CNFs into Structured Deterministic DNNFs*, Section 3.3, Lemmes 4-7.

---

## Vue d'ensemble du planning

| Semaine | Objectif | Livrable | Démo prof (mercredi) |
|---------|----------|----------|----------------------|
| **S9** | Phase 1 — Construction d-DNNF | DAG buildable, `dnnf_count == sharpsat` sur exemple1 | « Voici le circuit, je peux compter #SAT en parcourant le DAG » |
| **S10** | Phase 2 — Requêtes sur d-DNNF | `find_model`, `enumerate`, `condition`, `forget` | « 4 nouvelles requêtes en O(\|D\|), chiffres à l'appui » |
| **S11** | Benchmarks complets + graphiques | CSV de mesures, figures matplotlib | « Voici les résultats sur 30+ instances » |
| **S12** | Rédaction chapitre 3 (Implémentation) + chapitre 4 (Résultats) | 30+ pages | Discuter du contenu, structure, graphiques |
| **S13** | Rédaction chapitre 5 (Discussion) + 6 (Conclusion) + relecture intro/état de l'art | Thèse complète, draft final | Relecture partielle |
| **S14** | Corrections, mise en forme, soumission | PDF final | Confirmation soumission |

---

## Phase 1 — Semaine 9 : Construction du DAG d-DNNF

### Objectif

Modifier `procedure3.c` pour qu'**en plus** de calculer maxsat et sharpsat, elle construise explicitement le DAG d-DNNF correspondant aux traces du DP, exactement comme le décrivent les Lemmes 4-7 de Bova et al.

### 1.1. Nouveau module `src/utils/dnnf.{h,c}`

**Structure de données du nœud DAG :**

```c
typedef enum {
    DNNF_AND,       // ∧-gate, decomposable
    DNNF_OR,        // ∨-gate, deterministic
    DNNF_LIT_POS,   // littéral positif x_i
    DNNF_LIT_NEG,   // littéral négatif ¬x_i
    DNNF_TRUE,      // constante ⊤
    DNNF_FALSE      // constante ⊥
} DNNFNodeType;

typedef struct DNNFNode {
    int id;                     // identifiant unique global
    DNNFNodeType type;
    int var_index;              // 1..n pour LIT_POS/LIT_NEG, sinon ignoré
    struct DNNFNode** children; // tableau de pointeurs (NULL pour feuilles)
    int num_children;
    int capacity;               // capacité allouée du tableau children
} DNNFNode;
```

**Pool d'allocation centralisé :**

```c
typedef struct {
    DNNFNode** nodes;       // pool de tous les nœuds créés
    int num_nodes;          // nombre actuel
    int capacity;           // capacité (realloc ×2 si pleine)
    DNNFNode* node_true;    // singleton ⊤ partagé
    DNNFNode* node_false;   // singleton ⊥ partagé
} DNNFPool;
```

**API publique minimale :**

```c
DNNFPool*  create_dnnf_pool(int initial_capacity);
void       free_dnnf_pool(DNNFPool* pool);

DNNFNode*  dnnf_make_literal(DNNFPool* pool, int var, int positive);
DNNFNode*  dnnf_make_constant(DNNFPool* pool, int value);  // 0 ou 1
DNNFNode*  dnnf_make_and(DNNFPool* pool, DNNFNode* left, DNNFNode* right);
DNNFNode*  dnnf_make_or(DNNFPool* pool, int initial_capacity);
void       dnnf_or_add_child(DNNFNode* or_node, DNNFNode* child);

int        dnnf_size(DNNFNode* root);     // nombre d'arêtes
long long  dnnf_count(DNNFNode* root, int num_vars);  // #SAT (smoothing inclus)
void       dnnf_print_dot(DNNFNode* root, FILE* out); // export Graphviz
```

### 1.2. Modifications de `src/utils/dp_table.h`

Ajouter un troisième champ à `DPTable` :

```c
typedef struct {
    long long* maxsat;
    long long* sharpsat;
    DNNFNode** dnnf;     // NOUVEAU : dnnf[i*num_cols + j] = ϕ_v(S_i, S'_j)
    int num_rows, num_cols;
} DPTable;
```

Initialiser tous les pointeurs à `NULL` dans `create_dp_table`. Ne PAS libérer les nœuds dans `free_dp_table` (ils sont possédés par le `DNNFPool`).

### 1.3. Modifications de `procedure3.c`

**Cas feuille variable** (autour de la ligne où on calcule `count = 2 ou 1`) : pour chaque shape valide, créer un littéral et le stocker dans `tab->dnnf[i*cols+j]`.

```c
// Pour chaque (i, j) valide :
int polarity = (...);   // déterminé par PS'(Fv) et l'affectation correspondante
tab->dnnf[i*cols+j] = dnnf_make_literal(pool, x, polarity);
```

**Cas feuille clause** : pour chaque shape, stocker une constante.

```c
int c_in_C_prime = (...);   // bit c dans C'
tab->dnnf[i*cols+j] = dnnf_make_constant(pool, c_in_C_prime);
```

**Cas nœud interne** (la grosse boucle DP) : pour chaque shape S = (i, j), créer un OR-gate ; pour chaque triplet générateur trouvé dans la boucle, créer un AND-gate enfant.

```c
// Avant la boucle interne sur les triplets :
DNNFNode* or_v = dnnf_make_or(pool, 8);

for (chaque triplet (idx_c1, idx_c2, j) qui génère (i, j_v)) {
    DNNFNode* child1 = tab_c1->dnnf[idx_c1 * cols_c1 + j_c1];
    DNNFNode* child2 = tab_c2->dnnf[idx_c2 * cols_c2 + j_c2];
    if (!child1 || !child2) continue;  // shape enfant invalide

    DNNFNode* and_node = dnnf_make_and(pool, child1, child2);
    dnnf_or_add_child(or_v, and_node);
}

if (or_v->num_children == 0) {
    tab->dnnf[i*cols+j] = NULL;   // shape sans génération valide
} else {
    tab->dnnf[i*cols+j] = or_v;
}
```

**Lecture du résultat** (à la fin de `solve_dp`) : `pool->root = tab_root->dnnf[0]`. Stocker dans le résultat retourné.

### 1.4. Modifications de `main.c`

Étendre `DPResult` :

```c
typedef struct {
    long long maxsat_value;
    long long sharpsat_count;
    DNNFNode* dnnf_root;        // NOUVEAU : racine de la d-DNNF compilée
    int dnnf_size;              // nombre d'arêtes du circuit
} DPResult;
```

Afficher la taille du circuit dans le résumé. Optionnellement, dumper le DAG en `.dot` si une variable d'env `DNNF_DUMP=1` est set.

### 1.5. Critères de validation Phase 1

- [ ] `make rebuild` compile sans warning.
- [ ] Sur `exemple1.cnf` (mode manual), `dnnf_count(root, 5) == 17`.
- [ ] Sur `exemple1.cnf` (mode greedy), `dnnf_count == 17`.
- [ ] Sur `exemple1.cnf`, `dnnf_size <= 7 * ps_width^3 * (5 + 4)` (borne du Lemme 7).
- [ ] Sur `random_k3_v8_c20.cnf`, `dnnf_count == sharpsat_count` retourné par le DP.
- [ ] Aucune fuite mémoire détectable (compiler avec `-fsanitize=address` et tester).
- [ ] Le DAG dumpé en `.dot` se visualise avec Graphviz et a la forme attendue.

### 1.6. Démo prof (mercredi S10)

Préparer un mini-rapport (1 page) :
1. « J'ai ajouté le module `dnnf.{h,c}` qui implémente la construction des Lemmes 4-7 de Bova et al. »
2. « Sur exemple1, j'obtiens un DAG de N nœuds qui retourne #SAT = 17 quand on le parcourt avec ∨ → +, ∧ → ×. »
3. « Sur les instances Type 3 t=3 s=2, la taille mesurée est cohérente avec la borne O(k³(n+m)). »
4. Montrer une visualisation Graphviz du circuit pour exemple1.

---

## Phase 2 — Semaine 10 : Requêtes sur le DAG

### Objectif

Implémenter les requêtes polytime supportées par d-DNNF, sans toucher à la procédure de compilation. Toutes les opérations dans `dnnf.c`.

### 2.1. Counting (déjà esquissé en Phase 1 pour validation)

```c
long long dnnf_count(DNNFNode* root, int num_vars);
```

Parcours bottom-up avec mémoïsation (sur `node->id`). Smoothing : pour chaque variable absente d'un sous-circuit, multiplier par 2.

**Test :** `dnnf_count` doit donner exactement le même résultat que `solve_dp().sharpsat_count` sur toutes les instances du benchmark.

### 2.2. Find a model

```c
int* dnnf_find_model(DNNFNode* root, int num_vars);  // retourne tableau [n+1] avec valeurs 0/1
```

Parcours top-down :
- À un AND : descendre dans tous les enfants.
- À un OR : descendre dans le premier enfant dont `dnnf_count > 0`.
- À un littéral : fixer `model[var] = polarity`.
- À ⊤ : rien à faire.
- À ⊥ : impossible (le parent OR aurait dû filtrer).

**Test :** vérifier que le modèle retourné satisfait bien la formule originale (re-parser le CNF, évaluer).

### 2.3. Énumération de modèles

```c
typedef void (*model_callback)(const int* model, int num_vars, void* user_data);
void dnnf_enumerate(DNNFNode* root, int num_vars, model_callback cb, void* user_data);
```

Parcours top-down avec backtracking. À chaque OR, itérer sur tous les enfants. À chaque AND, énumérer le produit cartésien des modèles des enfants.

**Test :** sur exemple1, l'énumération doit produire exactement 17 modèles distincts. Comparer avec une énumération brute-force.

### 2.4. Conditioning (projection sur affectation partielle)

```c
DNNFNode* dnnf_condition(DNNFPool* pool, DNNFNode* root, int var, int value);
```

Construit un nouveau DAG où la variable `var` est fixée à `value` :
- Toute feuille `LIT_POS(var)` devient `value ? TRUE : FALSE`.
- Toute feuille `LIT_NEG(var)` devient `value ? FALSE : TRUE`.
- Propagation des constantes : un AND avec un enfant FALSE → FALSE ; un OR avec un enfant TRUE n'est pas simplifié (il reste OR — sinon on perd la déterminisme structurelle).
- Le nouveau DAG est encore une d-DNNF.

**Test :** sur exemple1, `dnnf_count(condition(root, 1, 1))` doit donner #SAT(F | x₁=1) = 13 (vérifiable par brute-force).

### 2.5. Forgetting (quantification existentielle)

```c
DNNFNode* dnnf_forget(DNNFPool* pool, DNNFNode* root, int var);
```

Calcule $\exists x. F$ en construisant `condition(root, var, 0) ∨ condition(root, var, 1)`. **Attention** : le résultat est une DNNF, plus nécessairement déterministe (le déterminisme se perd ici — c'est documenté par Darwiche & Marquis).

**Test :** `dnnf_count(forget(root, 1)) ` ≥ `dnnf_count(condition(root, 1, 0))` (l'existentielle a plus de modèles que la restriction).

### 2.6. Critères de validation Phase 2

- [ ] `dnnf_count` matche exactement `sharpsat_count` sur les 30+ instances du benchmark.
- [ ] `dnnf_find_model` retourne un modèle qui satisfait la formule (vérification par parser).
- [ ] `dnnf_enumerate` produit exactement #SAT modèles distincts sur exemple1, type1_v50.
- [ ] `dnnf_condition` donne le bon compte sur 5 cas test choisis manuellement.
- [ ] `dnnf_forget` est cohérent avec `condition(0) ∨ condition(1)`.

### 2.7. Démo prof (mercredi S11)

Tableau récapitulatif :

| Requête | exemple1 | type3_n30 | random_k3_v8 |
|---------|----------|-----------|--------------|
| #SAT (DP) | 17 | ? | ? |
| #SAT (DAG) | 17 ✓ | ? ✓ | ? ✓ |
| Modèle trouvé | ✓ | ✓ | ✓ |
| Énumération | 17 modèles ✓ | ? ✓ | ? ✓ |
| Conditioning x₁=1 | 13 ✓ | ? | ? |

---

## Semaine 11 — Benchmarks et figures

### Mesures à collecter (par instance)

- $n, m$ (variables, clauses)
- ps-width (calculée)
- temps de compilation (ms)
- $|D|$ : nombre de nœuds, nombre d'arêtes
- borne théorique $7k^3(n+m)$
- temps d'une requête #SAT sur le DAG
- ratio `temps_DAG / temps_DP_direct`

### Script `data/script/bench_compilation.py`

À écrire en Python : appelle `./sat_solver` sur chaque instance avec un nouveau mode `compile`, parse la sortie, écrit un CSV. Déjà 35+ instances dans le benchmark existant.

### Figures pour le rapport

1. **Taille du DAG vs ps-width** (log-log) — vérifier la borne $k^3$ empiriquement.
2. **Temps de compilation vs $n$** (par famille d'instances).
3. **Comparaison temps_compilation + temps_requête vs temps_DP_seul** (montrer le break-even).
4. **Compression** : ratio `2^n / |D|` pour les instances à ps-width bornée.

---

## Semaines 12-13 — Rédaction

### Semaine 12

- **Chapitre 3 (Implémentation)** : architecture, structures de données, modifications par phase, choix techniques.
  - Section dédiée à la construction du DAG (Lemmes 4-7 illustrés sur exemple1).
- **Chapitre 4 (Résultats expérimentaux)** : tableaux de mesures, graphiques, comparaison borne théorique vs mesure.

### Semaine 13

- **Chapitre 5 (Discussion)** : forces, limites, comparaison qualitative avec c2d/D4 (sans implémentation, juste positionnement).
- **Chapitre 6 (Conclusion + perspectives)** : ce qu'on a fait, ce qui reste, pistes (compilation incrémentale, autres paramètres structurels).
- **Relecture critique de l'intro et de l'état de l'art** : ces chapitres ont été générés en partie par IA et n'ont jamais été vraiment validés. Vérifier chaque référence, chaque affirmation, reformuler les passages flous.
- Relecture biblio, abstract, table des matières.

---

## Semaine 14 — Finalisation

- Corrections orthographiques (LanguageTool, relecture humaine).
- Formatage final : marges, en-têtes, numéros de page, table des figures, biblio.
- Vérification du PDF (rendus de figures, hyperliens cliquables).
- Soumission.

---

## Risques et mitigations

| Risque | Probabilité | Mitigation |
|--------|-------------|------------|
| Phase 1 déborde sur S10 | Moyenne | Si `dnnf_count` ne marche pas en fin de S9, repousser conditioning/forgetting en S11. |
| Bug subtil de déterminisme dans le DAG | Élevée | Test systématique : `dnnf_count == sharpsat` sur tout le benchmark dès S9. |
| Memory leak sur grands circuits | Moyenne | Compiler avec ASan dès le début. |
| Rédaction tardive | Élevée | Démarrer le chapitre 3 dès S10 en parallèle de la Phase 2. |
| Taille DAG explosive sur Tseytin | Faible (attendu) | Documenter comme limitation théorique (cohérent avec Bova et al. Section 5). |

---

## Fichiers à créer / modifier

### À créer
- `src/utils/dnnf.h` (~80 lignes)
- `src/utils/dnnf.c` (~400 lignes au total, étalées sur S9 et S10)
- `data/script/bench_compilation.py`
- `docs/result/benchmarks_kc.md` (mesures brutes)

### À modifier
- `src/utils/dp_table.h` : ajouter `DNNFNode** dnnf`
- `src/utils/dp_table.c` : init/free
- `src/algo/procedure3.c` : émission des nœuds (modification principale, ~50-80 lignes ajoutées)
- `src/algo/procedure3.h` : nouveau champ dans `DPResult`
- `src/main.c` : nouveau mode `compile`, affichage des stats DAG, dump dot
- `src/makefile` : ajouter `utils/dnnf.c` aux sources

### À ne PAS modifier (déjà OK)
- `src/utils/bitset.{h,c}`
- `src/utils/trie.{h,c}`
- `src/utils/ps_set.{h,c}`
- `src/utils/reverse_maps.{h,c}`
- `src/algo/procedure1.{h,c}`, `src/algo/procedure2.{h,c}`
- `src/core/*`
