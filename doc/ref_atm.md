### Référence principale du projet

1. **Saether, S.H., Telle, J.A., Vatshelle, M.** (2015). *Solving #SAT and MaxSAT by Dynamic Programming*. Journal of Artificial Intelligence Research (JAIR), 54, pp. 59-82.

### Backdoors pour SAT

2. **Williams, R., Gomes, C., Selman, B.** (2003). *Backdoors to Typical Case Complexity*. Proceedings of IJCAI 2003, pp. 1173-1178.
   — *Article fondateur introduisant la notion de backdoor.*

3. **Gaspers, S., Szeider, S.** (2013). *Strong Backdoors to Bounded Treewidth SAT*. Proceedings of FOCS 2013, pp. 489-498. IEEE.
   — *Algorithme cubique pour trouver un backdoor fort vers la treewidth bornée. Résultat clé pour la combinaison backdoor + décomposition.*

4. **Gaspers, S., Szeider, S.** (2012). *Backdoors to Satisfaction*. In: Bodlaender et al. (eds.), The Multivariate Algorithmic Revolution and Beyond, LNCS 7370, pp. 287-317. Springer.
   — *Survey sur les backdoors et la complexité paramétrée.*

5. **Fomin, F.V., Lokshtanov, D., Misra, N., Ramanujan, M.S., Saurabh, S.** (2015). *Solving d-SAT via Backdoors to Small Treewidth*. Proceedings of SODA 2015.
   — *Étend Gaspers & Szeider à la treewidth $t > 1$ avec temps FPT linéaire en l'input.*

### Backdoor Treewidth

6. **Ganian, R., Ramanujan, M.S., Szeider, S.** (2017). *Backdoor Treewidth for SAT*. Proceedings of SAT 2017, LNCS 10491, pp. 20-37. Springer.
   — *Introduit le concept de "backdoor treewidth" : exploiter la structure arborescente du backdoor plutôt que sa taille. FPT pour Horn, Anti-Horn, 2CNF.*

### Backdoors récursifs et avancés

7. **Mählmann, N., Siebertz, S., Vigny, A.** (2021). *Recursive Backdoors for SAT*. Proceedings of ICALP 2021.
   — *Notion de backdoor récursif qui permet des backdoors de taille non bornée mais de profondeur bornée.*

8. **Dreier, J., Ordyniak, S., Szeider, S.** (2022). *SAT Backdoors: Depth Beats Size*. Proceedings of ESA 2022, LIPIcs 244:46.
   — *Algorithmes d'approximation FPT pour la "backdoor depth" vers Horn et Krom.*

### Interval bigraphs

9. **Müller, H.** (1997). *Recognizing Interval Digraphs and Interval Bigraphs in Polynomial Time*. Discrete Applied Mathematics, 78(1-3), pp. 189-205.
   — *Premier algorithme polynomial de reconnaissance des interval bigraphs.*

10. **Rafiey, A.** (2022). *Recognizing Interval Bigraphs by Forbidden Patterns*. Journal of Graph Theory, 99(4).
    — *Algorithme amélioré en $O(nm)$ basé sur les forbidden patterns.*

11. **Hell, P., Huang, J.** (2004). *Interval Bigraphs and Circular Arc Graphs*. Journal of Graph Theory, 46(4), pp. 313-327.
    — *Montre que les interval bigraphs sont les graphes bipartis dont le complément est un graphe d'arcs circulaires sans deux arcs couvrant le cercle entier.*

### Vertex Deletion

12. **Lewis, J.M., Yannakakis, M.** (1980). *The Node-Deletion Problem for Hereditary Properties is NP-Complete*. Journal of Computer and System Sciences, 20(2), pp. 219-230.
    — *Résultat classique : vertex deletion est NP-complet pour toute propriété héréditaire non triviale.*

13. **Cai, L.** (1996). *Fixed-Parameter Tractability of Graph Modification Problems for Hereditary Properties*. Information Processing Letters, 58(4), pp. 171-176.
    — *FPT par branching si la famille de sous-graphes interdits est finie.*

14. **Van 't Hof, P., Villanger, Y.** (2013). *Proper Interval Vertex Deletion*. Algorithmica, 65(4), pp. 845-867.
    — *Algorithme FPT en $O(6^k \cdot k \cdot n^6)$ pour la suppression vers les proper interval graphs.*

### Treewidth et #SAT

15. **Samer, M., Szeider, S.** (2010). *Algorithms for Propositional Model Counting*. Journal of Discrete Algorithms, 8(1), pp. 50-64.
    — *Algorithme DP classique pour #SAT paramétré par la treewidth incidente, en $O(4^k \cdot |F|)$.*

16. **Slivovsky, F., Szeider, S.** (2020). *A Faster Algorithm for Propositional Model Counting Parameterized by Incidence Treewidth*. Proceedings of SAT 2020, LNCS 12178. Springer.
    — *Améliore le facteur exponentiel de $4^k$ à $2^k$ pour #SAT paramétré par treewidth incidente.*

### Backdoors et heuristiques pratiques

17. **Williams, R., Gomes, C., Selman, B.** (2003). *Backdoors to Typical Case Complexity*. Proceedings of IJCAI 2003, pp. 1173-1178.
    — *Article fondateur introduisant la notion de backdoor.*

18. **Dilkina, B., Gomes, C., Sabharwal, A.** (2009). *Backdoors in the Context of Learning*. Proceedings of SAT 2009.
    — *Introduit les "Learning-Sensitive Backdoors" exploitant l'activité VSIDS/CDCL.*

19. **Kilby, P., Slaney, J., Thiébaux, S., Walsh, T.** (2005). *Backbones and Backdoors in Satisfiability*. Proceedings of AAAI 2005.
    — *Relation entre backbones (variables fixées dans toute solution) et backdoors.*

20. **Ruan, Y., Kautz, H., Horvitz, E.** (2004). *The Backdoor Key: A Path to Understanding Problem Hardness*. 
    — *Lien entre taille du backdoor et difficulté empirique des instances.*

21. **Nishimura, N., Ragde, P., Szeider, S.** (2004). *Detecting Backdoor Sets with Respect to Horn and Binary Clauses*. Proceedings of SAT 2004, pp. 96-103.
    — *Premiers résultats FPT pour la détection de backdoors vers Horn et 2CNF.*
