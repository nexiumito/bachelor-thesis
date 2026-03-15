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
* Étudier les différentes approches (modernes) de ces problèmes 
     >par ex. [*Counting, Knowledge Compilation and Application, Stefan Mengel, 2021*](doc/Counting_Knowledge_Compilation_and_Application_Stefan_Mengel.pdf)

---

## Références Clés

Le travail se base notamment sur les recherches suivantes :

> [**Solving #SAT and MAXSAT by dynamic programming**](doc/Solving_SharpSAT_and_MaxSAT_Dynamic_Programming.pdf) <br>
> *S.H. Saether, J.A. Telle, M. Vatshelle*

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
   git clone [https://github.com/nexiumito/bachelor-thesis.git](https://github.com/nexiumito/bachelor-thesis.git)
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

Le solveur prend deux arguments obligatoires : le chemin vers le fichier .cnf à résoudre, et le mode de décomposition en arbre souhaité.

```bash
./sat_solver <chemin_vers_fichier.cnf> <mode>
```

Modes disponibles :
- **manual** : Utilise l'arbre de décomposition manuel (issu de la Figure 2 du papier).
- **random** : Génère un arbre de décomposition de manière purement aléatoire.
- **linear** : Génère une décomposition linéaire (Linear Branch Decomposition), optimisée en se basant sur la topologie des variables.

**Exemple d'exécution** :

```bash
./sat_solver script/instances_test/type3_k3_v40_c100_b20.cnf linear
```