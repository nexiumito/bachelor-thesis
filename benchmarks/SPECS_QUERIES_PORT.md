# SPECS_QUERIES_PORT — Port des 3 requêtes manquantes (CE, IM, ME multi)

Document de spécifications pour porter les requêtes Darwiche-Marquis manquantes
du repo privé `bachelor-thesis-private/` vers le repo public. Ne contient que
des specs, pas de code. Format identique à `benchmarks/SPECS_FIX_RUN1.md` /
`SPECS_FIX_RUN2.md`.

Source : audit de session S10 (4 requêtes implémentées sur le public — CO, VA,
CT, ME variante 1-modèle ; 3 requêtes manquantes implémentées sur le privé —
CE, IM, ME variante multi-modèles ; helpers `dnnf_condition` et `dnnf_smooth`
manquants).

---

## Sommaire

| ID | Titre | Sévérité | Effort | Dépend de |
|---|---|---|---|---|
| Q1 | Audit préalable et matrice de divergences privé↔public | bloquant | M | — |
| Q2 | Étendre `DNNFPool` (hashcons, scope_by_id, scope_capacity, num_vars) | critique | S | Q1 |
| Q3 | Exposer `allocate_raw_node` + `pool_register_node` non-`static` | critique | S | Q2 |
| Q4 | Migrer `dnnf_transform.{h,c}` (compress, smooth, condition, free_extras) | critique | L | Q2, Q3 |
| Q5 | Migrer `query/entails.{h,c}` (CE) | important | S | Q4 |
| Q6 | Migrer `query/is_implicant.{h,c}` (IM) | important | S | Q4 |
| Q7 | Migrer `query/enumerate.{h,c}` (ME multi) — étend `find_model` | important | M | Q4 |
| Q8 | `main.c` : CLI dispatch + flag `--json-with-queries` | critique | M | Q5, Q6, Q7 |
| Q9 | Mise à jour du Makefile | critique | S | Q4, Q5, Q6, Q7 |
| Q10 | Suite de validation cross-Z3 (`tests/test_queries_z3.py`) | critique | M | Q5..Q9 |
| Q11 | Mise à jour `README.md` + `.claude/CLAUDE.md` | important | S | Q10 |

Notation effort : **S** = ≤ 30 min, **M** = 1–3 h, **L** = ½–1 jour.

---

## Avertissements préliminaires (à lire avant toute action)

Les divergences ci-dessous interdisent un copier-coller. Toute entrée Q* doit
les respecter.

### A1. Discipline `alloc_failed` du public — à NE PAS casser

Le public a été durci post-bug B3 (cf. `benchmarks/SPECS_FIX_RUN1.md` §B3). Dans
`src/utils/dnnf.c` :

- `allocate_raw_node` retourne `NULL` sur échec (pas de `exit`).
- `pool_register_node` retourne `int` (`0` succès, `-1` échec ; lève
  `pool->alloc_failed`).
- Les factories `dnnf_make_*` testent `pool->alloc_failed` en entrée et
  renvoient `pool->node_false` si levé (sémantique conservatrice : 0 modèle).
- `create_dnnf_pool` retourne `NULL` propre sur échec d'allocation initiale.
- Le main vérifie `dp_result.alloc_failed` puis appelle
  `print_json_error("alloc_fail", ...)` avant `return 1`.

Le privé **ne respecte pas cette discipline** : `pool_register_node` est `void`,
allocation supposée infaillible. Toute fonction portée doit être adaptée pour
tester `pool->alloc_failed` à l'entrée et early-bail vers `pool->node_false`,
propager les `NULL` retournés par `allocate_raw_node`, et utiliser le retour
`int` de `pool_register_node`.

### A2. Signature `dnnf_count` — divergente entre privé et public

- **Public** : `long long dnnf_count(DNNFNode* root, DNNFPool* pool, int* overflow_out);`
  — détection sticky d'overflow via `__builtin_mul_overflow`, saturation à
  `LLONG_MAX`. Critique pour I1 (`dnnf_count_match`).
- **Privé** : `long long dnnf_count(DNNFNode* root, DNNFPool* pool);` — sans
  détection.

Les requêtes du privé qui appellent `dnnf_count(current, pool)` doivent passer
`NULL` ou `&local_overflow` selon le besoin de propager l'overflow.

### A3. Champs auxiliaires `DNNFPool` — absents du public

Le privé étend `DNNFPool` avec `DNNFHashTable* hashcons`, `Bitset** scope_by_id`,
`int scope_capacity`, `int num_vars`. Indispensables à `dnnf_transform.c`. Leur
ajout au public doit préserver la rétro-compatibilité (initialisation à `NULL`
dans `create_dnnf_pool`, libération via `dnnf_transform_free_pool_extras`
appelé depuis `free_dnnf_pool`).

### A4. Helpers `allocate_raw_node` / `pool_register_node` — `static` côté public

`static` dans `src/utils/dnnf.c` du public, exposés (non-`static`, `extern`
côté `dnnf_transform.c`) sur le privé. L'exposition publique doit conserver les
signatures durcies (retour `NULL` / `int`).

### A5. Inclusion croisée `bitset.h` dans `dnnf.h`

Le privé fait `#include "bitset.h"` au sommet de `dnnf.h` (pour `Bitset*` dans
`scope_by_id`). À ajouter au public lors de Q2.

---

## Q1 — Audit préalable et matrice de divergences privé↔public

- **Sévérité** : bloquant (à exécuter avant tout port)
- **Effort** : M
- **Fichiers concernés** :
  - `bachelor-thesis-private/src/utils/dnnf.{h,c}` (référence)
  - `bachelor-thesis-private/src/utils/dnnf_transform.{h,c}` (référence)
  - `bachelor-thesis-private/src/utils/query/{count,consistency,validity,find_model,entails,is_implicant,enumerate}.{h,c}` (référence)
  - `bachelor-thesis/src/utils/dnnf.{h,c}` (cible actuelle)
  - `bachelor-thesis/src/utils/query/{count,consistency,validity,find_model}.{h,c}` (cible actuelle)
  - `bachelor-thesis/src/main.c` (cible actuelle)

### Diagnostic

L'écart privé↔public n'est pas seulement un manque de fichiers — c'est aussi un
écart d'invariants (cf. avertissements A1–A5). Sans audit ligne à ligne, on
risque (1) d'écraser des défenses durcies du public (`alloc_failed` discipline),
(2) de casser les invariants du run 3 (I1 `dnnf_count_match`, I2 borne BCMS),
(3) d'introduire des leaks que ASan détecterait.

### Spec du fix

1. Pour chaque fichier listé ci-dessus, exécuter
   `diff bachelor-thesis-private/src/<f> bachelor-thesis/src/<f>` et **archiver**
   le diff dans un fichier de travail temporaire `AUDIT_DIFF.md` à la racine du
   repo public.
2. Produire dans `AUDIT_DIFF.md` une matrice avec une ligne par divergence,
   colonnes : `Fichier` / `Lignes privé` / `Lignes public` / `Nature de la
   divergence` / `Décision retenue` / `Justification`.
3. Pour chaque divergence, **trancher** explicitement entre :
   - **Garder le public** (cas typique : durcissement défensif post-B3),
   - **Adopter le privé** (cas typique : nouvelle fonctionnalité strictement
     absente du public),
   - **Hybrider** (signature privée + comportement durci du public, p.ex.
     `count.{h,c}` avec sticky overflow).
4. Cas particuliers à trancher explicitement :
   - **`dnnf.h`** : ajout des champs `hashcons`, `scope_by_id`, `scope_capacity`,
     `num_vars`, plus la forward declaration
     `struct dnnf_hash_table; typedef struct dnnf_hash_table DNNFHashTable;` et
     le `#include "bitset.h"`. **Adopter du privé** (Q2).
   - **`dnnf.c`** : passage de `static` à non-`static` pour `allocate_raw_node`
     et `pool_register_node`. **Hybrider** : exposer (extern) MAIS conserver
     les retours `NULL` / `int` du public, **pas** les versions infaillibles
     du privé (Q3).
   - **`free_dnnf_pool`** : ajout d'un appel
     `dnnf_transform_free_pool_extras(pool)` en tête (libère hashcons +
     scope_by_id si alloués). **Adopter du privé** (Q4).
   - **`count.{h,c}`** : signature publique (avec `int* overflow_out`)
     **conservée** (référence : `find_model.c` public ligne 54,
     `consistency.c` public ligne 8). Toutes les requêtes portées doivent
     passer `NULL` ou un local. Aucun changement à `count.{h,c}` (déjà OK
     côté public).
5. Confirmation finale : aucun chantier de Q2..Q11 ne doit toucher la signature
   externe de `dnnf_count`, `dnnf_consistency`, `dnnf_validity`,
   `dnnf_find_model` du public.

### Validation

```bash
# AUDIT_DIFF.md existe a la racine
test -f AUDIT_DIFF.md && echo "OK" || echo "MANQUANT"

# Aucun exit() parasite a porter
grep -n "exit(" bachelor-thesis-private/src/utils/dnnf.c \
                bachelor-thesis-private/src/utils/dnnf_transform.c
# Attendu : aucune ligne (sinon flagger comme "a durcir" dans AUDIT_DIFF.md)
```

### Dépendances

Aucune (entrée racine).

---

## Q2 — Étendre `DNNFPool` avec les champs auxiliaires

- **Sévérité** : critique (prérequis structural pour Q4..Q7)
- **Effort** : S
- **Fichiers concernés** :
  - `bachelor-thesis/src/utils/dnnf.h` (à modifier)
  - `bachelor-thesis/src/utils/dnnf.c` (à modifier — `create_dnnf_pool`)
  - `bachelor-thesis-private/src/utils/dnnf.h` lignes 6, 65–93 (référence)

### Diagnostic

Sans ces 4 champs, `dnnf_transform.c` ne compile pas (`dnnf_compute_scopes` écrit
dans `pool->scope_by_id`, `dnnf_compress` initialise `pool->hashcons` lazy).

### Spec du fix

1. Dans `src/utils/dnnf.h` :
   - Ajouter en haut : `#include "bitset.h"` (juste après `#include <stdio.h>`).
   - Ajouter une forward declaration **avant** `typedef struct { … } DNNFPool;` :
     ```c
     // Forward declaration de la table de hash-consing (dnnf_transform.c).
     struct dnnf_hash_table;
     typedef struct dnnf_hash_table DNNFHashTable;
     ```
   - Dans `DNNFPool`, ajouter en queue (après `int alloc_failed;`) :
     ```c
     // Champs auxiliaires installes lazy par dnnf_transform :
     //   hashcons       : table de hash-consing (NULL avant 1er compress).
     //   scope_by_id    : Bitset par noeud (NULL avant 1er smooth).
     //   scope_capacity : taille de scope_by_id.
     //   num_vars       : capture au 1er compute_scopes (necessaire au smoothing).
     DNNFHashTable*  hashcons;
     Bitset**        scope_by_id;
     int             scope_capacity;
     int             num_vars;
     ```
2. Dans `src/utils/dnnf.c`, fonction `create_dnnf_pool` : initialiser les 4
   nouveaux champs à `NULL` / `0` après l'initialisation de `alloc_failed`.
3. **Ne pas** modifier `free_dnnf_pool` ici — délégué à
   `dnnf_transform_free_pool_extras` (introduit en Q4). Ajouter un commentaire
   `// TODO Q4 : appeler dnnf_transform_free_pool_extras(pool);` à l'emplacement.

### Validation

```bash
cd src && make rebuild           # 0 warning sur -Wall -Wextra
./sat_solver ../data/exemple1.cnf greedy --json | python3 -m json.tool
# Attendu : JSON identique au pre-Q2, dnnf_count_match: true
```

### Dépendances

Q1.

---

## Q3 — Exposer `allocate_raw_node` et `pool_register_node`

- **Sévérité** : critique (prérequis pour Q4 — `dnnf_transform.c` les appelle via `extern`)
- **Effort** : S
- **Fichiers concernés** :
  - `bachelor-thesis/src/utils/dnnf.h` (ajout API interne)
  - `bachelor-thesis/src/utils/dnnf.c` (retirer `static`)
  - `bachelor-thesis-private/src/utils/dnnf_transform.c` lignes 13–14 (référence : declarations `extern`)

### Diagnostic

`dnnf_transform.c` (privé) déclare
`extern DNNFNode* allocate_raw_node(void);` et
`extern void pool_register_node(DNNFPool* pool, DNNFNode* node);`. Côté public,
ces deux fonctions sont actuellement `static` dans `dnnf.c`. Pour permettre la
compilation de `dnnf_transform.c`, il faut les rendre visibles **sans**
régresser la discipline `alloc_failed` (avertissement A1).

### Spec du fix

1. Dans `src/utils/dnnf.c` : retirer `static` devant `allocate_raw_node` (ligne
   32) et `pool_register_node` (ligne 52). Conserver les corps actuels (retour
   `NULL` et retour `int` durcis post-B3).
2. Dans `src/utils/dnnf.h`, ajouter à la fin (après les prototypes existants),
   dans une section commentée explicitement comme **interne** :
   ```c
   // ============================================================================
   // API INTERNE : helpers exposes uniquement pour dnnf_transform.c
   // ============================================================================
   // NE PAS appeler depuis main.c, procedure3.c, ou les requetes : utiliser les
   // factories dnnf_make_* qui appliquent les simplifications locales et la
   // discipline alloc_failed. Ces helpers existent uniquement pour permettre a
   // dnnf_compress de construire des noeuds bruts sans simplification (les
   // factories sont incompatibles avec le hash-consing).

   /**
    * Alloue un DNNFNode brut (champs id, type, var_index, children,
    * num_children, capacity non initialises ; a la charge du caller).
    * Retourne NULL si malloc echoue (le caller doit lever pool->alloc_failed).
    */
   DNNFNode* allocate_raw_node(void);

   /**
    * Enregistre node dans pool->nodes, lui assigne id = pool->num_nodes, double
    * la capacite si besoin. Retourne 0 sur succes, -1 sur echec (alloc_failed
    * leve). Sur echec, le caller doit liberer node lui-meme.
    */
   int pool_register_node(DNNFPool* pool, DNNFNode* node);
   ```

### Validation

```bash
cd src && make rebuild           # 0 warning
nm utils/dnnf.o | grep -E "allocate_raw_node|pool_register_node"
# Attendu : symboles en T (texte global), pas en t (local)

./sat_solver ../data/exemple1.cnf greedy --json
# Attendu : JSON identique au pre-Q3
```

### Dépendances

Q2.

---

## Q4 — Migrer `dnnf_transform.{h,c}` (compress, smooth, condition, free_extras)

- **Sévérité** : critique (prérequis pour Q6 et Q7 — IM et CE l'utilisent)
- **Effort** : L (730 lignes au total à porter avec adaptations alloc_failed)
- **Fichiers concernés** :
  - `bachelor-thesis/src/utils/dnnf_transform.h` (à créer)
  - `bachelor-thesis/src/utils/dnnf_transform.c` (à créer)
  - `bachelor-thesis/src/utils/dnnf.c` (modification de `free_dnnf_pool`)
  - `bachelor-thesis/src/utils/dnnf.h` (mise à jour commentaire en tête)
  - `bachelor-thesis-private/src/utils/dnnf_transform.{h,c}` (source, 730 lignes)

### Diagnostic

Sans ce module, ni CE ni IM ne sont implémentables polytime sur d-DNNF :

- CE : `count(condition(F, ¬c)) == 0`
- IM : `count(smooth(condition(F, γ))) == 2^(n-k)`

### Spec du fix

**Décisions à graver dans la spec (issue de l'audit Q1) :**

- **Immutabilité du DAG d'origine** : `dnnf_condition` et `dnnf_smooth`
  **n'écrivent pas** dans les nœuds existants. Ils construisent de nouveaux
  nœuds dans le pool partagé (la table `pool->nodes` grandit). Le DAG d'origine
  reste valide après l'opération (c'est ce qui permet de bencher des requêtes
  successives sans recompiler).
- **`dnnf_smooth` automatique vs explicite** : appliqué **uniquement** par
  `is_implicant.c` (besoin sémantique strict). Pas appliqué par CE
  (`entails.c`), pour lequel un compte = 0 reste = 0 indépendamment du lissage.
  Pas appliqué automatiquement par les factories ni par `dnnf_condition`.
- **`dnnf_compress`** : NE PAS exposer en CLI dans cette spec. Reste un outil
  interne ; intégration éventuelle (post-DP, optionnelle, comme optimisation de
  taille DAG) sort du scope de Q4.

**Mise en œuvre :**

1. Créer `src/utils/dnnf_transform.h` en copiant le fichier privé tel quel
   (préserver les `#include "dnnf.h"` et les commentaires de référence Bova
   2016, Darwiche-Marquis Lemme A.1, Définition 5.4).
2. Créer `src/utils/dnnf_transform.c` en partant du privé avec les
   **adaptations obligatoires** :
   - **Lignes 13–14 (extern)** : conserver telles quelles.
   - **Section 1 (hash-consing, lignes 21–333)** : porter sans modification.
     Indépendant de `alloc_failed`.
   - **`compress_alloc_internal` (lignes 187–203)** : ajouter en tête
     `if (pool->alloc_failed) return pool->node_false;`. Vérifier le retour
     de `allocate_raw_node` (peut être `NULL` côté public) et de
     `pool_register_node` (peut retourner `-1`) ; en cas d'échec, lever
     `pool->alloc_failed` et retourner `pool->node_false`.
   - **Section 2 (scopes + smooth, lignes 336–570)** : porter sans
     modification. Les appels à `dnnf_make_or`, `dnnf_or_add_child`,
     `dnnf_make_and` héritent du durcissement public.
   - **Section 3 (condition, lignes 573–715)** : porter sans modification.
     **Vérifier** explicitement le commentaire ligne 581–583 ("ne pas
     simplifier OR(TRUE, X) en TRUE") — le maintenir, c'est le piège qui
     interagit avec smooth.
   - **`dnnf_transform_free_pool_extras` (lignes 722–729)** : porter tel quel.
3. Dans `src/utils/dnnf.c`, modifier `free_dnnf_pool` :
   - Ajouter `#include "dnnf_transform.h"` en tête.
   - Au début de `free_dnnf_pool`, juste après le test `if (!pool) return;`,
     ajouter `dnnf_transform_free_pool_extras(pool);`.
4. Dans `src/utils/dnnf.h`, mettre à jour le commentaire en tête : remplacer
   `// REQUETES (CO, VA, CT, ME) : voir utils/query/.` par
   `// REQUETES (CO, VA, CT, ME, CE, IM) : voir utils/query/.\n// TRANSFORMATIONS internes (compress, smooth, condition) : voir\n// utils/dnnf_transform.{h,c}.`

### Validation

```bash
# Compilation : ajouter manuellement utils/dnnf_transform.c a SRCS
# (Q9 le fera definitivement) puis :
cd src && make rebuild

# Test unitaire ad-hoc (driver tests/test_transform.c) :
#   1. Construire OR(AND(x1, x2), AND(~x1, x3)) — non lisse (x2 absent de la
#      2e branche, x3 absent de la 1ere).
#   2. Compter avant smooth : attendu 2 (1 modele par branche).
#   3. Lisser via dnnf_smooth(root, pool, 3).
#   4. Compter apres smooth : attendu 4 (chaque branche doublee par la
#      variable libre manquante).
#   5. Conditionner sur x1=1 via dnnf_condition(smoothed, pool, 1, 1).
#   6. Compter apres condition : attendu 2 (la branche AND(~x1, x3) collapse).

# Validation memoire :
make asan
./sat_solver ../data/exemple1.cnf greedy 2>&1 | grep -i sanitizer
./sat_solver ../data/type3/type3_n30_t3_s2.cnf greedy 2>&1 | grep -i sanitizer
# Attendu : 0 fuite, 0 use-after-free
```

### Dépendances

Q2, Q3.

---

## Q5 — Migrer `query/entails.{h,c}` (CE)

- **Sévérité** : important (requête utilisateur)
- **Effort** : S
- **Fichiers concernés** :
  - `bachelor-thesis/src/utils/query/entails.h` (à créer)
  - `bachelor-thesis/src/utils/query/entails.c` (à créer)
  - `bachelor-thesis-private/src/utils/query/entails.{h,c}` (source)

### Diagnostic

CE est polytime sur d-DNNF par le théorème CO+CD ⇒ CE de Darwiche-Marquis 2002
(page 240, Lemme A.3 + Table 7). Réduction utilisée :
$$
F \models (\ell_1 \lor \dots \lor \ell_k) \iff
F \land \neg \ell_1 \land \dots \land \neg \ell_k \text{ est UNSAT} \iff
\mathrm{count}(\mathrm{condition}(\dots)) = 0.
$$

### Spec du fix

1. Créer `src/utils/query/entails.h` en copiant `bachelor-thesis-private/src/utils/query/entails.h`
   tel quel (codes de retour `DNNF_ENTAILS_YES/NO/VACUOUSLY/BAD_VAR`, signature
   `int dnnf_entails(DNNFNode*, DNNFPool*, const int*, int, int, long long*)`).
2. Créer `src/utils/query/entails.c` en partant du privé (30 lignes) avec les
   adaptations :
   - **Ligne 27 du privé** : `long long c = dnnf_count(current, pool);` →
     `long long c = dnnf_count(current, pool, NULL);` (signature `count`
     durcie côté public, A2).
   - **Considérer** l'ajout d'un code `DNNF_ENTAILS_OVERFLOW = -3` : passer
     `int local_overflow = 0` à `dnnf_count`. Si `local_overflow == 1` et
     `c > 0`, on ne peut pas distinguer "0 modèle" de "saturé à LLONG_MAX" →
     renvoyer `DNNF_ENTAILS_OVERFLOW`. Si `c == 0`, l'overflow est silencieux
     (`0` reste `0`) → renvoyer `DNNF_ENTAILS_YES`. **Trancher** lors de la
     mise en œuvre : recommandé d'ajouter ce code (cohérent avec
     `dnnf_validity` qui a déjà `DNNF_VALIDITY_OVERFLOW`). Ajouter le
     `#define DNNF_ENTAILS_OVERFLOW -3` dans le `.h`.
3. Le `.h` doit documenter explicitement que `literals` est un tableau
   **DIMACS** (entiers non nuls, jamais 0).

### Validation

```bash
cd src
# F entraine la clause (x1 v x2) ?
./sat_solver ../data/exemple1.cnf greedy entails 1 2

# Cross-check Z3 (oracle) :
python3 -c "
from z3 import Solver, Bool, Or, Not, And, sat
# Lire exemple1.cnf, ajouter Not(Or(x1, x2)), verifier UNSAT.
# Si Z3 dit UNSAT, dnnf_entails doit retourner YES (1).
"

# Sanity systematique : F entraine trivialement chacune de ses propres clauses.
# Pour 5 instances petites, choisir 3 clauses au hasard de chaque, verifier
# que dnnf_entails == YES (code 1) a chaque fois.
```

### Dépendances

Q4 (utilise `dnnf_condition`).

---

## Q6 — Migrer `query/is_implicant.{h,c}` (IM)

- **Sévérité** : important (requête utilisateur)
- **Effort** : S
- **Fichiers concernés** :
  - `bachelor-thesis/src/utils/query/is_implicant.h` (à créer)
  - `bachelor-thesis/src/utils/query/is_implicant.c` (à créer)
  - `bachelor-thesis-private/src/utils/query/is_implicant.{h,c}` (source)

### Diagnostic

IM polytime sur d-DNNF (Darwiche-Marquis Table 5). Équivalence utilisée :
$$
\gamma \models F \iff \#\mathrm{SAT}(F \mid \gamma) = 2^{n-k}
$$
où $n$ = `num_vars` et $k$ = nombre de variables distinctes dans γ. Le
conditionnement peut casser la lissité ; lissage avant comptage est obligatoire.

### Spec du fix

1. Créer `src/utils/query/is_implicant.h` en copiant le fichier privé tel quel
   (codes `DNNF_IS_IMPLICANT_YES/NO/UNSAT/BAD_VAR/OVERFLOW`).
2. Créer `src/utils/query/is_implicant.c` en partant du privé (60 lignes) avec :
   - **Ligne 56 du privé** : `long long c = dnnf_count(smoothed, pool);` →
     `long long c = dnnf_count(smoothed, pool, NULL);` (A2).
   - **Considérer** la propagation overflow : si `dnnf_count` overflow et
     `target > 0`, on ne peut pas trancher ; si `c` saturé == `target`, faux
     YES. Recommandé : passer `int local_overflow = 0`, et si levé renvoyer
     `DNNF_IS_IMPLICANT_OVERFLOW` (le code existe déjà dans le `.h` privé).
     Mettre à jour le commentaire du `.h` pour clarifier que ce code couvre
     aussi ce cas (actuellement il ne couvre que `n - k >= 63`).
3. Vérifier la libération de `seen` (privé ligne 38) — pas de leak introduit.

### Validation

```bash
cd src

# γ = ∅ (terme vide) : IM doit retourner YES ssi F est tautologie.
# Pour exemple1 (17 modeles, 2^5 = 32) : doit retourner NO.
./sat_solver ../data/exemple1.cnf greedy is_implicant     # γ = ∅

# γ = {x1=1} : compte F | x1=1, doit etre 2^4 = 16 si γ ⊨ F.
./sat_solver ../data/exemple1.cnf greedy is_implicant 1

# Cross-check Z3 : γ ⊨ F ⇔ Not(Implies(And(γ), F)) est UNSAT.
python3 -c "
from z3 import Solver, Bool, Implies, And, Not
# Pour gamma = {x1}, verifier que And(x1) -> F est tautologie.
"

# Sanity : sur tout modele M de F obtenu via find_model, le terme M complet
# est trivialement implicant (toutes les vars fixees, count = 1 = 2^0).
```

### Dépendances

Q4 (utilise `dnnf_condition` ET `dnnf_smooth`).

---

## Q7 — Migrer `query/enumerate.{h,c}` (ME multi) — étend `find_model`, ne le remplace pas

- **Sévérité** : important (requête utilisateur)
- **Effort** : M (décision architecturale + tests interruption)
- **Fichiers concernés** :
  - `bachelor-thesis/src/utils/query/enumerate.h` (à créer)
  - `bachelor-thesis/src/utils/query/enumerate.c` (à créer)
  - `bachelor-thesis/src/utils/query/find_model.{h,c}` (inchangés — décision argumentée ci-dessous)
  - `bachelor-thesis-private/src/utils/query/enumerate.{h,c}` (source)

### Diagnostic

ME polytime sur d-DNNF (Lemme A.3 page 244). `enumerate` produit cartésiens aux
AND k-aires en CPS (continuation passing style).

### Spec du fix

**Décision : on ÉTEND, on ne REMPLACE PAS.** Justification :

- `find_model` est en O(|D|) et retourne **un seul** modèle. API simple,
  signature compacte (`int* model_out` pré-alloué par le caller). Très utilisé
  en CLI (cf. PREPA_MEETING_2026-04-29.md).
- `enumerate` énumère **tous** les modèles via callback. Sa signature
  (`dnnf_model_callback cb`, `void* user_data`) est plus lourde et
  interromp-able. Strictement plus expressive mais imposerait au caller de
  gérer une callback même pour un seul modèle.
- Coexistence : deux cas d'usage distincts, pas de duplication sémantique.
- **Côté bench** : c'est `enumerate` qu'on chronométrera pour ME (cf.
  `benchmarks/SPECS_QUERY_BENCH.md` Q-B3). `find_model` reste timable
  séparément (1 modèle vs énumération complète).

**Mise en œuvre :**

1. Créer `src/utils/query/enumerate.h` en copiant le fichier privé tel quel
   (typedef `dnnf_model_callback`, signature
   `long long dnnf_enumerate(DNNFNode*, DNNFPool*, int, dnnf_model_callback, void*)`).
2. Créer `src/utils/query/enumerate.c` en copiant le fichier privé (122 lignes)
   tel quel — aucune adaptation nécessaire (n'appelle ni `dnnf_count` ni les
   factories, juste `n->type` et `n->children`).
3. **Ne pas toucher** `src/utils/query/find_model.{h,c}`.
4. Conserver le commentaire d'avertissement "PIEGE : sur un DAG NON LISSE..."
   (privé ligne 18) dans le `.h`.

### Validation

```bash
cd src

# F a 17 modeles (verifier via cross-check counter callback).
# Attendu : compter via callback == 17 sur exemple1 greedy.
./sat_solver ../data/exemple1.cnf greedy enumerate

# Test interruption : callback retourne 1 au 5e modele, verifier que
# dnnf_enumerate retourne 5 (pas 17).

# Cross-check sur 3 instances supplementaires : enumerate count == dnnf_count
# exactement sur DAG lisse construit par phase 3.
# Si l'invariant echoue, c'est un signal qu'il faut smoother avant — trancher
# entre wrapper enumerate_smoothed (Q7.a) ou laisser au caller (Q7.b, deja
# documente dans le PIEGE du .h).
```

### Dépendances

Q1 (audit). Si décision (a) wrapper `enumerate_smoothed` retenue : ajouter Q4.

---

## Q8 — `main.c` : CLI dispatch + flag `--json-with-queries`

- **Sévérité** : critique (rend les requêtes utilisables en CLI et machine-readable)
- **Effort** : M (refactor du dispatch + nouveau mode JSON enrichi avec timings par requête)
- **Fichiers concernés** :
  - `bachelor-thesis/src/main.c` (lignes 297–353 pour CLI, lignes 588–637 pour dispatch, lignes 870–912 pour JSON)

### Diagnostic

Le main public dispatch actuellement `consistency`, `validity`, `find_model`
via une chaîne de `strcmp` (lignes 343–349 et 590–637). À étendre pour
`entails`, `is_implicant`, `enumerate`. Le `--json` actuel ne contient AUCUNE
information sur les requêtes (lignes 870–912) — à étendre pour permettre au
bench de chronométrer les requêtes individuelles **sur DAG déjà compilé**.

### Spec du fix

#### Q8.1 — CLI dispatch (modes ASCII)

1. Modifier l'aide (lignes 297–316) pour ajouter dans la liste des requêtes :
   - `entails L1 L2 ...    : F entraine-t-elle (L1 v L2 v ...) ? (clause donnee en arguments DIMACS)`
   - `is_implicant L1 L2 ...: terme (L1 ^ L2 ^ ...) est-il implicant de F ?`
   - `enumerate [LIMIT]     : Enumere LIMIT modeles (defaut: tous, max ~1e6 pour eviter le bourrage stdout)`
2. Étendre la validation des arguments (lignes 339–350) :
   - `argc` peut maintenant aller au-delà de 4.
   - Refactoriser : extraire `query.name = argv[3]` et stocker
     `query.argc = argc - 4`, `query.argv = &argv[4]`.
   - Étendre la liste des noms acceptés à `consistency`, `validity`,
     `find_model`, `entails`, `is_implicant`, `enumerate`.
   - Pour `entails` et `is_implicant`, exiger `argc >= 5` (au moins 1 littéral).
   - Pour `enumerate`, `argc` peut être 4 (pas de limit) ou 5 (limit en
     `argv[4]`).
3. Étendre `solve_formula` (lignes 588–637) avec 3 branches supplémentaires :
   - `entails` : parser `argv[4..]` en tableau d'entiers DIMACS, appeler
     `dnnf_entails`, switcher sur les 5 codes (`YES`, `NO`, `VACUOUSLY`,
     `BAD_VAR`, `OVERFLOW`).
   - `is_implicant` : symétrique, appel à `dnnf_is_implicant`.
   - `enumerate` : callback qui imprime le modèle, `limit` cap par défaut à
     1e6, retourne `1` (stop) si `limit` atteint.

#### Q8.2 — JSON `--json-with-queries` enrichi (timings des requêtes)

Le bench du run 4 (cf. `benchmarks/SPECS_QUERY_BENCH.md`) doit pouvoir mesurer
chaque requête individuellement, **sur un DAG déjà compilé**, sans repayer la
phase 3. La meilleure solution architecturale est **in-process** : le binaire C
compile une fois, puis chronomètre toutes les requêtes en interne, et émet les
timings dans le JSON.

1. Ajouter un nouveau **mode JSON enrichi** : flag `--json-with-queries` (en
   plus de `--json`). Le mode `--json` actuel reste **strictement inchangé**
   (rétro-compat impérative pour les CSV des runs 1, 2, 3).
2. Si `--json-with-queries` présent, après la sérialisation actuelle, ajouter
   les champs :
   - `query_co_ms` (float) — temps de `dnnf_consistency`
   - `query_va_ms` (float) — temps de `dnnf_validity`
   - `query_ct_ms` (float) — temps de `dnnf_count` (sur DAG déjà construit)
   - `query_me_ms` (float) — temps de `dnnf_find_model` (1 modèle)
   - `query_ce_ms` (float) — temps de `dnnf_entails` sur clause de référence
   - `query_im_ms` (float) — temps de `dnnf_is_implicant` sur terme de référence
   - `query_enum_first_ms` (float) — temps pour produire le 1er modèle via
     `dnnf_enumerate` (callback qui retourne 1 au 1er appel)
   - `query_enum_all_ms` (float) — temps pour énumérer tous les modèles, capé
     à 1e6 modèles
   - `query_co_result` (int 0/1)
   - `query_va_result` (int parmi VALID/NOT_VALID/UNSAT/OVERFLOW)
   - `query_ce_result` (int parmi YES/NO/VACUOUSLY/BAD_VAR/OVERFLOW)
   - `query_im_result` (int)
   - `query_ct_count` (long long ou "overflow")
   - `query_enum_count` (long long) — = `dnnf_count` sur DAG lisse
   - `query_repetitions` (int) — défaut 5
   - `query_enum_all_skipped` (bool) — `true` si `dnnf_count > 1e6`
3. **Choix des arguments de référence** (paramètres internes, pas exposés CLI) :
   - **CE** : prendre la **première clause** de la formule originale (relire
     le DIMACS via le parser). F entraîne trivialement chacune de ses propres
     clauses → résultat attendu = `YES` systématiquement. Mesure stable.
   - **IM** : prendre le **terme vide** γ = ∅ ; alors
     `IM(γ, F) = YES ⇔ F est tautologie`. Sur les instances du bench, F n'est
     presque jamais tautologie → résultat attendu `NO`. Coût = 1 condition + 1
     smooth + 1 count = O(|D|).
4. **Anti-biais démarrage process** : `query_repetitions` répétitions
   (paramètre, défaut 5). Stocker la médiane. `clock_gettime(CLOCK_MONOTONIC)`
   autour de chaque appel. La 1ère exécution est jetée ; on ne mesure que les
   répétitions 2..N.
5. **Cap sur `query_enum_all_ms`** : si `dnnf_count > 1e6`, ne pas exécuter
   `enumerate(all)`. Émettre `query_enum_all_ms: null` et
   `query_enum_all_skipped: true`. `query_enum_first_ms` reste mesurable.

### Validation

```bash
cd src && make rebuild

# CLI ASCII : 3 nouvelles requetes
./sat_solver ../data/exemple1.cnf greedy entails 1 2
./sat_solver ../data/exemple1.cnf greedy is_implicant 1
./sat_solver ../data/exemple1.cnf greedy enumerate

# JSON strict : retro-compat
./sat_solver ../data/exemple1.cnf greedy --json | python3 -m json.tool
# Attendu : JSON identique au pre-Q8 (memes cles)

# JSON enrichi : nouveaux champs
./sat_solver ../data/exemple1.cnf greedy --json-with-queries | python3 -m json.tool
# Attendu : tous les champs query_*. En particulier query_ct_count == 17,
# query_enum_count == 17, query_co_result == 1, query_va_result == NOT_VALID.
```

### Dépendances

Q5, Q6, Q7.

---

## Q9 — Mise à jour du Makefile

- **Sévérité** : critique (sinon link error)
- **Effort** : S
- **Fichiers concernés** :
  - `bachelor-thesis/src/makefile`

### Diagnostic

`SRCS` actuel ne contient pas `dnnf_transform.c`, `entails.c`, `is_implicant.c`,
`enumerate.c`.

### Spec du fix

1. Dans `src/makefile`, ajouter à `SRCS` (en respectant l'ordre actuel) :
   ```make
   SRCS = main.c \
          utils/bitset.c \
          utils/ps_set.c \
          utils/trie.c \
          utils/dp_table.c \
          utils/reverse_maps.c \
          utils/dnnf.c \
          utils/dnnf_transform.c \
          utils/query/count.c \
          utils/query/consistency.c \
          utils/query/validity.c \
          utils/query/find_model.c \
          utils/query/entails.c \
          utils/query/is_implicant.c \
          utils/query/enumerate.c \
          core/parser.c \
          core/decomposition_tree.c \
          algo/procedure1.c \
          algo/procedure2.c \
          algo/procedure3.c
   ```
2. Conserver les cibles `clean`, `rebuild`, `asan` inchangées.

### Validation

```bash
cd src && make clean && make           # 0 warning
make asan
./sat_solver ../data/exemple1.cnf greedy enumerate 2>&1 | grep -i sanitizer
# Attendu : 0 leak, 0 use-after-free
```

### Dépendances

Q4 (introduit `dnnf_transform.c`), Q5, Q6, Q7.

---

## Q10 — Suite de validation cross-Z3

- **Sévérité** : critique (preuve de correction des nouvelles requêtes)
- **Effort** : M
- **Fichiers concernés** :
  - `bachelor-thesis/tests/test_queries_z3.py` (à créer ; nouveau dossier `tests/`)
  - `bachelor-thesis/benchmarks/runners/run_z3.py` (référence, parser DIMACS et runner Solver)

### Diagnostic

Sans cross-check Z3, les nouvelles requêtes ne sont validées que par cohérence
interne (`dnnf_count` vs `enumerate`). Pour avoir confiance dans CE et IM, il
faut un oracle externe.

### Spec du fix

1. Créer un script Python `tests/test_queries_z3.py` qui :
   - Importe `parse_dimacs` depuis `benchmarks/runners/run_z3.py` (pas de
     duplication).
   - Liste 6 instances **petites** comme suite de test :
     - `data/exemple1.cnf`
     - `data/type1/type1_v20_c25.cnf`
     - `data/type3/type3_n30_t3_s2.cnf`
     - `data/random/random_k3_v8_c20.cnf`
     - `data/type2/type2_v25_c100_t3.cnf`
     - `data/random/random_k3_v15_c40.cnf`
   - Pour chaque instance :
     - **CE** : pour chacune des 3 premières clauses, vérifier
       `./sat_solver <inst> greedy entails <litteraux>` → DOIT renvoyer `YES`.
       Symétriquement, pour 3 clauses **fausses** (= clauses originales avec
       un littéral inversé), vérifier que la réponse est `NO`. Comparaison
       Z3 : `Solver` sur `F ∧ ¬c` doit donner `unsat` pour les vraies,
       `sat` pour les fausses.
     - **IM** : pour 3 termes choisis (terme vide, singleton `{x1}`,
       affectation complète extraite de `dnnf_find_model`) :
       - Terme vide → `NO` (sauf si F tautologie).
       - Singleton → réponse à valider via Z3
         (`Implies(x1, F)` doit être tautologie ssi `IM` répond `YES`).
       - Affectation complète extraite de `find_model` → DOIT renvoyer `YES`.
     - **ME (enumerate)** : compter via callback (mode `--json-with-queries`,
       lire `query_enum_count`) doit égaler `dnnf_count_recomputed` (champ
       déjà présent dans le JSON). Sur instances avec ≤ 1e4 modèles
       uniquement (sinon timeout).
   - Imprimer un rapport ASCII : nombre de tests OK / FAIL / SKIPPED.
2. **Ne pas** ajouter ce script au harness benchmark — il reste dans `tests/`
   comme suite de validation indépendante, à lancer manuellement après chaque
   modification du module `query/`.

### Validation

```bash
python3 tests/test_queries_z3.py
# Attendu : tous les tests OK.
# En cas de FAIL : reproduire localement, archiver dans AUDIT_DIFF.md la
# divergence, fixer la cause root avant de marquer Q10 comme done.
```

### Dépendances

Q5, Q6, Q7, Q8, Q9.

---

## Q11 — Mise à jour de la documentation (`README.md`, `.claude/CLAUDE.md`)

- **Sévérité** : important (sinon prochaine session Claude Code redécouvre tout)
- **Effort** : S
- **Fichiers concernés** :
  - `bachelor-thesis/README.md` (lignes 119–137, section requêtes)
  - `bachelor-thesis/.claude/CLAUDE.md` (sections Architecture, Rôle de chaque fichier, État actuel, Pièges connus)

### Diagnostic

Le `README.md` (lignes 119–137) liste explicitement 3 requêtes (`consistency`,
`validity`, `find_model`) et précise que `entails`, `is_implicant`, `enumerate`
sont "TODO / Reste à faire". Le `.claude/CLAUDE.md` du public liste aussi
seulement 4 requêtes.

### Spec du fix

1. **README.md** (section "Requêtes sur le DAG compilé", lignes 119+) :
   - Ajouter dans le tableau les 3 nouvelles requêtes :
     - `entails` | **CE** | F entraîne-t-elle une clause donnée ?
     - `is_implicant` | **IM** | un terme γ est-il implicant de F ?
     - `enumerate` | **ME (multi)** | énumère les modèles via callback
   - Ajouter des exemples CLI pour chacune.
   - Mentionner le flag `--json-with-queries` pour la sortie machine-readable
     enrichie.

2. **`.claude/CLAUDE.md`** :
   - Section "Architecture du code" : étendre l'arbre `src/utils/query/` avec
     les 3 nouveaux fichiers + ajouter `utils/dnnf_transform.h/c`.
   - Tableau "Rôle précis de chaque fichier" : ajouter 4 lignes
     (`dnnf_transform.c`, `entails.c`, `is_implicant.c`, `enumerate.c`).
     S'inspirer du tableau du privé (`bachelor-thesis-private/.claude/CLAUDE.md`)
     qui contient déjà ces lignes.
   - Section "État actuel du projet" : remplacer la liste `consistency`,
     `validity`, `find_model` par la liste complète des 6 requêtes
     Darwiche-Marquis Table 5. Marquer la TODO "Étendre les requêtes…" comme
     **faite**.
   - Section "Pièges connus" : ajouter un piège #7 :
     > **`dnnf_smooth` invalide `pool->scope_by_id`**. La table de portées est
     > recalculée à chaque appel ; toute fonction qui s'appuyait sur
     > `scope_by_id` après un smooth doit la recalculer via
     > `dnnf_compute_scopes`. Voir `dnnf_transform.c:567`.
   - Section "Pièges connus" : ajouter un piège #8 :
     > **`dnnf_condition` peut casser la lissité**. Le DAG retourné peut avoir
     > des OR avec enfants à portées différentes. Toute requête qui dépend du
     > lissage (count exact, validity, IM) doit appeler `dnnf_smooth` après. CE
     > n'a pas besoin de smooth (car teste seulement `count == 0`). Voir
     > `is_implicant.c:54-55` (smoothe) vs `entails.c:27` (smoothe pas).

3. **Mémoire utilisateur** (`memory/project_kc_phases.md`) : ne PAS toucher
   dans cette spec — la mise à jour de mémoire doit être faite par l'instance
   Claude Code orchestrant l'ensemble, après validation Q10.

### Validation

```bash
grep -c "entails\|is_implicant\|enumerate" README.md
# Attendu : >= 6 (chaque requete mentionnee au moins 2 fois : tableau + exemple)

grep -c "dnnf_transform\|entails\|is_implicant\|enumerate" .claude/CLAUDE.md
# Attendu : >= 8

# Lecture humaine : la section "Etat actuel" du .claude/CLAUDE.md doit
# refleter les 6 requetes completes.
```

### Dépendances

Q10 (validation Z3 passée avant de prétendre que la phase 2 est terminée).

---

## DAG des dépendances

```
Q1 (audit)
 └─→ Q2 (DNNFPool fields)
      └─→ Q3 (expose helpers)
           └─→ Q4 (dnnf_transform port)
                ├─→ Q5 (entails)
                ├─→ Q6 (is_implicant)
                └─→ Q7 (enumerate)
                     └─→ Q8 (main.c CLI + JSON enrichi)
                          └─→ Q9 (Makefile)
                               └─→ Q10 (cross-Z3 validation)
                                    └─→ Q11 (doc update)
```

Ordre d'exécution recommandé :
**Q1 → Q2 → Q3 → Q4 → (Q5 ∥ Q6 ∥ Q7) → Q8 → Q9 → Q10 → Q11**.

Q5/Q6/Q7 peuvent être faites en parallèle après Q4 (zéro couplage entre elles
côté code C). Tout le reste est strictement séquentiel.
