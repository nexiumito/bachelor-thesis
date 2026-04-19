# Données — Formules de test pour le solveur

Ce répertoire regroupe toutes les formules CNF utilisées par le solveur ainsi que les scripts qui permettent de les générer.


## Origine des fichiers

### `exemple1.cnf`, `type1/`, `type2/`, `type3/`, `random/`, `script/generator.py`

Les familles type1, type2 et type3 reproduisent les classes d'instances décrites en Section 6.1 du papier de Saether, Telle et Vatshelle (JAIR 2015). 
Le script `generator.py` écrit chaque formule dans le sous-dossier correspondant à son type.

### `tseytin/`, `factorization/`, `script/tseytin.py`, `script/binaryConverter.py`

**Ces fichiers ne sont pas mon travail.** Ils proviennent du projet de bachelor de **Loïc Bardi** (UNIGE), qui a implémenté un encodage Tseytin de circuits de multiplication binaire pour générer des formules CNF de factorisation. 
Je les réutilise tels quels, sans modification de la classe `TseytinTransformation`, uniquement parce que ces formules constituent un cas d'étude intéressant pour évaluer mon solveur sur des instances structurées issues de l'arithmétique.

Plus précisément :

- `script/tseytin.py` : classe `TseytinTransformation` écrite par Loïc Bardi
  (`code/transformation/` dans son projet original), copiée sans modification.
  **Seul ajout personnel :** un bloc `if __name__ == "__main__"` en fin de
  fichier pour exposer un CLI (`python3 tseytin.py …`), voir la section
  *Utilisation* ci-dessous.
- `script/binaryConverter.py` : helper de Loïc Bardi, copié sans modification.
- `tseytin/{3x3,4x4,5x5,7x7,10x10}.dimacs` : instances produites par le
  générateur de Loïc et copiées telles quelles depuis `data/dimacs/old/` de
  son projet. **Attention :** ces fichiers ne sont pas des circuits Tseytin
  purs : chacun contient `2k` clauses unitaires supplémentaires en fin de
  fichier qui fixent les bits de sortie `z` à une valeur spécifique (choisie
  par Loïc pour ses propres expériences de factorisation). Ils ne peuvent
  donc pas être reproduits bit-à-bit sans connaître ces valeurs de `z`.
- `factorization/5x5_464d.dimacs` et `factorization/5x5_464d_simplified.dimacs` :
  instances de factorisation 5×5 avec valeur de produit `z` fixée. Copiées
  depuis `data/dimacs/` de son projet. Le suffixe `464d` est un hash lié à
  la valeur spécifique de `z`, également inconnue de mon côté.

Le projet original de Loïc Bardi contient bien plus de fichiers (notebooks,
scripts d'expérimentation, environnement Python, formules de toutes les tailles
de 3×3 à 15×15, etc.). Je n'ai retenu que le strict minimum dont mon solveur a
besoin, afin de ne pas inclure ni redistribuer son projet entier.

Tout crédit pour la conception et l'implémentation du générateur Tseytin et des formules associées revient à Loïc Bardi.

## Utilisation

### Régénérer les formules type1/2/3/random

```bash
# Depuis data/script/
python3 generator.py
```

Chaque formule sera écrite dans `data/<type>/`.

### Régénérer / générer des formules Tseytin

Deux modes sont disponibles via le CLI ajouté en fin de `tseytin.py` :

```bash
# Depuis data/script/

# Mode 1 — circuits de multiplication purs (SANS contrainte sur z).
# Écrit data/tseytin/pure_3x3.dimacs ... pure_10x10.dimacs.
# Ces fichiers sont des circuits Tseytin "vierges" : #SAT = 2^(2k) car toute
# paire (x,y) est une multiplication valide. Ils sont distincts des fichiers
# `tseytin/{k}x{k}.dimacs` livrés par Loïc, qui eux fixent z avec une valeur inconnue.
python3 tseytin.py

# Mode 2 — instance de factorisation k×k pour une valeur précise de z.
# Écrit data/factorization/<k>x<k>_z<z>.dimacs.
python3 tseytin.py factor 5 323        # multiplieur 5×5 avec z = 323
python3 tseytin.py factor 3 15         # multiplieur 3×3 avec z = 15 (= 3*5)
```
