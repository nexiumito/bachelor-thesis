# Procédure de déploiement et de lancement du benchmark sur racer

> Convention de paths utilisée dans ce document :
> - **Sur racer** : `~/bachelor/bachelor-thesis/`
> - **Sur la machine de dev locale** : `~/Documents/GitHub/bachelor/bachelor-thesis/`
>
> Si tu changes ces chemins, substitue-les partout dans les commandes ci-dessous.

## Étape A — Préparation initiale (une seule fois)

```bash
# 1. SSH + clone
ssh ebussod@racer
mkdir -p ~/bachelor && cd ~/bachelor
git clone <URL_DU_REPO_PUBLIC> bachelor-thesis
cd bachelor-thesis

# 2. Compiler sat_solver
make -C src

# 3. Créer un venv Python pour le harness
python3 -m venv .venv-bench
source .venv-bench/bin/activate
pip install --upgrade pip
pip install -e benchmarks

# 4. Vérifier que Z3 marche
python3 -c "from z3 import Optimize, Bool, Or; print('Z3 OK')"
```

Sur ta **machine de dev locale**, installe `rsync` si absent (utilisé en
étapes D.8 et G pour rapatrier les figures) :

```bash
sudo apt install rsync   # Debian / Ubuntu
```

---

## Étape B — Configuration des notifications Discord (une seule fois)

```bash
# Édite ~/.bashrc et ajoute la ligne suivante avec le webhook réel
# (NE LE COLLE JAMAIS DANS UN FICHIER VERSIONNÉ) :
echo 'export DISCORD_WEBHOOK_URL="<URL_DU_WEBHOOK>"' >> ~/.bashrc
source ~/.bashrc

# Sanity check : la variable est bien set
echo $DISCORD_WEBHOOK_URL | head -c 50

# Test manuel : envoie un message au salon Discord
python3 -c "
import os, requests
r = requests.post(os.environ['DISCORD_WEBHOOK_URL'],
                  json={'content': 'Test depuis racer — OK'})
print(r.status_code)
"
# Attendu : 204 (Discord renvoie 204 No Content sur succès).
```

Variante Telegram (à la place de Discord) :

```bash
echo 'export TELEGRAM_TOKEN="..."' >> ~/.bashrc
echo 'export TELEGRAM_CHAT_ID="..."' >> ~/.bashrc
# Puis dans benchmarks/config/benchmark.yaml : notifications.channel: telegram
```

Si la variable n'est pas définie, le bench tourne en mode no-op silencieux.

---

## Étape C — Validation du mapping CCD (une seule fois)

```bash
ssh ebussod@racer
lscpu --extended | head -30
```

Repère la colonne L3 (cache id). Vérifie que les CPU 0, 8, 16, 24 ont
**4 valeurs différentes** (= 4 CCDs distincts du socket 0).

Si non, identifie 4 CPUs sur 4 CCDs distincts du socket 0 et édite
`benchmarks/config/benchmark.yaml` :

```yaml
machine:
  passe_b_taskset_cpus: [<a>, <b>, <c>, <d>]
```

Puis note les valeurs choisies dans
`bachelor-thesis-private/docs/IMPLEMENTATION_LOG.md` section
"Mapping CCD validé sur racer".

---

## Étape D — Validation pré-bench

### D.1 — Test direct du flag `--json`

```bash
ssh ebussod@racer
cd ~/bachelor/bachelor-thesis
source .venv-bench/bin/activate

cd src
./sat_solver ../data/exemple1.cnf greedy --json | python3 -m json.tool
# Attendu : un JSON pretty-printé avec dnnf_count_match: true
cd ..
```

### D.2 — Vérification 0 fuite mémoire (ASan)

```bash
cd ~/bachelor/bachelor-thesis/src
make asan
./sat_solver ../data/exemple1.cnf manual --json 2>&1 | grep -i sanitizer
# Attendu : aucune ligne renvoyée (= 0 fuite)
make rebuild   # rebuild en mode release pour le bench
cd ~/bachelor/bachelor-thesis
```

### D.3 — Test Z3 stats keys après install

```bash
cd ~/bachelor/bachelor-thesis
source .venv-bench/bin/activate
PYTHONPATH=benchmarks python3 -c "
from runners.run_z3 import run_z3_maxsat
class I:
    id = 'test'
    path = '../data/random/random_k3_v8_c20.cnf'
r = run_z3_maxsat(I(), 30, repo_root='.')
print('z3_status     =', r.get('z3_status'))
print('z3_conflicts  =', r.get('z3_conflicts'))
print('z3_stats_keys =', r.get('z3_stats_keys_available')[:120])
"
# Attendu :
#   z3_status = sat
#   z3_conflicts non-null (entier)
#   z3_stats_keys contient 'sat conflicts' et 'sat decisions'
```

### D.4 — Dry-run de l'orchestrateur

```bash
cd ~/bachelor/bachelor-thesis
python3 benchmarks/orchestrator.py --dry-run
# Attendu : affiche le nombre d'instances, de tâches passe A DP, passe A Z3.
# Aucun sous-process lancé.
```

### D.5 — Smoke test (3 instances triviales, < 60 s)

```bash
make -C benchmarks bench-smoke
```

Inspection :

```bash
LATEST=$(ls -t benchmarks/results/ | head -1)
ls benchmarks/results/$LATEST/
cat benchmarks/results/$LATEST/SUMMARY.md
```

Attendu : `structure.csv` (>= 6 lignes DP + Z3), `timings.csv`,
`invariants.csv`, `figures/*.pdf` (8 plots), `SUMMARY.md`.

### D.6 — Vérification des invariants après smoke

```bash
LATEST=$(ls -t benchmarks/results/ | head -1)
column -t -s, benchmarks/results/$LATEST/invariants.csv | head -30

# Plus rapide : juste les FAIL hard
grep ",FAIL," benchmarks/results/$LATEST/invariants.csv || echo "Aucun FAIL"
```

Attendu : aucune ligne `FAIL` avec `severity=hard`. OK sur I1/I2/I4 pour
les instances `type3`.

### D.7 — Test Ctrl-C + reprise auto

Important : `make bench-smoke` crée un **nouveau dossier timestampé à chaque
appel** ; pour valider la reprise, il faut explicitement pointer vers le
dossier du run interrompu via `--resume`.

```bash
make -C benchmarks bench-smoke &
BENCH_PID=$!
sleep 5
kill -INT $BENCH_PID
wait $BENCH_PID 2>/dev/null
echo "Exit code (echo, pas un vrai check) : 130"

# Vérifier les CSV partiels
LATEST=$(ls -t benchmarks/results/ | head -1)
wc -l benchmarks/results/$LATEST/structure.csv

# Relancer EN REPRENANT le run interrompu (sinon nouveau dossier vierge) :
python3 benchmarks/orchestrator.py --resume benchmarks/results/$LATEST \
    --instances type1_v20_c25,type3_n30_t3_s2,random_k3_v8_c20
# Attendu : logs du type "skip N deja faites"
```

Note : le smoke test étant très rapide (~7 s), le `sleep 5 + kill -INT`
peut arriver après que la passe A soit déjà finie ; dans ce cas le test
ne démontre pas réellement la coupure mid-run, mais il valide bien la
reprise via `--resume` (ce qui est le scénario important pour le bench long).

### D.8 — Vérification visuelle des 8 plots

**Sur racer** : lister puis régénérer les plots (la cible `plot` cible
automatiquement le dernier run via `--resume`) :

```bash
# Sur racer :
LATEST=$(ls -t benchmarks/results/ | head -1)
ls benchmarks/results/$LATEST/figures/

# Régénération des plots seuls (sans relancer le bench)
make -C benchmarks plot
```

**Sur ta machine de dev locale** (PAS sur racer — ouvrir un nouveau terminal,
sinon le `ssh` rebondit vers racer et les chemins ne correspondent pas) :

```bash
# Depuis n'importe quel dossier de la machine locale :
LATEST=$(ssh ebussod@racer "ls -t bachelor/bachelor-thesis/benchmarks/results/" | head -1)
echo "LATEST=$LATEST"
mkdir -p /tmp/figures
rsync -av ebussod@racer:bachelor/bachelor-thesis/benchmarks/results/$LATEST/figures/ /tmp/figures/
xdg-open /tmp/figures/time_vs_pswidth.pdf   # ou n'importe quel viewer PDF
```

Attendu : 8 PDF + 8 PNG dans `figures/`. Les PDF doivent être vectoriels
(`file figures/*.pdf` mentionne "PDF document").

---

## Étape E — Lancement du benchmark complet

```bash
ssh ebussod@racer
cd ~/bachelor/bachelor-thesis
git pull                          # toujours pull avant un bench long
source .venv-bench/bin/activate

tmux new -s bench
make -C benchmarks bench

# Détache la session : Ctrl-b puis d
# Le job continue à tourner même si SSH coupe.
```

Tu reçois un message Discord au démarrage avec l'ETA. Puis un toutes les 30 min.
Plus une alerte sur chaque FAIL hard. Plus le récap final.

#### Reprise après interruption (Ctrl-C, SSH coupé, OOM…)

`make bench` crée un nouveau dossier `results/<UTC>/` à chaque appel ;
relancer `make bench` directement repart de zéro. Pour reprendre le run
interrompu et skipper les tâches déjà faites, utiliser `--resume` :

```bash
ssh ebussod@racer
cd ~/bachelor/bachelor-thesis
source .venv-bench/bin/activate
LATEST=$(ls -t benchmarks/results/ | head -1)
echo "Reprise de : $LATEST"
tmux new -s bench
python3 benchmarks/orchestrator.py --resume benchmarks/results/$LATEST
```

---

## Étape F — Surveillance pendant le run (depuis n'importe où)

```bash
# Tail du heartbeat
ssh ebussod@racer "tail -f bachelor/bachelor-thesis/benchmarks/results/\$(ls -t bachelor/bachelor-thesis/benchmarks/results/ | head -1)/progress.log"

# Voir les CSV en cours
ssh ebussod@racer "wc -l bachelor/bachelor-thesis/benchmarks/results/\$(ls -t bachelor/bachelor-thesis/benchmarks/results/ | head -1)/structure.csv"

# Récupérer la session tmux
ssh ebussod@racer
tmux attach -t bench
```

### F.4 — Passe C (bench des requêtes sur DAG compilé)

La passe C est lancée automatiquement après les passes A et B si
`passe_c.enabled: true` dans `benchmark.yaml` (défaut). Pour la lancer
isolément (par exemple si on veut juste rejouer cette mesure sans relancer
A et B) :

```bash
python3 benchmarks/orchestrator.py --only-passe-c \
        --output benchmarks/results/<TIMESTAMP>
# Ou via la cible Make :
make -C benchmarks bench-query
```

**Durée typique** : ~30 min sur racer (110 instances OK greedy filtrées par
`passe_c.dnnf_nodes_max=1e6` ; 1 appel solveur par instance, 5 répétitions
internes par requête). Le solveur fait toutes les répétitions en mémoire
via le flag `--json-with-queries`.

**Ressources** : utilise les 4 CCDs (`machine.passe_b_taskset_cpus` par
défaut, hérité de la passe B). RLIMIT_AS 50 GiB par process.

**Sortie** : nouvelles lignes dans `structure.csv` avec `runner='dp_query'`
et colonnes `query_*_ms` / `query_*_result` renseignées. Les passes A et B
laissent ces colonnes vides (rétro-compat).

**Plots additionnels** générés par cette passe :

- `figures/breakeven_n.pdf` — courbe de coût DP+queries vs N×Z3 + ECDF des N*
- `figures/query_vs_z3.pdf` — speedup par requête × famille (boxplot)
- `figures/query_per_edge.pdf` — coût empirique µs/arête par requête

---

## Étape G — Récupération des résultats (côté local, après le run)

À exécuter **sur ta machine de dev locale**, dans un terminal hors session SSH :

```bash
cd ~/Documents/GitHub/bachelor/bachelor-thesis
LATEST=$(ssh ebussod@racer "ls -t bachelor/bachelor-thesis/benchmarks/results/" | head -1)
echo "LATEST=$LATEST"
mkdir -p benchmarks/results/$LATEST
rsync -av --progress ebussod@racer:bachelor/bachelor-thesis/benchmarks/results/$LATEST/ \
                     benchmarks/results/$LATEST/
xdg-open benchmarks/results/$LATEST/SUMMARY.md
```

---

## Étape H — Post-run : revisiter les instances désactivées

Une fois le `SUMMARY.md` du run complet relu, si certaines familles sont
sous-représentées, active manuellement les instances qu'on avait désactivées
par sécurité.

Dans `benchmarks/config/instances.yaml`, lignes `enabled: false` :

```yaml
- type3_n50000_t3_s2           # 50k vars, ~3 GiB RAM, ~30 min DP attendu
- type3_n100000_t3_s2          # 100k vars, ~6 GiB RAM, ~1h30 DP attendu
- type3_n5000_t5_s3            # plus gros, psw ~60
- type3_n10000_t5_s3
- tseytin_7x7                  # multiplications 7x7
- tseytin_pure_7x7
- tseytin_10x10
- tseytin_pure_10x10
```

Active ce que tu veux, puis relance `make -C benchmarks bench` — le resume
auto skippera les tâches déjà OK.

---

## Annexe — Dépannage

| Symptôme | Action |
|---|---|
| Z3 timeout sur tout | Augmenter `z3.timeout_s` dans `benchmark.yaml`, ou relancer avec `--skip-z3` |
| OOM sur grosse instance | Augmenter `memory_cap_gib_per_proc` dans `benchmark.yaml` |
| Run interrompu (Ctrl-C, SSH coupé) | `python3 benchmarks/orchestrator.py --resume benchmarks/results/$(ls -t benchmarks/results/ \| head -1)` (cf. section "Reprise après interruption" de l'étape E) |
| Plot crash | `make -C benchmarks plot` régénère les plots à partir des CSV |
| Invariants à re-vérifier | `make -C benchmarks verify` |
| Summary à régénérer | `make -C benchmarks summary` |
| Webhook révoqué | Remplacer `DISCORD_WEBHOOK_URL` dans `~/.bashrc`, puis `source ~/.bashrc` |
| Z3 ne s'installe pas | `apt install libgmp-dev` puis `pip install z3-solver` à nouveau |
| `lscpu --extended` montre un mapping différent | Ajuster `passe_b_taskset_cpus` dans `benchmark.yaml` |
