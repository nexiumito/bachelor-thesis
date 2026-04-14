# **Programmation Dynamique pour Formules SAT : État de l'Art Exhaustif**

## **1\. SAT, \#SAT et MaxSAT — Définitions et complexité**

L'étude de la satisfaisabilité booléenne constitue le socle fondamental de la théorie de la complexité algorithmique et de l'optimisation combinatoire. Les problèmes qui en dérivent, notamment SAT, \#SAT et MaxSAT, modélisent des paradigmes de décision, de dénombrement et d'optimisation dont les ramifications théoriques et pratiques s'étendent de la cryptanalyse à l'inférence probabiliste.

### **Définition formelle de SAT**

Le problème de la satisfaisabilité booléenne (SAT) consiste à déterminer s'il existe une assignation de valeurs de vérité (Vrai ou Faux) pour un ensemble de variables booléennes telle qu'une formule logique donnée soit évaluée à Vrai. Dans la littérature algorithmique et pour l'écrasante majorité des solveurs, la formule est représentée sous la forme normale conjonctive (CNF). Une formule CNF $\\Phi$ est une conjonction ($\\land$) de clauses, où chaque clause est une disjonction ($\\lor$) de littéraux. Un littéral correspond à une variable propositionnelle $x\_i$ ou à sa négation $\\neg x\_i$.  
Formellement, soit un ensemble de variables $X \= \\{x\_1, x\_2, \\dots, x\_n\\}$. Une assignation est une fonction $\\tau : X \\rightarrow \\{\\text{Vrai}, \\text{Faux}\\}$. Une clause $C \= (l\_1 \\lor l\_2 \\lor \\dots \\lor l\_k)$ est satisfaite par $\\tau$ si et seulement si au moins l'un de ses littéraux $l\_i$ est évalué à Vrai sous $\\tau$. La formule entière $\\Phi \= C\_1 \\land C\_2 \\land \\dots \\land C\_m$ est dite satisfaisable s'il existe une assignation $\\tau$ qui satisfait simultanément toutes les clauses $C\_j$. Une telle assignation est appelée une assignation satisfaisante, ou modèle.

### **NP-complétude de SAT et importance pratique**

Le problème SAT revêt une importance historique et théorique fondatrice, ayant été le tout premier problème formellement démontré comme étant NP-complet. Ce résultat canonique, établi par Stephen Cook en 1971 \[Cook, 1971\], a défini le paysage de la complexité computationnelle. Le théorème de Cook-Levin démontre que tout problème de la classe de complexité NP (les problèmes de décision dont une solution peut être vérifiée en temps polynomial par une machine de Turing déterministe) peut être réduit en temps polynomial à SAT.  
L'année suivante, Richard Karp a consolidé ce paradigme en publiant ses 21 problèmes NP-complets, démontrant des réductions polynomiales de SAT vers des problèmes combinatoires classiques tels que la coloration de graphes, le problème de la clique, ou la couverture exacte \[Karp, 1972\]. L'implication directe est que l'existence d'un algorithme déterministe s'exécutant en temps polynomial pour résoudre SAT prouverait que P \= NP.  
Malgré cette barrière théorique réputée infranchissable dans le pire des cas, l'ingénierie algorithmique moderne a permis le développement de solveurs SAT qualifiés de "déraisonnablement efficaces". En pratique, ces solveurs gèrent couramment des instances industrielles impliquant des dizaines de millions de variables et de clauses. Cette dichotomie entre la complexité pire-cas (exponentielle) et la complexité typique (souvent polynomiale ou faiblement exponentielle sur des instances structurées) a transformé des domaines entiers de l'ingénierie, allant de la conception de circuits intégrés, à la vérification formelle de logiciels, jusqu'à la démonstration automatique de théorèmes.

### **\#SAT (Model Counting) : Définition, \#P-complétude et applications**

Alors que le problème SAT standard pose une question de décision (existe-t-il au moins un modèle?), le problème \#SAT, également appelé *model counting*, pose une question de dénombrement absolu : combien d'assignations de vérité distinctes satisfont la formule?  
La complexité computationnelle de \#SAT a été formalisée par Leslie Valiant, qui a démontré en 1979 que ce problème est \#P-complet \[Valiant, 1979\]. La classe \#P englobe les problèmes de comptage associés aux problèmes de décision de la classe NP. Fait remarquable mis en évidence par l'analyse de Valiant, même pour des fragments syntaxiques où le problème de décision est facile, le problème de dénombrement demeure extrêmement ardu. Par exemple, décider si une formule 2CNF (au plus deux littéraux par clause) ou une formule de Horn est satisfaisable se fait en temps linéaire, mais compter leurs modèles respectifs (\#2SAT et \#Horn-SAT) reste strictement \#P-complet. Un oracle résolvant \#SAT permet de résoudre instantanément la satisfaisabilité (le compte est-il strictement supérieur à zéro?), validant que la classe \#P contient NP.  
Les algorithmes capables de résoudre \#SAT, même de manière contrainte, ont un impact immense car ils forment le cœur du raisonnement probabiliste. Le calcul d'inférences dans un réseau bayésien (probabilités marginales, probabilités a posteriori) ou dans des bases de données probabilistes peut se traduire de manière compacte en un problème de comptage de modèles pondérés (Weighted Model Counting, WMC). Dans ce contexte, la structure de la formule CNF capture la topologie du réseau et les dépendances conditionnelles, tandis que l'évaluation probabiliste est extraite par le dénombrement global. \#SAT sert également dans des tâches complexes de vérification, d'analyse de fiabilité des systèmes, et dans le calcul du flux d'information pour la cybersécurité.

### **MaxSAT : Définition, complexité et variantes**

La variante d'optimisation de SAT est le problème de la satisfaisabilité maximale, ou MaxSAT. Au lieu de requérir la satisfaction de l'intégralité des clauses, MaxSAT demande de trouver une assignation de variables qui maximise le nombre (ou la somme des poids) des clauses satisfaites. MaxSAT est formellement NP-difficile (et sa version décisionnelle est NP-complète).  
Afin de capturer les subtilités de modélisation du monde réel, la littérature académique et les compétitions de solveurs divisent le problème en plusieurs variantes :

1. **Partial MaxSAT (PMS) :** L'ensemble des clauses est explicitement divisé en deux sous-ensembles : les clauses *dures* (hard clauses), qui doivent impérativement être satisfaites pour que la solution soit valide, et les clauses *souples* (soft clauses), qui peuvent être violées mais dont la satisfaction globale doit être maximisée.  
2. **Weighted MaxSAT (WMS) :** Aucune distinction dure/souple n'est faite, mais chaque clause est associée à un poids (une pénalité en cas de violation). L'objectif est de trouver une assignation maximisant la somme des poids des clauses satisfaites.  
3. **Weighted Partial MaxSAT (WPMS) :** Cette variante combine les deux précédentes, offrant une modélisation particulièrement riche pour des problèmes d'ordonnancement, de planification sous contraintes de ressources, ou de diagnostic de pannes logicielles.

| Variantes de SAT | Objectif de la résolution | Complexité algorithmique fondamentale |
| :---- | :---- | :---- |
| **SAT** | Existe-t-il une assignation satisfaisante? | NP-complet \[Cook, 1971\] |
| **\#SAT** | Dénombrement exact de toutes les solutions | \#P-complet \[Valiant, 1979\] |
| **MaxSAT** | Maximisation stricte des clauses satisfaites | NP-difficile |
| **WPMS** | Optimisation pondérée avec clauses dures/souples | NP-difficile |

### **Lien entre \#SAT et compilation de connaissances**

L'impossibilité de résoudre efficacement \#SAT sur des instances massives a mené à l'émergence de la *compilation de connaissances* (Knowledge Compilation). La résolution du dénombrement se heurte souvent au problème de devoir réévaluer de larges pans de l'espace de recherche lorsque les paramètres (par exemple les poids dans un réseau bayésien) changent légèrement. La compilation de connaissances résout ce problème en transformant la formule CNF initiale, par un processus hors-ligne coûteux, vers une structure de données cible canonique ou hautement structurée (comme un d-DNNF ou un SDD). Une fois cette structure obtenue, l'opération \#SAT ou le WMC peut y être exécuté en temps strictement polynomial par rapport à la taille de la structure compilée. La difficulté est alors déplacée du temps de requête (online) vers le temps de compilation (offline).

## **2\. Algorithmes SAT modernes**

L'incroyable essor des méthodes de résolution formelle repose sur le raffinement continu d'un algorithme de recherche arborescente datant des années 1960\.

### **DPLL : Principe, propagation unitaire et backtracking**

L'algorithme de Davis-Putnam-Logemann-Loveland (DPLL) constitue l'approche historique et systématique de la résolution SAT. Il repose sur un parcours en profondeur (Depth-First Search) de l'arbre des assignations partielles, couplé à des règles d'inférence strictes pour élaguer l'espace de recherche. Ses mécaniques s'articulent ainsi :

1. **Choix de décision (Branching) :** Une variable non assignée est choisie heuristiquement et une valeur (Vrai ou Faux) lui est affectée.  
2. **Propagation unitaire (Unit Propagation / BCP) :** Dès qu'une clause ne contient plus qu'un seul littéral non assigné et que tous ses autres littéraux ont été falsifiés par l'assignation partielle courante, ce littéral restant est forcé de prendre la valeur Vrai. Ce forçage peut, en cascade, rendre d'autres clauses unitaires, déclenchant une réaction en chaîne appelée propagation des contraintes booléennes (BCP).  
3. **Règle du littéral pur :** Si un littéral n'apparaît qu'avec une seule polarité dans l'ensemble des clauses non satisfaites, la variable est assignée de manière à satisfaire toutes ces clauses simultanément.  
4. **Retour sur trace (Backtracking) :** Si l'assignation partielle conduit à une clause dont tous les littéraux sont falsifiés (un conflit), DPLL conclut que l'assignation courante est une impasse. Il annule alors chronologiquement les assignations jusqu'à la dernière décision ayant une branche inexplorée, en inverse la valeur, et reprend l'exploration.

### **CDCL : L'apprentissage de clauses dirigé par les conflits**

Au milieu des années 1990, Marques-Silva et Sakallah, ainsi que Bayardo et Schrag, ont fait évoluer DPLL vers l'architecture *Conflict-Driven Clause Learning* (CDCL), qui est aujourd'hui l'étalon-or des solveurs industriels. Plutôt que de simplement constater un conflit et rebrousser chemin de façon aveugle (chronologiquement), un solveur CDCL exploite l'échec pour "apprendre".  
Lorsqu'un conflit survient durant la propagation unitaire, le solveur construit un **graphe d'implication**. Ce graphe modélise la causalité : les nœuds sont les littéraux assignés et les arêtes pointent des littéraux falsifiants vers le littéral unitaire forcé, en étiquetant chaque arête avec la clause responsable. En analysant ce graphe depuis le nœud du conflit en remontant vers les variables de décision, le solveur identifie une coupe structurelle, généralement au niveau du premier point d'implication unique (*1UIP \- First Unique Implication Point*).  
Les littéraux qui forment cette coupe constituent la cause profonde du conflit. Le solveur génère alors la négation logique de cette conjonction, produisant une **clause apprise** (Learnt Clause). Cette nouvelle clause est ajoutée dynamiquement à la base de données de la formule d'origine. Cet apprentissage a deux vertus fondamentales : d'une part, il empêche définitivement le solveur d'explorer la même combinaison stérile à l'avenir ; d'autre part, il permet un **backjumping non chronologique**. Au lieu de remonter à la décision immédiatement précédente, le solveur saute au niveau de décision le plus profond impliqué dans la clause apprise, ignorant des pans entiers de l'arbre de recherche qui sont désormais mathématiquement prouvés comme non pertinents.  
Des solveurs phares tels que **zChaff** (Moskewicz et al., 2001), **MiniSAT** (Eén & Sörensson, 2003), **Glucose** et **CaDiCaL** reposent sur cette dynamique analytique.

### **Propagation unitaire (BCP) : Rôle, confluence et complexité**

La propagation unitaire (BCP) consomme jusqu'à 90 % du temps d'exécution d'un solveur CDCL. Sa vélocité détermine la capacité du solveur à traverser l'espace d'états. Une avancée majeure fut l'introduction des **deux littéraux observés** (Two-Watched Literals) par le solveur zChaff \[Moskewicz et al., 2001\]. Au lieu d'analyser chaque clause à chaque assignation, le solveur maintient exactement deux pointeurs sur des littéraux non falsifiés par clause. Tant que ces deux littéraux ne sont pas affectés à Faux, la clause ne peut mathématiquement pas être unitaire ni falsifiée. Cette structure de données a le double avantage de nécessiter très peu d'accès mémoire et de n'engendrer strictement aucune pénalité de maintenance lors du *backjumping* (les pointeurs n'ont pas besoin d'être mis à jour en remontant l'arbre).  
Théoriquement, le mécanisme de BCP possède une propriété de **confluence**. Quelles que soient les clauses unitaires traitées en premier lors d'une réaction en chaîne, le point fixe final de l'assignation partielle (ou la détection inévitable d'un conflit si l'état est unitairement inconsistant) reste identique. La complexité amortie du processus BCP par nœud de recherche est extrêmement faible grâce à l'approche des littéraux observés, permettant aux solveurs de déduire des millions d'implications par seconde.

### **Heuristiques de branchement : VSIDS, EVSIDS, VMTF**

L'apprentissage de clauses n'est efficace que s'il génère des clauses pertinentes. Cela dépend crucialement de l'ordre de branchement des variables, dicté par des heuristiques orientées par l'activité des conflits :

* **VSIDS (Variable State Independent Decaying Sum) :** Introduite par zChaff \[Moskewicz et al., 2001\], cette heuristique associe un score à chaque variable. Lors de l'analyse d'un conflit, les variables impliquées dans la résolution du graphe d'implication voient leur score incrémenté. Périodiquement, les scores de toutes les variables sont divisés par une constante. Ce mécanisme d'oubli privilégie l'exploration des variables associées aux conflits les plus récents, guidant le solveur vers les sous-problèmes locaux très contraints.  
* **EVSIDS (Exponential VSIDS) :** Popularisée par MiniSAT, EVSIDS raffine VSIDS en conservant l'amortissement mathématique sans avoir à mettre à jour les variables non impliquées. Au lieu de diviser les scores existants, l'incrément accordé aux variables fautives augmente exponentiellement au fil des conflits. Les variables s'organisent alors dans une file de priorité flottante à la complexité de maintenance minimale.  
* **VMTF (Variable Move-To-Front) :** Développée par Lawrence Ryan (2004) pour le solveur Siege, cette heuristique abandonne le système de calcul de flottants pour une gestion purement listée. Les variables sont gérées dans une liste doublement chaînée ; lors d'un conflit, un sous-ensemble des variables responsables est immédiatement déplacé en tête de liste. Bien que la mise en œuvre diffère drastiquement d'EVSIDS, leur capacité théorique à exploiter la localité temporelle des conflits est analogue.

## **3\. Solveurs \#SAT et MaxSAT**

Les exigences de \#SAT et MaxSAT diffèrent du paradigme de décision de SAT : là où un solveur SAT s'arrête au premier modèle trouvé, \#SAT et MaxSAT demandent une exploration globale et structurée de tout ou partie de l'espace de recherche.

### **Approches DPLL avec cache de composantes**

Pour résoudre \#SAT sans succomber à une complexité strictement exponentielle en temps, les solveurs modernes étendent DPLL avec le **caching de composantes** (Component Caching). Au fur et à mesure que les variables sont assignées, le graphe d'incidence de la formule CNF résiduelle se fragmente souvent en plusieurs composantes connexes déconnectées. Puisque ces composantes ne partagent aucune variable non assignée, le dénombrement de leurs modèles est mathématiquement indépendant : le nombre de modèles de l'union de ces composantes disjointes est égal au produit de leurs comptes individuels.  
Les solveurs phares de cette génération, comme *Cachet* et ultérieurement *sharpSAT*, stockent les décomptes de ces sous-formules évaluées dans une table de hachage. Si une même sous-formule est rencontrée dans une branche de recherche complètement distincte, son décompte est extrait instantanément du cache ($O(1)$). *sharpSAT* a grandement perfectionné cette architecture en proposant un encodage ultra-compact des composantes pour minimiser les défauts de cache et en intégrant une BCP implicite (Look-Ahead BCP) pour forcer des équivalences précoces, offrant une scalabilité inédite face aux approches DPLL exhaustives naïves.

### **Solveurs MaxSAT modernes et approches algorithmiques**

L'évolution de MaxSAT s'est détachée des approches traditionnelles de *Branch and Bound* pour embrasser la résolution basée sur des oracles SAT (Core-Guided et Implicit Hitting Set), permettant de tirer pleinement profit de l'ingénierie CDCL.

* **Algorithmes Core-Guided (OLL) \- Open-WBO, RC2 :** Les solveurs comme *Open-WBO* et *RC2* s'appuient sur l'extraction itérative de noyaux insatisfaisables (unsatisfiable cores). L'algorithme OLL, d'abord développé pour la programmation par ensemble de réponses et adapté pour MaxSAT, interroge un oracle SAT sur la formule incluant toutes les clauses molles. Si l'oracle retourne UNSAT, il fournit un noyau (un sous-ensemble minimal de clauses contradictoires). Le solveur relâche alors ces clauses fautives en ajoutant des variables de relaxation, et impose via un réseau de tri matériel (Totalizer ou Cardinality Networks) qu'au plus une variable relaxée ne soit active. La limite des variables de relaxation incrémente la pénalité connue, forçant la borne inférieure à se resserrer jusqu'à l'obtention d'une solution SAT.  
* **Implicit Hitting Set (MaxHS) :** L'approche de *MaxHS* orchestre une collaboration entre un solveur SAT (pour l'extraction de noyaux) et un solveur de programmation linéaire en nombres entiers (ILP/MIP). Le solveur SAT identifie la topologie des noyaux insatisfaisables, tandis que le solveur MIP est chargé de trouver un *Hitting Set* (un ensemble intersectant) de poids minimum couvrant tous ces noyaux. Ce Hitting Set dicte quelles clauses doivent être violées au prochain appel SAT. Le processus converge lorsque le solveur SAT trouve un modèle sans contredire le Hitting Set fourni.

### **Bornes inférieures et supérieures pour MaxSAT**

Les algorithmes MaxSAT manipulent implicitement l'optimisation par la gestion de bornes (Bounds).

* **Bornes Supérieures (Upper Bounds) :** Souvent trouvées via des heuristiques locales (recherche stochastique, SAT-UNSAT linéaire), elles représentent le coût actuel de la meilleure assignation connue.  
* **Bornes Inférieures (Lower Bounds) :** Elles quantifient le coût inévitable des conflits. Les algorithmes *Core-Guided* augmentent itérativement la borne inférieure de 1 (ou du poids minimal du noyau pour le Weighted MaxSAT) chaque fois qu'un noyau conflictuel est relaxé et converti en contrainte de cardinalité. L'optimalité est prouvée lorsque la borne inférieure rejoint la borne supérieure.

## **4\. Satisfaisabilité modulo théories (SMT) et Z3**

L'intégration des solveurs dans le génie logiciel formel a nécessité une expressivité bien supérieure à la logique purement propositionnelle, conduisant au développement du problème de Satisfaisabilité Modulo Théories (SMT).

### **Définition de SMT et différence avec SAT pur**

SMT est le problème de décision qui détermine la satisfaisabilité d'une formule du premier ordre sans quantificateurs (ou avec quantificateurs restreints) par rapport à une ou plusieurs théories mathématiques de fond. Là où SAT analyse la formule $(A \\lor \\neg B)$, SMT analyse une formule de type $(x \+ y \\ge 7 \\lor f(x) \\neq f(y))$.  
La différence majeure avec SAT pur réside dans la corrélation sémantique des atomes booléens. Dans SAT, les variables $A$ et $B$ sont totalement indépendantes. Dans SMT, si $A \\equiv (x \< y)$, $B \\equiv (y \< z)$ et $C \\equiv (x \< z)$, la théorie de l'arithmétique linéaire impose que l'assignation $\\{A \= \\text{Vrai}, B \= \\text{Vrai}, C \= \\text{Faux}\\}$ est mathématiquement inconsistante, quand bien même elle satisferait la structure booléenne abstraite de la formule.

### **Architecture des solveurs SMT : DPLL(T)**

Pour éviter la traduction naïve et exponentielle des théories en CNF (bit-blasting de bas niveau), les solveurs modernes adoptent l'architecture DPLL(T).  
Dans DPLL(T), un moteur CDCL rapide est chargé de chercher des modèles sur l'abstraction purement booléenne de la formule SMT. Lorsqu'une assignation propositionnelle partielle est trouvée, les prédicats sous-jacents sont communiqués à un ou plusieurs **T-solvers** (solveurs de théories).  
Le rôle du T-solver est double :

1. Vérifier l'absence d'inconsistances sémantiques. Si une incohérence est détectée, le T-solver génère un **lemme explicatif** (conflict clause) formulé avec les atomes booléens originaux, qui est renvoyé au CDCL. Le moteur SAT assimile ce lemme, effectue un *backjumping* et apprend ainsi la dynamique de la théorie.  
2. Propager les conséquences de la théorie (Theory Propagation). Le T-solver peut déduire que certaines variables non assignées sont forcées par la mathématique (ex : si $x=y$ est vrai, alors $f(x)=f(y)$ l'est aussi via les fonctions non interprétées).

Les théories supportées classiquement incluent : l'arithmétique linéaire entière/réelle (LIA/LRA), les fonctions non interprétées avec égalité (EUF), la théorie des tableaux (Arrays) et les vecteurs de bits de taille fixe (Bit-Vectors).

### **Z3 : Présentation et utilisation**

Z3, conçu par de Moura et Bjørner (2008) pour Microsoft Research, est le solveur SMT de référence dans le monde académique et industriel. Outre l'implémentation hyper-efficace de DPLL(T), Z3 gère les quantificateurs universels et existentiels via le paradigme de l'E-matching, et incorpore des interfaces d'optimisation (OMT) permettant de gérer les spécifications de type MaxSAT.  
Z3 intègre également un moteur complet de manipulation de clauses, de tactiques heuristiques configurables et de simplifications au vol de formules, ce qui permet à l'utilisateur de l'employer indifféremment pour la validation SMT complexe ou pour résoudre des problèmes purs \#SAT et MaxSAT en s'appuyant sur des API de haut niveau.

### **Comparaison des performances SMT vs SAT sur instances propositionnelles**

Bien que l'architecture DPLL(T) des solveurs SMT comme Z3 soit bâtie autour d'un moteur SAT CDCL, leurs performances face à un solveur SAT dédié (tel que CaDiCaL ou Kissat) sur des instances **purement propositionnelles** sont systématiquement en retrait.  
Cette dégradation des temps de calcul s'explique par l'infrastructure lourde maintenue par le solveur SMT. Chaque atome dans Z3 est modélisé sous la forme d'un arbre syntaxique abstrait pour faciliter les potentiels appels aux solveurs de théories. Le maintien de ce graphe asémantique, la communication entre la structure d'état DPLL et les composants de théorie (même inactifs), et l'absence de structures d'accès mémoire aussi resserrées et optimales que les *two-watched literals* d'un solveur SAT strict induisent un overhead majeur. Par conséquent, lors d'un pipeline de résolution logicielle, si un problème peut être modélisé entièrement via un encodage booléen dense, le recours exclusif à un solveur SAT pur reste toujours la solution de facto pour l'efficacité.

## **5\. Compilation de connaissances**

La compilation de connaissances (Knowledge Compilation \- KC) représente une alternative à la résolution ponctuelle d'instances : elle vise à surmonter l'intractabilité du raisonnement propositionnel par un remaniement asymétrique des coûts computationnels.

### **Définition et objectifs**

L'objectif fondamental de la KC est de scinder le processus de résolution en deux phases. La première phase, la **compilation hors-ligne** (off-line), transforme la formule logique initiale (généralement une CNF) vers une structure de représentation cible. Cette phase est autorisée à consommer un temps et un espace exponentiels dans les pires cas.  
La seconde phase, le **questionnement en ligne** (on-line), exploite la structure cible pour répondre à de multiples requêtes (comme le *model counting*, les équivalences ou les inférences causales) en un temps garanti strictement polynomial (souvent linéaire) par rapport à la taille de la structure compilée. Cette asymétrie est particulièrement puissante lorsque la base de connaissances reste statique et doit être interrogée des milliers de fois, comme dans la configuration de produits industriels ou le diagnostic embarqué.

### **Langages de représentation cibles**

Les langages de KC s'étendent des formes normales plates (CNF, DNF) aux graphes orientés acycliques (DAG).

* **BDD (Binary Decision Diagrams) :** Développés par Bryant (1986), les ROBDD fournissent un graphe de décision canonique fondé sur une expansion de Shannon où l'ordre d'évaluation des variables est strict. Si les BDD excellent pour les tests d'équivalence en temps constant, ils requièrent un ordre des variables critique, dont le mauvais choix provoque une explosion combinatoire dramatique de la taille du graphe.  
* **DNNF (Decomposable Negation Normal Form) :** Un DAG de portes AND, OR et de feuilles littérales défini par la propriété de **décomposabilité** : les sous-graphes enfants de tout nœud AND doivent avoir des ensembles de variables propositionnelles complètement disjoints.  
* **d-DNNF (Deterministic DNNF) :** En plus de la décomposabilité, ce langage exige le **déterminisme** : pour chaque nœud OR, les enfants doivent être logiquement mutuellement exclusifs (une seule branche peut être vraie pour toute assignation). C'est ce déterminisme combiné à la décomposabilité qui permet de résoudre \#SAT en temps linéaire par simple sommation et multiplication des valeurs de bas en haut.  
* **SDD (Sentential Decision Diagrams) :** Proposé par Darwiche en 2011, le langage SDD est un sous-ensemble strict des d-DNNF. Il impose une structure basée sur un arbre de variables (vtree). Les SDD offrent une canonicité et des capacités de recombinaison dynamique (Apply) similaires aux BDD tout en se révélant exponentiellement plus succincts sur des problèmes hautement structurés.

### **La hiérarchie de la compilation (KC Map)**

Dans leur article séminal, Darwiche et Marquis (2002) ont dressé une "carte" rigoureuse de la compilation. Les langages y sont évalués et classés selon deux dimensions mathématiques :

1. **L'efficacité spatiale (Succinctness) :** Un langage A est strictement plus succinct qu'un langage B s'il existe une fonction polynomiale capable de traduire tout B en A, mais que la traduction de A en B exige une taille exponentielle.  
2. **L'efficacité temporelle :** La liste des requêtes (Validité, Inconsistance, Dénombrement \- MC, Équivalence \- EQ) et des transformations polynomialement supportées.

*Table 1 : Extrait de la carte de la compilation de connaissances.*

| Langage cible | Succinctness (plus haut \= plus compact) | Requêtes supportées en temps polynomial (sélection) |
| :---- | :---- | :---- |
| **DNNF** | Très élevée | SAT (CO), Clauses (CE) |
| **d-DNNF** | Élevée (sous-ensemble de DNNF) | SAT, Dénombrement (\#SAT/MC), Énumération (ME) |
| **SDD** | Intermédiaire (sous-ensemble de d-DNNF) | SAT, \#SAT, Combinaisons binaires (Apply) |
| **OBDD** | Faible (sous-ensemble strict) | SAT, \#SAT, Équivalence (EQ), Négation |

La KC Map a formellement démontré que le langage d-DNNF est strictement plus succinct que l'OBDD, justifiant les investissements massifs vers la génération de d-DNNF.

### **Compilateurs et lien avec la Treewidth**

La création de d-DNNF a motivé l'écriture de compilateurs "top-down" qui instrumentent l'architecture CDCL avec le caching de composantes de \#SAT.

* **c2d :** Le compilateur pionnier, reposant sur un partitionnement de l'hypergraphe des variables pour diriger les branches de décision.  
* **D4 :** Compilateur moderne de Lagniez et Marquis (2017) \[Lagniez & Marquis, 2017\] générant des d-DNNF avec une gestion extrêmement agressive du partitionnement dynamique et de la mémorisation des composants d'implication.

La taille finale de la représentation d-DNNF compilée (et, intrinsèquement, sa faisabilité) n'est pas dictée par la taille de la formule d'entrée, mais par ses propriétés topologiques. Théoriquement, il est établi que la taille de la compilation est bornée exponentiellement par un paramètre appelé la **largeur d'arbre (treewidth)** du graphe d'incidence de la formule. La topologie régit la complexité.

## **6\. Décompositions arborescentes et paramètres de largeur**

Pour analyser pourquoi des instances SAT gigantesques (mais issues du monde réel) sont solubles en temps polynomial, la théorie de la complexité paramétrée extrait la structure de connectivité des formules via des graphes.

### **Treewidth : Définition formelle**

Introduite de manière exhaustive par Robertson et Seymour dans la théorie des mineurs de graphes, la largeur d'arbre (*treewidth*) mesure le degré de "ressemblance" d'un graphe quelconque à un arbre parfait.  
Pour un graphe $G \= (V, E)$, une décomposition arborescente (tree decomposition) est un couple $(X, T)$ où $T$ est un arbre et $X$ une famille de sous-ensembles (ou "sacs") de sommets $X\_i \\subseteq V$ attachés aux nœuds de l'arbre $T$, obéissant à trois règles :

1. Chaque sommet de $V$ apparaît dans au moins un sac de $X$.  
2. Pour chaque arête $(u, v) \\in E$, il existe un sac contenant simultanément $u$ et $v$.  
3. Pour chaque sommet $v \\in V$, les nœuds de l'arbre $T$ dont les sacs contiennent $v$ forment un sous-arbre strictement connexe de $T$.

La largeur de cette décomposition est définie comme le cardinal du plus grand sac moins un ($\\max |X\_i| \- 1$). La treewidth (tw) du graphe est le minimum de cette largeur sur toutes les décompositions possibles. Bien que trouver la treewidth optimale soit un problème NP-difficile en général, le problème de vérifier si $tw \\le k$ (et de construire l'arbre) est Fixed-Parameter Tractable (FPT) pour un $k$ fixé.

### **Algorithmes DP paramétrés par la treewidth**

Une décomposition arborescente fournit une fondation rigoureuse pour les algorithmes de programmation dynamique (DP) capables de résoudre \#SAT et MaxSAT de bas en haut (depuis les feuilles de l'arbre vers sa racine).

* **Samer et Szeider (2010) :** Ils ont formalisé le premier algorithme DP traitant \#SAT paramétré par la treewidth du graphe d'incidence (variables et clauses constituent les sommets). Pour chaque sac, une table maintient la traçabilité des assignations des variables et le statut de satisfaction des clauses du sac. La taille de la table de chaque nœud croît exponentiellement avec la taille du sac, induisant une complexité d'algorithme globale en temps $O^\*(4^k)$, où $k$ est l'incidence treewidth.  
* **Slivovsky et Szeider (2020) :** Ils ont fondamentalement amélioré ce résultat, écrasant la complexité à $O^\*(2^k)$. La complexité majeure provenait de la jonction des tables aux nœuds internes de l'arbre de décomposition. Les auteurs ont redéfini cette étape de jonction comme le calcul d'un **produit couvrant** (covering product) algébrique sur les sous-ensembles de clauses. Par l'exploitation combinée de la Transformée Rapide de Zeta et de l'inversion de Möbius, les itérations naïves sur toutes les paires d'états ont été remplacées par une multiplication matricielle en temps optimisé, atteignant ainsi la borne asymptotique inférieure imposée par la conjecture SETH (Strong Exponential Time Hypothesis).

### **Paramètres alternatifs : Clique-width, Rank-width, Mim-width**

La treewidth est limitative car elle n'est bornée que pour des graphes globalement clairsemés (sparses). D'autres paramètres visent à capturer la "simplicité" structurelle de graphes denses.

* **Clique-width et Rank-width :** La clique-width évalue la complexité via des opérations de création et de fusion d'ensembles de sommets. La rank-width analyse le rang booléen de la matrice d'adjacence entre les partitions du graphe. Ces deux métriques dominent théoriquement la treewidth : tout graphe de treewidth bornée possède une clique-width bornée ($cw \\le 3 \\cdot 2^{tw-1}$), mais un graphe complètement dense (clique pure) a une treewidth non bornée tout en ayant une clique-width de 2\.  
* **Mim-width :** Introduite par Vatshelle (2012) \[Vatshelle, 2012\], la *Maximum Induced Matching Width* évalue l'ampleur de la communication (via la taille du plus grand couplage induit) entre les deux sous-graphes générés par toute coupe dans une *branch decomposition* (décomposition par branchement). La mim-width est d'une généralité suprême : des familles entières de graphes avec une clique-width asymptotiquement infinie (comme les graphes d'intervalles ou de permutation) conservent une mim-width bornée (linéaire) à 1\.

### **L'Intractabilité de la Mim-width (ICALP 2025\)**

L'avantage théorique indiscutable de la mim-width fut terni par la difficulté algorithmique à générer la décomposition requise. En 2025, le travail révolutionnaire de Bergougnoux, Bonnet et Duron publié au congrès ICALP a mis fin à une question ouverte persistante : **calculer ou approximer la mim-width est un problème paraNP-complet**.  
Par le biais d'une réduction extrêmement complexe depuis la variante de satisfaisabilité *4-Occ Not-All-Equal 3-Sat*, les auteurs démontrent qu'il est formellement NP-difficile de discerner un graphe de mim-width linéaire inférieure à 1211 d'un graphe dont la largeur dépasse 1216\. Cette preuve clôt la discussion théorique : contrairement à la treewidth, il ne peut exister d'algorithme FPT ou même polynomial (XP) pour extraire mécaniquement cette décomposition, reléguant la mim-width à un outil analytique ou exigeant des heuristiques pures pour l'ingénierie SAT.

## **7\. PS-width et l'algorithme de Sæther-Telle-Vatshelle (2015)**

Pour transposer la puissance de la mim-width directement à l'analyse de formules SAT, Sæther, Telle et Vatshelle (JAIR 2015\) ont introduit la notion purement logique de **ps-width**.

### **Graphe d'incidence et définition de la ps-width**

L'approche se base sur le graphe d'incidence de la formule CNF, un graphe biparti modélisant les liens entre les clauses et les variables qui les composent. Une *branch decomposition* structure ce graphe d'incidence sous la forme d'un arbre binaire, où les feuilles sont les variables et les clauses.  
La largeur de projection-satisfaisabilité (**ps-width**) évalue chaque arête de cet arbre (chaque coupe scindant la formule en deux). Pour une coupe donnée, on s'intéresse au nombre de *Projection-Satisfiable Sets* (ensembles de clauses conditionnellement satisfaisables), c'est-à-dire les sous-ensembles de clauses traversant la coupe qui peuvent être rendus Vrais par une assignation unilatérale d'un côté de la coupe. La taille maximale (base log2) de ces ensembles pour toutes les coupes définit la ps-width de la décomposition.  
En termes de relation de domination mathématique, la ps-width domine directement la mim-width du graphe d'incidence (si la ps-width est bornée, la mim-width l'est obligatoirement), positionnant ce paramètre au sommet de la hiérarchie de décomposition, très au-dessus de la treewidth.

### **L'Algorithme de Programmation Dynamique**

L'article présente un algorithme DP ascendant s'exécutant sur l'arbre de la décomposition de branchement pour évaluer \#SAT et Weighted MaxSAT. L'état DP en chaque nœud interne stocke précisément les PS-sets et leurs évaluations dynamiques (comptages ou poids optimums).  
La mécanique algorithmique est découpée en trois procédures rigoureuses :

* **Procédure 1 (Feuilles) :** Établit les blocs de base en isolant la projection locale pour une seule clause ou une seule variable.  
* **Procédure 2 (Extensions conditionnelles) :** Opère des mises à jour des ensembles lors de l'assimilation d'arcs d'adjacence locaux.  
* **Procédure 3 (Nœuds de jonction) :** Le cœur du processus, fusionnant les tables de PS-sets provenant des branches gauche et droite du nœud de décomposition par le biais d'unions conditionnelles.  
  L'algorithme aboutit à une complexité temporelle pire-cas de $O(k^3 \\cdot m \\cdot (m+n))$, où $k$ est la ps-width de l'arbre fourni, $n$ les variables et $m$ les clauses, offrant ainsi une résolution de complexité polynomiale pour les instances à faible ps-width.

### **L'heuristique GreedyOrder et les instances types**

Puisque générer une décomposition optimale minimisant la ps-width est mathématiquement irréalisable en temps polynomial, les auteurs implémentent **GreedyOrder**, une heuristique gloutonne qui choisit séquentiellement les sommets du graphe d'incidence possédant le plus petit degré de connectivité aux partitions inexplorées.  
Leur validation empirique est effectuée sur trois types de configurations combinatoires modélisées pour contrecarrer les solveurs DPLL/CDCL et favoriser DP :

* **Type 1 :** Instances modélisées par des graphes bipartis d'intervalles (interval bigraphs).  
* **Type 2 :** Structures identiques au Type 1, mais générées strictement avec de petites clauses, induisant souvent un statut UNSAT rapide où *sharpSAT* excelle via sa propagation unitaire foudroyante.  
* **Type 3 :** Formules CNF extraites de fonctions logiques XOR cycliques superposées, formant des graphes bipartis d'arcs de cercle (circular arc bigraphs). Sur ces instances algébriques circulaires, l'algorithme basé sur la ps-width écrase fondamentalement les outils modernes CDCL, ces derniers se perdant dans l'absence totale de modularité arborescente classique (treewidth).

## **8\. Backdoors pour SAT**

Tandis que la largeur d'arbre et la ps-width paramétrisent l'intégralité du graphe sous-jacent, la théorie des "Backdoors" étudie le phénomène fascinant des sous-ensembles "cachés" de variables qui, une fois assignés, pulvérisent instantanément la complexité de l'entièreté de la formule CNF résiduelle.

### **Définitions des backdoors forts et faibles**

La notion fut théorisée par Williams, Gomes et Selman en 2003\. Un sous-ensemble de variables $B$ forme un **backdoor fort** (strong backdoor) vis-à-vis d'une classe de formules théoriquement tractables $\\mathcal{C}$ (comme les formules 2CNF ou de Horn) si, et seulement si, **quelle que soit** l'assignation de valeurs appliquées au sous-ensemble $B$, la sous-formule résiduelle obtenue appartient intégralement à la classe $\\mathcal{C}$ (et peut donc être résolue en temps linéaire).  
Un **backdoor faible** (weak backdoor) assouplit cette contrainte : il demande uniquement qu'il **existe au moins une** assignation des variables de $B$ aboutissant à une formule dans la classe $\\mathcal{C}$ qui soit certifiée SAT (modèle utile pour la recherche précipitée de satisfaisabilité en environnement industriel).

### **Détection, Complexité et Treewidth**

Trouver le plus petit backdoor possible est NP-difficile. Néanmoins, en théorie de la complexité paramétrée, Nishimura, Ragde et Szeider (2004) ont prouvé que la détection d'un backdoor de taille au plus $k$ vers les classes Horn et 2CNF est FPT. Plus tard, Gaspers et Szeider (2013) ont redéfini la portée de ces portes dérobées non plus vers des syntaxes fixes (Horn), mais vers des critères topologiques : les **backdoors vers la treewidth bornée** ($\\mathcal{W}\_t$), assurant qu'une fois $k$ variables assignées, la treewidth du graphe restant tombe sous la constante $t$, permettant l'exécution de l'algorithme FPT.  
La perspective s'est encore approfondie avec la notion de **Backdoor Treewidth** (Ganian, Ramanujan et Szeider, 2017). Plutôt que de borner brutalement la taille absolue du backdoor, cette théorie exploite la treewidth de l'ensemble de backdoor *lui-même*, créant des hiérarchies hybrides offrant des limites algorithmiques bien plus tolérantes pour la résolution structurelle.

### **Implication des solveurs CDCL et Backdoors Récursifs**

Une limite théorique de ces backdoors statiques est qu'ils ignoraient l'adaptation du solveur. Dilkina, Gomes et Sabharwal (2009) ont intégré ce paramètre dynamique avec les **Learning-Sensitive Backdoors** (LSB). En intégrant l'apprentissage de clauses (CDCL) à l'évaluation, ils ont mathématiquement prouvé qu'un backdoor LSB peut être de taille *exponentiellement plus petite* qu'un backdoor statique classique. La clause acquise lors d'un conflit modifie la structure du problème au fil de l'exécution, réduisant de facto la liste des variables nécessaires pour atteindre la classe polynomiale.  
Sur le plan topologique, Mählmann, Siebertz et Vigny (ICALP 2021\) ont défini les **backdoors récursifs**. Observant qu'assigner une variable scinde souvent la formule en composantes disjointes, la taille absolue de la porte dérobée n'est plus la métrique optimale. La *profondeur de backdoor* (backdoor depth) mesure le niveau récursif nécessaire pour désamorcer l'instance. Ce changement métrique valide qu'une formule avec une taille de backdoor infinie peut posséder une profondeur de backdoor purement constante et traitable.

### **Applications pratiques : cryptanalyse et factorisation SAT**

La puissance prédictive de la théorie des backdoors s'incarne de manière pragmatique dans l'évaluation de la robustesse cryptographique. Semenov, Zaikin et al. (AAAI 2018\) ont transposé l'extraction de backdoors à l'attaque cryptanalytique par **guess-and-determine**. Dans ce scénario, attaquer un registre à décalage (A5/1) ou craquer une fonction de hachage inversée revient à localiser le backdoor fort minimal de l'encodage SAT de l'algorithme. Les variables du backdoor dictent précisément quels bits du flux cryptographique "deviner" en premier. La recherche du backdoor s'effectue par des oracles de Monte-Carlo mimant les exécutions CDCL, transformant la cryptanalyse en un calcul d'optimisation en boîte noire de la topologie SAT.

## **9\. Circuits de multiplication et transformation de Tseytin**

La modélisation de fonctions arithmétiques pures constitue le test ultime (et souvent la limite infranchissable) des solveurs SAT modernes, le circuit de multiplication booléenne en étant l'exemple le plus didactique.

### **Transformation de Tseytin (1968)**

Pour qu'un circuit logique combinatoire soit analysé par DPLL/CDCL ou par un compilateur de connaissances, il doit d'abord être retranscrit sous forme normale conjonctive (CNF). Appliquer rigoureusement les axiomes de la distributivité logique provoque une explosion combinatoire exponentielle de la taille de la formule.  
La transformation de Tseytin (1968) esquive cette explosion par la substitution de variables. Elle instaure une nouvelle variable booléenne auxiliaire pour la sortie de chaque sous-porte logique du circuit. Les relations comportementales locales (entrées vers sortie) de la porte (ex. AND, XOR) sont traduites de manière isolée en quelques clauses locales de petite taille. L'équation globale est la conjonction de toutes ces définitions assortie de l'assertion de la variable de sortie finale. Cette mécanique mathématique génère une formule strictement équi-satisfaisable dont la taille grandit de manière purement linéaire avec le nombre de portes du circuit initial.

### **Structure d'un multiplicateur $k \\times k$ bits**

L'encodage d'un multiplicateur binaire standard (array multiplier) illustre parfaitement les murs de complexité matériels. La multiplication de deux entiers de $k$ bits passe par la production matricielle de calculs partiels en grille, qui sont en cascade additionnés par un maillage bidimensionnel strict composé de demi-additionneurs (Half Adders) et d'additionneurs complets (Full Adders). Chaque additionneur relaie ses données via des bits de retenue (carry bits) asynchrones.  
La traduction de la matrice d'opérations logiques sous la transformation canonique de Tseytin injecte inévitablement un volume de variables auxiliaires et de clauses de l'ordre de $O(k^2)$, créant un réseau CNF à dépendances circulaires asymétriques intenses.

### **Mim-width des grilles et enjeux de la cryptanalyse**

La structure de connectivité interne de ce multiplicateur s'apparente purement et simplement à une grille bidimensionnelle $k \\times k$. Or, le théorème de Vatshelle (2012) démontre mathématiquement que la mim-width d'une grille $n \\times n$ augmente de manière linéaire proportionnelle à sa taille, c'est-à-dire qu'elle évolue en $\\ge n/3$ \[Vatshelle, 2012\].  
Par induction stricte, parce que la ps-width d'une formule domine irrémédiablement sa mim-width, la ps-width du multiplicateur matriciel explose elle aussi avec $k$. Les conséquences pour la résolution par les algorithmes paramétrés ou DPLL exhaustifs sont fatales : l'absence de limite supérieure stricte (unboundedness) sur ces paramètres structurels invalide l'espoir d'obtenir une compilation en temps FPT via l'algorithme de Sæther-Telle-Vatshelle (2015) ou tout autre solveur dynamique arborescent.  
C'est précisément cette inattaquabilité topologique qui garantit la résilience intrinsèque des systèmes asymétriques basés sur la factorisation de nombres entiers bipremiers face aux assauts purs des solveurs de satisfaisabilité : ni l'ingénierie CDCL/EVSIDS, ni les backdoors, ni la topologie des largeurs arborescentes ne parviennent à dénouer l'enchevêtrement exponentiel des bits de retenue d'un multiplicateur.

## ---

**Références**

Benjamin Bergougnoux, Édouard Bonnet, Julien Duron. "Mim-Width is paraNP-complete". In : 52nd International Colloquium on Automata, Languages, and Programming (ICALP 2025). DOI : 10.4230/LIPIcs.ICALP.2025.25.  
Randal E. Bryant. "Graph-Based Algorithms for Boolean Function Manipulation". In : IEEE Transactions on Computers, Vol. C-35, No. 8, 1986\.  
\[Cook, 1971\] Stephen A. Cook. "The complexity of theorem-proving procedures". In : Proceedings of the third annual ACM symposium on Theory of computing (STOC), 1971\.  
Adnan Darwiche. "Decomposable negation normal form". In : Journal of the ACM, 48(4):608–647, 2001\.  
Adnan Darwiche. "New Advances in Compiling CNF into Decomposable Negation Normal Form". In : ECAI, 2004\.  
Adnan Darwiche. "SDD: A New Canonical Representation of Propositional Knowledge Bases". In : IJCAI, 2011\.  
Adnan Darwiche, Pierre Marquis. "A Knowledge Compilation Map". In : Journal of Artificial Intelligence Research (JAIR) 17, 2002\.  
Leonardo de Moura, Nikolaj Bjørner. "Z3: An Efficient SMT Solver". In : TACAS, 2008\.  
Bistra Dilkina, Carla P. Gomes, Ashish Sabharwal. "Backdoors in the Context of Learning". In : SAT 2009, LNCS 5584, 2009\.  
Robert Ganian, M. S. Ramanujan, Stefan Szeider. "Backdoor Treewidth for SAT". In : SAT 2017, LNCS 10491, 2017\.  
Serge Gaspers, Stefan Szeider. "Strong Backdoors to Bounded Treewidth SAT". In : 54th Annual IEEE Symposium on Foundations of Computer Science (FOCS), 2013\.  
\[Karp, 1972\] Richard M. Karp. "Reducibility Among Combinatorial Problems". In : Complexity of Computer Computations, 1972\.  
\[Lagniez & Marquis, 2017\] Jean-Marie Lagniez, Pierre Marquis. "An Improved Decision-DNNF Compiler". In : IJCAI, 2017\.  
Nikolas Mählmann, Sebastian Siebertz, Alexandre Vigny. "Recursive Backdoors for SAT". In : MFCS 2021, LIPIcs Vol 202, 2021\.  
\[Moskewicz et al., 2001\] Matthew W. Moskewicz, Conor F. Madigan, Ying Zhao, Lintao Zhang, Sharad Malik. "Chaff: Engineering an Efficient SAT Solver". In : 38th Design Automation Conference (DAC), 2001\.  
Lawrence Ryan. "Efficient algorithms for clause learning SAT solvers". Master's thesis, Simon Fraser University, 2004\.  
Sigve Hortemo Sæther, Jan Arne Telle, Martin Vatshelle. "Solving \#SAT and MaxSAT by Dynamic Programming". In : Journal of Artificial Intelligence Research (JAIR) 54, 2015\.  
Marko Samer, Stefan Szeider. "Algorithms for propositional model counting". In : Journal of Discrete Algorithms 8(1), 2010\.  
Alexander Semenov, Oleg Zaikin, Ilya Otpuschennikov, Stepan Kochemazov, Alexey Ignatiev. "On Cryptographic Attacks Using Backdoors for SAT". In : AAAI 2018\.  
Friedrich Slivovsky, Stefan Szeider. "A Faster Algorithm for Propositional Model Counting Parameterized by Incidence Treewidth". In : SAT 2020, LNCS 12178, 2020\.  
Marc Thurley. "sharpSAT – Counting Models with Advanced Component Caching and Implicit BCP". In : SAT 2006, LNCS 4121, 2006\.  
Grigori S. Tseitin. "On the complexity of derivation in propositional calculus". In : Structures in Constructive Mathematics and Mathematical Logic, Part II, 1968\.  
\[Valiant, 1979\] Leslie G. Valiant. "The Complexity of Enumeration and Reliability Problems". In : SIAM Journal on Computing, 1979\.  
\[Vatshelle, 2012\] Martin Vatshelle. "New width parameters of graphs". PhD Thesis, University of Bergen, 2012\.  
Ryan Williams, Carla P. Gomes, Bart Selman. "Backdoors to Typical Case Complexity". In : IJCAI 2003\.