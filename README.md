<div align="center">
  <img src="assets/logo_unige.png" alt="Université de Genève - Faculté des Sciences" width="300"/>
  
  <br/>
  <br/>

  # Programmation Dynamique pour Formules SAT
  
  **Travail de Bachelor en Sciences Informatiques** @ *Université de Genève - Faculté des Sciences*

  ![Language](https://img.shields.io/badge/Language-Python-blue?style=for-the-badge&logo=python) ![Year](https://img.shields.io/badge/Année-2025%20--%202026-orange?style=for-the-badge)
  ![Status](https://img.shields.io/badge/Status-En%20Cours-green?style=for-the-badge)

</div>

---

## Sujet

L'objectif principal est d'étudier et d'implémenter des approches modernes pour la résolution de formules SAT, en se concentrant spécifiquement sur la **programmation dynamique**. Le projet vise à appréhender les structures particulières de certaines formules pour optimiser la résolution de problèmes complexes comme **MAXSAT** et **#SAT** (comptage de solutions).

### Objectifs
* Comprendre et implémenter les algorithmes décrits dans la [littérature scientifique récente](doc/MaxSat.pdf).
* Étudier les différentes approches (modernes) de ces problèmes 
     >par ex. [*Counting, Knowledge Compilation and Application, Stefan Mengel, 2021*](doc/)

---

## Références Clés

Le travail se base notamment sur les recherches suivantes :

> [**Solving #SAT and MAXSAT by dynamic programming**](doc/MaxSAT) <br>
> *S.H. Saether, J.A. Telle, M. Vatshelle*

---

## Rapport

Le rapport complet détaillant l'étude, les algorithmes et les résultats du projet est disponible ici : **[Thesis](thesis/thesis.pdf)**

---

## Implémentation

Toutes les implémentations en Python et les scripts liés à ce projet se trouvent dans le répertoire suivant : **[Code](code/)**

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
   ```
2. **Créer un environnement Python** (sur VSCode: `^P`, `>`, `Python: Create Environment`)
3. **Installer toutes les dépendances** : 
   ```bash
   pip install -r requirements.txt
   ```

### Mise à jour

1. `pip freeze > requirements.txt`

### Utilisation