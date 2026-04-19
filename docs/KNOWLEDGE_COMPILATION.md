# Knowledge Compilation — Guide pour l'extension du solveur DP

> Document de référence pour comprendre la compilation de connaissances et son intégration
> dans le solveur #SAT/MaxSAT par programmation dynamique sur branch decomposition.
>
> Références :
> - Darwiche & Marquis, *A Knowledge Compilation Map*, JAIR 2002
> - Bova, Capelli, Mengel & Slivovsky, *On Compiling CNFs into Structured Deterministic DNNFs*, 2016
> - Capelli, *Compilation of CNF-formulas: new algorithms and lower bounds*, slides GT ALGA, 2016

---

## 1. Qu'est-ce que la Knowledge Compilation ?

Imagine la situation suivante. Tu as une formule CNF $F$ qui représente les contraintes d'un système (un circuit, un planning, un réseau bayésien). Tu dois répondre à une série de questions dessus :

- Combien de modèles a $F$ ? (#SAT)
- Quel est le maximum de clauses satisfaisables ? (MaxSAT)
- Si je fixe $x_3 = 1$, combien de modèles reste-t-il ?
- Énumère-moi tous les modèles de $F$.
- Est-ce que $F$ implique la clause $(x_1 \lor x_5)$ ?

Sans compilation, **chaque requête repart de zéro**. À chaque fois, tu relances un algorithme NP-dur ou #P-dur. Si tu as 100 requêtes, tu payes 100 fois le prix.

La **knowledge compilation** (compilation de connaissances) propose un paradigme en deux phases :

1. **Phase hors-ligne (compilation)** : transformer $F$ en une représentation cible $D$, une structure de données qui encode exactement les mêmes modèles que $F$. Cette phase est coûteuse — potentiellement exponentielle — mais on ne la fait qu'une seule fois.

2. **Phase en ligne (requêtes)** : une fois $D$ construit, répondre aux requêtes en temps **polynomial en la taille de $D$**. Compter les modèles ? $O(|D|)$. Trouver un modèle ? $O(|D|)$. Énumérer tous les modèles ? $O(|D| \times |var(D)|)$ par modèle.

C'est comme la différence entre interpréter et compiler un programme : on investit une fois dans la compilation pour ensuite exécuter rapidement autant de fois qu'on veut.

Comme le résume Capelli dans ses slides : sans compilation, chaque question est un problème difficile qu'il faut résoudre patiemment. Avec compilation, on paye une seule fois le coût de traduction, et ensuite les réponses sont quasi-instantanées.

---

## 2. Le langage NNF et ses sous-classes

### 2.1. NNF : le langage le plus général

Une **NNF** (Negation Normal Form) est un DAG (graphe acyclique dirigé) enraciné où :
- Chaque **feuille** est étiquetée par un littéral ($x$, $\neg x$) ou une constante ($\top$, $\bot$).
- Chaque **nœud interne** est étiqueté par $\wedge$ (ET) ou $\vee$ (OU) et peut avoir un nombre arbitraire d'enfants.

La **taille** d'une NNF est le nombre d'arêtes de son DAG. Toute formule propositionnelle peut être représentée en NNF — c'est un langage complet. Mais il est trop général pour permettre des requêtes efficaces. On va donc le restreindre.

### 2.2. Les propriétés qui restreignent NNF

Quatre propriétés clés permettent de définir des sous-langages de plus en plus puissants :

**Décomposabilité → DNNF.** Un $\wedge$-gate est *décomposable* si les sous-circuits de ses enfants portent sur des **ensembles de variables disjoints**. Formellement, si un nœud $\wedge$ a deux enfants dont les sous-DAGs $D_1$ et $D_2$ portent respectivement sur les variables $V_1$ et $V_2$, alors $V_1 \cap V_2 = \emptyset$. Une NNF où **tous** les $\wedge$-gates sont décomposables est une **DNNF** (Decomposable NNF).

Intuition : la décomposabilité garantit que les enfants d'un $\wedge$ traitent des parties indépendantes du problème. C'est exactement le même principe que notre branch decomposition, où chaque nœud interne partage $F$ en deux sous-formules à variables disjointes.

**Déterminisme → d-DNNF.** Un $\vee$-gate est *déterministe* si les sous-circuits de ses enfants n'ont **aucun modèle en commun** : $sat(D_1) \cap sat(D_2) = \emptyset$. Une DNNF où tous les $\vee$-gates sont déterministes est une **d-DNNF**.

Intuition : le déterminisme élimine le problème du double comptage. Quand on veut compter les modèles, on peut simplement **additionner** les compteurs des enfants d'un $\vee$ sans risque de compter un modèle deux fois. C'est ce qui rend le comptage linéaire.

**Structure → structured DNNF (s-DNNF).** Une DNNF est *structurée* s'il existe un arbre binaire (appelé **vtree**) dont les feuilles correspondent aux variables, tel que chaque $\wedge$-gate respecte une partition induite par un nœud du vtree. Le vtree formalise *comment* les variables sont découpées en groupes disjoints.

Intuition : notre branch decomposition est exactement un vtree ! Elle définit, pour chaque nœud, quelles variables sont « à gauche » et « à droite ».

**Lissage (Smoothness).** Chaque enfant d'un $\vee$-gate mentionne exactement le **même ensemble de variables**. Cette propriété simplifie certaines opérations mais n'est pas toujours nécessaire.

### 2.3. Les autres langages de la carte

Darwiche & Marquis (2002) organisent plus d'une douzaine de langages dans une hiérarchie. Les principaux, du plus restreint au plus général :

- **OBDD** (Ordered Binary Decision Diagram) : arbre de décision sur un ordre fixe des variables, avec partage de sous-graphes. Très restreint mais supporte presque toutes les opérations.
- **SDD** (Sentential Decision Diagram) : généralisation des OBDD avec un vtree. Plus succinct que les OBDD.
- **FBDD** (Free Binary Decision Diagram) : comme OBDD mais sans ordre fixe.
- **d-DNNF** : notre cible. Plus succinct que SDD et OBDD.
- **DNNF** : encore plus succinct, mais ne supporte pas le comptage en temps polynomial.
- **NNF** : le plus général, supporte très peu de requêtes.

La hiérarchie de succinctitude est : $\text{OBDD} \leq \text{SDD} \leq \text{d-DNNF} \leq \text{DNNF} \leq \text{NNF}$. Chaque inclusion est stricte : il existe des formules représentables en taille polynomiale dans le langage de droite mais qui nécessitent une taille exponentielle dans celui de gauche.

### 2.4. Tableau récapitulatif des requêtes et transformations

| Langage | CO | VA | CE | IM | CT | ME | SE | $\wedge$C | $\vee$C | $\neg$C | FO |
|---------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:---------:|:-------:|:-------:|:--:|
| NNF     | ✓  | ✓  | ✗  | ✓  | ✗  | ✗  | ✗  | ✓         | ✓       | ✓       | ✗  |
| DNNF    | ✓  | ✓  | ✓  | ✓  | ✗  | ✓  | ✗  | ✗         | ✗       | ✗       | ✓  |
| d-DNNF  | ✓  | ✓  | ✓  | ✓  | **✓** | ✓  | ✗  | ✗       | ✗       | ✗       | ✓  |
| s-d-DNNF| ✓  | ✓  | ✓  | ✓  | **✓** | ✓  | ✗  | **✓**   | ✗       | ✗       | ✓  |
| SDD     | ✓  | ✓  | ✓  | ✓  | ✓  | ✓  | ✓  | ✓         | ✓       | ✓       | ✓  |
| OBDD    | ✓  | ✓  | ✓  | ✓  | ✓  | ✓  | ✓  | ✓         | ✓       | ✓       | ✓  |

Légende : CO = consistency (est-ce satisfaisable ?), VA = validity, CE = clausal entailment, IM = implicant, CT = **counting** (#SAT), ME = model enumeration, SE = sentential entailment, $\wedge$C/$\vee$C/$\neg$C = conjonction/disjonction/négation, FO = forgetting (quantification existentielle).

**Le compromis fondamental** : les OBDD supportent tout en polytime, mais une formule peut nécessiter un OBDD de taille exponentielle là où sa d-DNNF est polynomiale. La d-DNNF offre le meilleur compromis entre succinctitude et puissance de requêtes pour notre usage.

---

## 3. Pourquoi les d-DNNF sont le bon choix pour notre projet

La d-DNNF est le langage cible naturel pour plusieurs raisons :

1. **Comptage en temps linéaire.** Grâce au déterminisme, compter les modèles se réduit à un parcours bottom-up du DAG : remplacer chaque $\vee$ par $+$ et chaque $\wedge$ par $\times$. Le résultat à la racine est $\#SAT(F)$. Complexité : $O(|D|)$.

2. **Recherche d'un modèle en $O(|D|)$.** Parcours top-down : à chaque $\vee$, choisir un enfant dont la valeur est non nulle ; à chaque $\wedge$, descendre dans tous les enfants.

3. **Énumération avec délai polynomial.** Chaque nouveau modèle est produit en temps $O(|D| \times |var(D)|)$ après le précédent, sans recalcul global.

4. **Projection et conditioning.** Fixer une variable $x = 1$ revient à remplacer $x$ par $\top$ et $\neg x$ par $\bot$ dans le DAG, puis à simplifier. Le DAG résultant est encore une d-DNNF, et on peut immédiatement recompter les modèles.

5. **Quantification existentielle (forgetting).** Éliminer une variable $x$ — calculer $\exists x. F$ — se fait en fusionnant les branches $x$ et $\neg x$. Le résultat encode tous les modèles de $F$ projetés sur les variables restantes.

6. **La structured d-DNNF supporte la conjonction.** Si on veut ajouter une contrainte supplémentaire (une nouvelle clause) à la formule compilée, la structure permet une opération de conjonction efficace — sans recompiler depuis zéro.

7. **Succinctitude maximale.** La d-DNNF est strictement plus succincte que les SDD et OBDD : il existe des formules dont la d-DNNF est de taille polynomiale alors que tout OBDD ou SDD équivalent est de taille exponentielle.

---

## 4. Le lien direct avec notre solveur DP — le résultat central

C'est la clé de voûte de l'extension. Bova, Capelli, Mengel et Slivovsky (2016) démontrent le théorème suivant :

> **Théorème.** Une formule CNF à $n$ variables, $m$ clauses et ps-width $k$ peut être compilée en une structured d-DNNF de taille $O(k^3 \cdot (n + m))$.

Et la méthode de construction est exactement celle de notre solveur : les **traces de l'algorithme DP de Sæther-Telle-Vatshelle** peuvent être utilisées pour construire cette d-DNNF.

Autrement dit : **notre solveur DP effectue déjà implicitement une compilation en d-DNNF**. Il calcule toutes les informations nécessaires (PS-sets, tables DP), mais il les jette après avoir extrait deux nombres (#SAT et MaxSAT). L'idée est de rendre cette construction **explicite** : au lieu de jeter les tables, on construit le DAG du circuit.

### Comment ça fonctionne concrètement

Bova et al. introduisent le concept de **shape** (« forme ») pour formaliser la correspondance avec le DP. Une shape pour un nœud $v$ est une paire $\mathbf{S} = (S, S')$ où :
- $S \in PS'(F_v)$ encode les clauses extérieures satisfaites depuis l'intérieur ;
- $S' \in PS'(F_{\bar{v}})$ encode les clauses intérieures satisfaites depuis l'extérieur.

Pour chaque shape $\mathbf{S}$, la construction associe **un sous-circuit DNNF** $\varphi_v(\mathbf{S})$ qui représente exactement les affectations satisfaisantes ayant cette shape. Voici la correspondance précise (Lemmes 4-7 de Bova et al.) :

**La branch decomposition = le vtree.** Notre arbre de décomposition $(T, \delta)$ sert directement de vtree pour la d-DNNF structurée. Chaque nœud $v$ de $T$ correspond à une partition des variables en « sous-arbre de $v$ » et « extérieur de $v$ » — exactement ce que fait un vtree.

**Une entrée de table DP = un OR-gate.** Chaque shape $\mathbf{S} = (S, S')$ pour le nœud $v$ correspond à **un OR-gate** $\varphi_v(\mathbf{S})$ dans la d-DNNF. Dans notre implémentation, c'est exactement l'entrée $Tab_v[i][j]$ où $i$ indexe $S \in PS'(F_v)$ et $j$ indexe $S' \in PS'(F_{\bar{v}})$. Le déterminisme est garanti car chaque enfant de ce OR correspond à une combinaison de shapes-enfants disjointe.

**Une mise à jour DP = un AND-gate.** Chaque triplet valide $(\mathbf{S}_1, \mathbf{S}_2)$ qui « génère » la shape $\mathbf{S}$ (au sens du Lemme 1 du paper) correspond à **un AND-gate** enfant du OR ci-dessus. Ce AND-gate combine $\varphi_{v_1}(\mathbf{S}_1)$ et $\varphi_{v_2}(\mathbf{S}_2)$. La décomposabilité est garantie car $X_{v_1}$ et $X_{v_2}$ sont disjoints par construction.

Formellement : $\varphi_v(\mathbf{S}) \equiv \bigvee_{(\mathbf{S}_1,\mathbf{S}_2) \text{ génère } \mathbf{S}} \varphi_{v_1}(\mathbf{S}_1) \wedge \varphi_{v_2}(\mathbf{S}_2)$.

**Les feuilles variables = des littéraux nus.** Pour une feuille variable $x$, il y a exactement **deux shapes** $\mathbf{S}^x$ et $\mathbf{S}^{\neg x}$, correspondant aux deux affectations possibles. À chaque shape on associe un sous-circuit trivial : $\varphi_v(\mathbf{S}^x) \equiv x$ et $\varphi_v(\mathbf{S}^{\neg x}) \equiv \neg x$. **Pas de OR-gate à la feuille** — l'OR émerge naturellement au nœud parent qui combine ces deux sous-circuits.

**Les feuilles clauses = des constantes.** Pour une feuille clause $c$, il y a deux shapes $\mathbf{S}^\bot = (\emptyset, \emptyset)$ et $\mathbf{S}^\top = (\emptyset, \{c\})$, qui mappent respectivement vers les **constantes** $\bot$ et $\top$. La constante $\top$ signifie « $c$ est satisfaite par l'extérieur », $\bot$ signifie « non ».

### Corollaires

Le théorème principal se décline en bornes pour d'autres paramètres structurels :

- **Treewidth incidente $k$** : la ps-width est au plus $2^{k+1}$, donc la d-DNNF a taille $O(8^k \cdot (n+m))$.
- **Clique-width incidente $k$** : la ps-width est au plus $m^k$, donc la d-DNNF a taille $O(m^{3k} \cdot (n+m))$.

Capelli résume cette connexion dans ses slides : *« Every known structure-based algorithm for #SAT may be seen as an implicit compilation of the formula into deterministic DNNF. »* — tout algorithme structurel pour #SAT est, en fait, un compilateur d-DNNF déguisé.

---

## 5. Ce que ça change concrètement pour notre implémentation

### 5.1. Phase de compilation : modifier la Procédure 3

Le changement principal se situe dans la Procédure 3 (`solve_dp`). Actuellement, elle calcule `maxsat[i*cols+j]` et `sharpsat[i*cols+j]` — des nombres. Il faut la modifier pour construire en parallèle un **DAG**, en suivant strictement la construction de Bova et al. (Section 3.3) :

**Feuille variable $x$.** Pour chaque shape $\mathbf{S} \in \{\mathbf{S}^x, \mathbf{S}^{\neg x}\}$ (au plus deux), associer un sous-circuit trivial : un nœud littéral $x$ ou $\neg x$. **Pas de OR-gate ici.** Stocker dans `Tab_v[i][j].dnnf_node` un pointeur vers le littéral.

**Feuille clause $c$.** Pour chaque shape, associer une constante : $\top$ si $c \in S'$, $\bot$ sinon. Stocker la constante dans `Tab_v[i][j].dnnf_node`.

**Nœud interne $v$.** Pour chaque shape $\mathbf{S} = (S, S')$ pour $v$ (i.e., chaque entrée $Tab_v[i][j]$) :
1. Créer **un OR-gate** $\varphi_v(\mathbf{S})$, stocké dans `Tab_v[i][j].dnnf_node`.
2. Pour chaque triplet $(\mathbf{S}_1, \mathbf{S}_2)$ qui génère $\mathbf{S}$ (au sens du Lemme 1 du paper, i.e. les triplets $(C_{c_1}, C_{c_2}, C')$ valides explorés actuellement par la Procédure 3) :
   - Créer **un AND-gate** dont les enfants sont $\varphi_{v_1}(\mathbf{S}_1)$ et $\varphi_{v_2}(\mathbf{S}_2)$ (récupérés via `Tab_{c1}[..]` et `Tab_{c2}[..]`).
   - Ajouter ce AND-gate comme enfant du OR-gate ci-dessus.
3. Si le OR-gate n'a aucun enfant (shape invalide), libérer et stocker `NULL`.

**Le résultat final** est $\varphi_r(\emptyset) = $ `Tab_r[0][0].dnnf_node`, qui par le Lemme 6 du paper est une structured d-DNNF représentant exactement les modèles de $F$.

La structure de données du DAG est un simple tableau de nœuds, chaque nœud ayant un type (`AND`, `OR`, `LIT_POS`, `LIT_NEG`, `TRUE`, `FALSE`), un index de variable pour les littéraux, et un tableau de pointeurs vers ses enfants.

### 5.2. Phase de requêtes : nouvelles fonctionnalités

Une fois le DAG $D$ construit, on peut implémenter les requêtes suivantes, toutes en temps polynomial :

**Counting (#SAT)** : parcours bottom-up du DAG. Pour chaque nœud :
- Feuille $x$ ou $\neg x$ → valeur 1
- Feuille $\top$ → 1, feuille $\bot$ → 0
- $\vee$-gate → somme des valeurs des enfants
- $\wedge$-gate → produit des valeurs des enfants

Le résultat à la racine est #SAT. Complexité : $O(|D|)$.

**Trouver un modèle** : parcours top-down. À chaque $\vee$, choisir un enfant dont le compteur est $> 0$. À chaque $\wedge$, descendre dans tous les enfants. Les littéraux rencontrés dans les feuilles forment le modèle.

**Énumération** : variante du parcours précédent avec backtracking, explorant systématiquement toutes les branches des $\vee$-gates.

**Conditioning** (projection sur $x = 1$) : remplacer toute feuille $x$ par $\top$ et $\neg x$ par $\bot$, puis simplifier le DAG (propager les constantes). Recompter donne #SAT($F \mid x=1$).

**Forgetting** (quantification existentielle $\exists x. F$) : fusionner les branches $x$ et $\neg x$. Le résultat est une DNNF (plus nécessairement déterministe) qui encode $\exists x. F$.

> **Note sur MaxSAT.** La construction de Bova et al. produit une d-DNNF qui représente
> les **modèles** de $F$ (les affectations satisfaisantes), pas la fonction « nombre de
> clauses satisfaites par une affectation ». Remplacer $\vee$ par $\max$ ne donne donc
> **pas** MaxSAT($F$). Pour conserver le calcul de MaxSAT, on garde la procédure DP
> existante (`procedure3.c`) en parallèle de la construction du DAG : les deux co-existent
> et utilisent les mêmes tables.

### 5.3. Avantages concrets

| Avant (solveur DP seul) | Après (avec compilation d-DNNF) |
|---|---|
| Une exécution = un résultat (#SAT + MaxSAT) | Une compilation = requêtes illimitées en $O(\|D\|)$ |
| Fixer une variable → relancer tout le pipeline | Fixer une variable → simplifier le DAG en $O(\|D\|)$ |
| Pas d'énumération possible | Énumération avec délai $O(\|D\| \cdot n)$ par modèle |
| Pas de projection | Projection et forgetting en temps linéaire |
| Tables DP jetées après usage | DAG persistant, réutilisable |

La taille du DAG est bornée par $O(k^3 \cdot (n+m))$, exactement le même paramètre que notre solveur. Si la ps-width est petite, le DAG est compact et toutes les requêtes sont rapides.

---

## 6. Exemple concret : la formule Figure 2

Prenons `exemple1.cnf` : 5 variables, 4 clauses.

$$F = (x_1 \lor x_2) \land (x_1 \lor \neg x_2 \lor x_3) \land (x_2 \lor \neg x_4 \lor x_5) \land (x_2 \lor x_3 \lor x_5)$$

Avec l'arbre manuel, notre solveur calcule : ps-width = 4, #SAT = 17, MaxSAT = 4.

**Actuellement**, la Procédure 3 remplit des tables $Tab_v$ pour chaque nœud, puis lit $Tab_r(\emptyset, \emptyset)$ et jette tout.

**Avec la compilation**, au lieu de jeter les tables, on émet des nœuds de circuit :

1. **Feuille $x_1$** : deux shapes, $\mathbf{S}^{x_1}$ et $\mathbf{S}^{\neg x_1}$. On stocke le littéral $x_1$ dans `Tab_{x_1}[0][0]` et $\neg x_1$ dans `Tab_{x_1}[1][0]`. Pas de OR-gate ici.

2. **Feuille $x_2$** : idem, deux littéraux $x_2$ et $\neg x_2$ stockés dans les entrées correspondantes.

3. **Nœud interne $A = (x_1, x_2)$** : pour chaque shape $\mathbf{S}$ pour $A$, on crée un OR-gate dont les enfants sont des AND-gates ; chaque AND combine un littéral pris dans `Tab_{x_1}` avec un littéral pris dans `Tab_{x_2}`, pour chaque triplet $(\mathbf{S}_1, \mathbf{S}_2)$ qui génère $\mathbf{S}$.

4. On continue récursivement pour tous les nœuds de l'arbre...

5. **À la racine**, l'unique entrée `Tab_r[0][0]` contient le OR-gate $\varphi_r(\emptyset)$, qui est la racine de notre d-DNNF finale.

**Requête #SAT sur le circuit** : parcours bottom-up, $\vee \to +$, $\wedge \to \times$. On retrouve 17 à la racine.

**Requête de projection** ($x_1 = 1$) : remplacer $x_1$ par $\top$, $\neg x_1$ par $\bot$, simplifier. Recompter donne #SAT($F \mid x_1 = 1$) = 13 (vérifiable par brute-force : sur les 17 modèles de $F$, 13 ont $x_1 = 1$).

---

## 7. Positionnement dans la littérature

Les compilateurs existants comme **c2d** (Darwiche), **D4** (Lagniez & Marquis) et **miniC2D** fonctionnent par **DPLL exhaustif avec caching de composantes** : ils branchent sur les variables une par une ($F[x \to 0]$ et $F[x \to 1]$), détectent les composantes disjointes, et cachent les sous-résultats. La structure du circuit résultant est dictée par l'ordre de branchement (un arbre de décision).

Notre approche est fondamentalement différente :

- On exploite la **ps-width**, un paramètre structurel plus général que la treewidth. Des classes de formules à treewidth non bornée (graphes d'intervalles, arcs circulaires) ont une ps-width bornée et sont donc compilables efficacement par notre méthode, mais pas par les compilateurs basés sur la treewidth.

- Le papier de Bova et al. (2016) fournit la **preuve théorique** que la construction via les traces du DP de Sæther-Telle-Vatshelle produit une structured d-DNNF valide. Cependant, à notre connaissance, **aucune implémentation de cette construction n'a été publiée**. Notre travail constituerait donc la première implémentation pratique de compilation CNF → structured d-DNNF via ps-width.

**Limitations.** La taille de la d-DNNF dépend de $k^3$, où $k$ est la ps-width. Si $k$ est grand (formules 3-SAT aléatoires, circuits de multiplication), le circuit compilé explose. C'est un mur théorique : Bova et al. montrent via la complexité de communication que certaines familles de 3-CNF monotones ont une complexité de multipartition $\Omega(n + m)$, ce qui implique qu'aucune DNNF — quel que soit l'algorithme — ne peut les représenter en taille sous-exponentielle.

---

## 8. Résumé : ce qu'on va faire

| Ce qu'on a déjà | Ce qu'on va ajouter |
|---|---|
| Solveur DP calculant #SAT et MaxSAT | Construction explicite du circuit d-DNNF pendant le DP |
| 4 modes de décomposition (manual, random, linear, greedy) | Utilisation de la décomposition comme vtree pour la d-DNNF |
| Résultat = deux nombres (`maxsat_value`, `sharpsat_count`) | Résultat = un DAG réutilisable supportant des requêtes polytime |
| Une exécution complète par requête | Compilation unique + requêtes multiples en $O(\|D\|)$ |
| Pas d'énumération ni de projection | Énumération de modèles, conditioning, forgetting |
| Comparaison avec Z3 sur MaxSAT uniquement | Comparaison avec c2d/D4 sur la taille des circuits compilés |

L'objectif est double :

1. **Implémenter** la construction du DAG d-DNNF en C, en modifiant principalement `procedure3.c` et en ajoutant un nouveau module `dnnf.{h,c}` pour la structure du circuit et les requêtes.

2. **Évaluer expérimentalement** la taille des circuits produits sur nos instances de test (type 1/2/3, random, Tseytin) et comparer avec les compilateurs existants, en montrant l'avantage sur les instances à ps-width bornée.
