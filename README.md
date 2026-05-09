<div align="center">
  <img src="assets/logo_unige.png" alt="Université de Genève - Faculté des Sciences" width="300"/>
  
  <br/>
  <br/>

  # Programmation Dynamique pour Formules SAT
  
  **Travail de Bachelor en Sciences Informatiques** @ *Université de Genève - Faculté des Sciences*

  ![Language](https://img.shields.io/badge/Language-C-blue?style=for-the-badge&logo=c) ![Year](https://img.shields.io/badge/Année-2025%20--%202026-orange?style=for-the-badge)
  ![Status](https://img.shields.io/badge/Status-En%20Cours-green?style=for-the-badge)

</div>

---

## Sujet

L'objectif principal est d'étudier et d'implémenter des approches modernes pour la résolution de formules SAT, en se concentrant spécifiquement sur la **programmation dynamique**. Le projet vise à appréhender les structures particulières de certaines formules pour optimiser la résolution de problèmes complexes comme **MAXSAT** et **#SAT** (comptage de solutions).

### Objectifs
* Comprendre et implémenter les algorithmes décrits dans la [littérature scientifique récente](doc/Solving_SharpSAT_and_MaxSAT_Dynamic_Programming.pdf).
* Étudier les différentes approches (modernes) de ces problèmes, en particulier la **compilation de connaissances** (Knowledge Compilation) :
     > [*On Compiling CNFs into Structured Deterministic DNNFs*, Bova, Capelli, Mengel, Slivovsky, 2016](doc/cnf-to-ddnnf-upper-bound.pdf) <br>
     > [*A Knowledge Compilation Map*, Darwiche, Marquis, 2002](doc/1106.1819v1-1.pdf)

---

## Références Clés

Le travail se base notamment sur les recherches suivantes :

> [**Solving #SAT and MAXSAT by dynamic programming**](doc/Solving_SharpSAT_and_MaxSAT_Dynamic_Programming.pdf) <br>
> *S.H. Saether, J.A. Telle, M. Vatshelle*

> [**On Compiling CNFs into Structured Deterministic DNNFs**](doc/cnf-to-ddnnf-upper-bound.pdf) <br>
> *S. Bova, F. Capelli, S. Mengel, F. Slivovsky*

> [**A Knowledge Compilation Map**](doc/1106.1819v1-1.pdf) <br>
> *A. Darwiche, P. Marquis*

---

## Rapport

Le rapport complet détaillant l'étude, les algorithmes et les résultats du projet est disponible ici : **[thesis](thesis/thesis.pdf)**

---

## Implémentation

Toutes les implémentations en C et les scripts liés à ce projet se trouvent dans le répertoire suivant : **[src](src/)**

---

## Auteur & Superviseurs :

| Rôle | Nom | Contact |
| :--- | :--- | :--- |
| **Étudiant** | **Elie Bussod** | [Elie.Bussod@etu.unige.ch](mailto:Elie.Bussod@etu.unige.ch) |
| **Responsables** | **Arnaud Casteigts** <br> **Pierre Leone** | [Arnaud.Casteigts@unige.ch](mailto:Arnaud.Casteigts@unige.ch) <br> [Pierre.Leone@unige.ch](mailto:Pierre.Leone@unige.ch) |

---

## Installation & Utilisation

1. **Cloner le dépôt** : 
   ```bash
   git clone https://github.com/nexiumito/bachelor-thesis.git
   cd bachelor-thesis/src
   ```

2. **Générer des instances de test (Optionnel)** :
Un script Python est fourni pour générer des formules SAT spécifiques (Totalement aléatoires ou Graphes d'intervalles bipartis).

```bash
python3 script/generator.py
```
Les fichiers générés seront placés dans le dossier script/instances_test/.

3. **Compiler le solveur** :
```bash
make
```

4. **Syntaxe** :

Le solveur prend deux arguments obligatoires : le chemin vers le fichier `.cnf` à résoudre, et le mode de décomposition en arbre souhaité.

```bash
./sat_solver <chemin_vers_fichier.cnf> <mode>
```

Modes disponibles :
- **manual** : Utilise l'arbre de décomposition manuel (issu de la Figure 2 du papier ; ne fonctionne que sur la formule à 5 variables / 4 clauses).
- **random** : Génère un arbre de décomposition de manière purement aléatoire.
- **linear** : Génère une décomposition linéaire (Linear Branch Decomposition), basée sur l'ordre des variables.
- **greedy** : GreedyOrder, l'heuristique décrite dans la Section 6 du papier — recommandée par défaut.

Mode spécial pour exécuter une batterie d'instances et afficher un tableau récapitulatif :

```bash
./sat_solver benchmark
```

**Exemples d'exécution** :

```bash
./sat_solver data/exemple1.cnf manual
./sat_solver data/type3/type3_n100_t3_s2.cnf greedy
./sat_solver benchmark
```

Sortie standard : nombre de modèles (`#SAT`), valeur MaxSAT, taille du DAG d-DNNF compilé, ps-width et temps d'exécution par phase.

5. **Requêtes sur le DAG compilé** (optionnel) :

Une fois la formule compilée en d-DNNF (*Knowledge Compilation*), on peut interroger ce DAG en temps polynomial en sa taille. Le solveur expose les **6 requêtes polytime** sur d-DNNF (cf. *A Knowledge Compilation Map*, Darwiche & Marquis 2002, Table 5) :

| Requête CLI | Sigle | Effet |
| :--- | :---: | :--- |
| `consistency` | **CO** | F est-elle satisfaisable ? |
| `validity` | **VA** | F est-elle une tautologie ? |
| `find_model` | **ME** (1 modèle) | Affiche une affectation satisfaisante (ou indique UNSAT) |
| `entails L1 L2 ...` | **CE** | F entraîne-t-elle la clause `(L1 ∨ L2 ∨ …)` ? (littéraux DIMACS) |
| `is_implicant L1 L2 ...` | **IM** | Le terme `(L1 ∧ L2 ∧ …)` est-il implicant de F ? |
| `enumerate [LIMIT]` | **ME** (multi) | Énumère tous les modèles via callback, capés à `LIMIT` (défaut 10⁶) |

Le model counting (**CT**, `#SAT`) est inclus de base dans la sortie standard du solveur, sans option à activer.

**Exemples** :

```bash
./sat_solver data/exemple1.cnf greedy consistency        # SAT ?
./sat_solver data/exemple1.cnf greedy validity           # tautologie ?
./sat_solver data/exemple1.cnf greedy find_model         # un modèle
./sat_solver data/exemple1.cnf greedy entails 1 2        # F |= (x1 ∨ x2) ?
./sat_solver data/exemple1.cnf greedy is_implicant 1     # x1 |= F ?
./sat_solver data/exemple1.cnf greedy enumerate 5        # 5 premiers modèles
```

Si aucune requête n'est passée, le solveur affiche uniquement le résumé standard (#SAT, MaxSAT, taille du DAG).

**Sortie machine-readable** : deux flags JSON sont disponibles.

```bash
./sat_solver data/exemple1.cnf greedy --json                 # JSON minimal (rétro-compatible runs 1-3)
./sat_solver data/exemple1.cnf greedy --json-with-queries    # JSON enrichi avec timings query_*_ms
```

Le mode `--json-with-queries` mesure en interne le temps de chaque requête sur le DAG **déjà compilé** (médiane sur 5 répétitions, 1ère jetée pour warm-up). Champs ajoutés : `query_co_ms`, `query_va_ms`, `query_ct_ms`, `query_me_ms`, `query_ce_ms`, `query_im_ms`, `query_enum_first_ms`, `query_enum_all_ms`, `query_enum_count`, `query_*_result`.

6. **Aide en ligne de commande** :

Toute exécution sans argument valide imprime la liste complète des modes et requêtes :

```bash
./sat_solver
```