### DAG d-DNNF
**DAG** = *Directed Acyclic Graph*, un graphe orienté sans cycles. Quand on dit qu'on a un « DAG d-DNNF », ça veut dire qu'on a un graphe dont les noeuds représentent une formule logique, et qui a deux propriétés spécifiques (décomposabilité et déterminisme — voir ci-dessous).

Dans notre DAG :
- les **feuilles** sont soit des littéraux (`x_i`, `¬x_i`), soit les constantes `TRUE` ou `FALSE`,
- les **noeuds internes** sont des `AND` (conjonction) ou des `OR` (disjonction).

Le DAG représente exactement les modèles de la formule F : si tu parcours le DAG en remplaçant `OR → +` et `AND → ×`, tu obtiens `#SAT(F)`. C'est le résultat principal de Bova et al. 2016.

### Décomposabilité
Aux noeuds `AND`, **les enfants ne partagent aucune variable**. Conséquence : les sous-DAG sont logiquement indépendants. Chaque sous-DAG « parle » de ses propres variables, sans interférer avec ses voisins.

C'est cette propriété qui permet, lors du model counting, de **multiplier** les comptes des enfants d'un AND : `count(AND(c1, c2)) = count(c1) × count(c2)`. Pas de risque de conflit puisque les variables sont disjointes.

### Déterminisme
Aux noeuds `OR`, **les enfants sont mutuellement incompatibles** : aucun modèle de F ne peut satisfaire deux enfants d'un même OR en même temps.

Conséquence : on peut **additionner** les comptes des enfants d'un OR sans risque de double-comptage. `count(OR(c1, c2)) = count(c1) + count(c2)`.

C'est ce qui distingue d-DNNF de DNNF (déterministe vs juste décomposable).

### Lissité (smoothness)
À chaque noeud `OR`, **tous les enfants mentionnent les mêmes variables** (la même « portée »). Si ce n'est pas le cas, certains enfants ont des variables « libres » que d'autres n'ont pas, et il faut multiplier par `2^(nombre de variables manquantes)` pour bien compter.

Le DAG construit en Phase 1 par Bova et al. **est lisse par construction**. C'est important pour nous : ça veut dire qu'on n'a pas besoin de lisser avant de compter, donc CT et VA fonctionnent directement.

---

## Requêtes


### CT (Model Counting, #SAT)

**Question** : « Combien d'affectations satisfont F ? »

**Algorithme** : un parcours **bottom-up** du DAG (des feuilles vers la racine). À chaque noeud, on calcule le nombre de modèles selon des règles simples :
- `TRUE` → 1
- `FALSE` → 0
- littéral (`x` ou `¬x`) → 1
- `AND` (conjonction décomposable) → produit des comptes des enfants
- `OR` (disjonction déterministe) → somme des comptes des enfants

On mémoïse les résultats par noeud (en utilisant l'`id` du noeud comme clé), ce qui donne une complexité **O(|D|)** où |D| est le nombre d'arêtes du DAG.

**Code** : fonction `dnnf_count(root, pool)` + `dnnf_count_table(root, pool)` qui retourne la table mémoïsée complète utile à `find_model` qui en a besoin pour décider quelle branche OR descendre.

**Pourquoi d-DNNF, pas DNNF** : sans déterminisme, deux branches d'un OR peuvent partager des modèles -> l'addition les compte deux fois. Le déterminisme rend les branches mutuellement exclusives.

### CO (Consistency)

**Question** : « F est-elle satisfaisable ? Existe-t-il au moins une affectation qui rend F vraie ? »

**Algorithme** : équivalent à demander « est-ce que `#SAT(F) > 0` ? ». Implémentation triviale : on calcule le model count, et on retourne `1` si > 0, sinon `0`.

**Code** : fonction `dnnf_consistency(root, pool)` qui retourne un entier 0/1.

**polytime sur d-DNNF** : la décomposabilité seule suffit (DNNF satisfait déjà CO). Pas besoin du déterminisme pour cette requête


### VA (Validity)

**Question** : « F est-elle une **tautologie** ? Toute affectation possible des variables rend-elle F vraie ? »

Une tautologie est une formule toujours vraie, par exemple `x ∨ ¬x`. En général c'est rare en sortie d'un parser CNF — la plupart des formules réelles ne sont pas tautologies.

**Algorithme** :
1. Calculer `c = dnnf_count(F)` (le nombre de modèles).
2. Calculer `2^n` où n est le nombre total de variables.
3. Si `c == 2^n`, F est une tautologie (toutes les `2^n` affectations possibles satisfont F).

**Code** : fonction `dnnf_validity` retourne un code parmi 4 :
- `DNNF_VALIDITY_VALID` (1) — F est une tautologie
- `DNNF_VALIDITY_NOT_VALID` (0) — F n'est pas une tautologie
- `DNNF_VALIDITY_UNSAT` (-1) — F est UNSAT (cas dégénéré : non valide aussi)
- `DNNF_VALIDITY_OVERFLOW` (-2) — `n >= 63`, donc `2^n` ne tient pas en `long long`

**Pourquoi d-DNNF, pas DNNF** : VA exige le déterminisme. Sans déterminisme, l'addition aux OR pourrait gonfler artificiellement le compte jusqu'à `2^n` et donner un faux positif.


### ME variante 1 modèle (find_model)

**Question** : « Donne-moi un modèle de F. »

**Algorithme** : descente top-down dans le DAG, en utilisant les comptes pour guider le choix.

1. D'abord on calcule la **table** des comptes de tous les noeuds via `dnnf_count_table` (parcours bottom-up unique).
2. Ensuite on descend depuis la racine, et à chaque noeud :
   - Si c'est une **feuille `TRUE`** : rien à faire (modèle complet pour cette branche).
   - Si c'est une **feuille `FALSE`** : ne devrait jamais arriver (un OR parent aurait dû la filtrer). Si on l'atteint, c'est un bug.
   - Si c'est un **littéral** `x` (positif) : on fixe `model[x] = 1`. Symétrique pour `¬x`.
   - Si c'est un **AND** : on descend dans **tous les enfants** (chacun fixe ses propres variables, qui sont disjointes grâce à la décomposabilité).
   - Si c'est un **OR** : on choisit **n'importe quel enfant** dont `count > 0` et on descend uniquement dedans (les autres branches sont écartées).

Coût total : **O(|D|)**.

**Code** : fonction `dnnf_find_model(root, pool, num_vars, model_out)` reçoit un tampon `model_out` et le remplit. Retour : 1 (modèle trouvé), 0 (UNSAT), -1 (erreur d'argument).

Lemme A.3 de Darwiche-Marquis 2002 (page 244) : *« Si un sous-langage de NNF satisfait CO et CD, alors il satisfait aussi ME »*. d-DNNF satisfait CO et CD, donc ME est polytime. find_model est ME arrêté au premier modèle.