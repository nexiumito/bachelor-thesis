# Procédure de déploiement et de lancement du benchmark sur racer

## Étape A — Préparation initiale (une seule fois)

```bash
# 1. SSH + clone
ssh ebussod@racer
cd ~
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
cd ~/bachelor-thesis
source .venv-bench/bin/activate

cd src
./sat_solver ../data/exemple1.cnf greedy --json | python3 -m json.tool
# Attendu : un JSON pretty-printé avec dnnf_count_match: true
cd ..
```

### D.2 — Vérification 0 fuite mémoire (ASan)

```bash
cd ~/bachelor-thesis/src
make asan
./sat_solver ../data/exemple1.cnf manual --json 2>&1 | grep -i sanitizer
# Attendu : aucune ligne renvoyée (= 0 fuite)
make rebuild   # rebuild en mode release pour le bench
cd ..
```

### D.3 — Test Z3 stats keys après install

```bash
cd ~/bachelor-thesis
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
cd ~/bachelor-thesis
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

```bash
make -C benchmarks bench-smoke &
BENCH_PID=$!
sleep 5
kill -INT $BENCH_PID
wait $BENCH_PID 2>/dev/null
echo "Exit code attendu : 130"

# Vérifier les CSV partiels
LATEST=$(ls -t benchmarks/results/ | head -1)
wc -l benchmarks/results/$LATEST/structure.csv

# Relancer : doit skipper les tâches déjà faites
make -C benchmarks bench-smoke
# Attendu : logs du type "skip N déjà faites"
```

### D.8 — Vérification visuelle des 8 plots

```bash
LATEST=$(ls -t benchmarks/results/ | head -1)
ls benchmarks/results/$LATEST/figures/

# Régénération des plots seuls (sans relancer le bench)
make -C benchmarks plot

# Récupération côté local pour ouvrir les PDF :
# (depuis ta machine de dev, dans un autre terminal)
LATEST=$(ssh ebussod@racer "ls -t bachelor-thesis/benchmarks/results/" | head -1)
rsync -av ebussod@racer:bachelor-thesis/benchmarks/results/$LATEST/figures/ /tmp/figures/
open /tmp/figures/*.pdf
```

Attendu : 8 PDF + 8 PNG dans `figures/`. Les PDF doivent être vectoriels
(`file figures/*.pdf` mentionne "PDF document").

---

## Étape E — Lancement du benchmark complet

```bash
ssh ebussod@racer
cd ~/bachelor-thesis
git pull                          # toujours pull avant un bench long
source .venv-bench/bin/activate

tmux new -s bench
make -C benchmarks bench

# Détache la session : Ctrl-b puis d
# Le job continue à tourner même si SSH coupe.
```

Tu reçois un message Discord au démarrage avec l'ETA. Puis un toutes les 30 min.
Plus une alerte sur chaque FAIL hard. Plus le récap final.

---

## Étape F — Surveillance pendant le run (depuis n'importe où)

```bash
# Tail du heartbeat
ssh ebussod@racer "tail -f bachelor-thesis/benchmarks/results/\$(ls -t bachelor-thesis/benchmarks/results/ | head -1)/progress.log"

# Voir les CSV en cours
ssh ebussod@racer "wc -l bachelor-thesis/benchmarks/results/\$(ls -t bachelor-thesis/benchmarks/results/ | head -1)/structure.csv"

# Récupérer la session tmux
ssh ebussod@racer
tmux attach -t bench
```

---

## Étape G — Récupération des résultats (côté local, après le run)

```bash
cd ~/UNIGE/Bachelor/L3/S2/bachelor/bachelor-thesis
LATEST=$(ssh ebussod@racer "ls -t bachelor-thesis/benchmarks/results/" | head -1)
rsync -av --progress ebussod@racer:bachelor-thesis/benchmarks/results/$LATEST/ \
                     benchmarks/results/$LATEST/
open benchmarks/results/$LATEST/SUMMARY.md
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
| Run interrompu (Ctrl-C, SSH coupé) | Relancer `make -C benchmarks bench` : reprise auto |
| Plot crash | `make -C benchmarks plot` régénère les plots à partir des CSV |
| Invariants à re-vérifier | `make -C benchmarks verify` |
| Summary à régénérer | `make -C benchmarks summary` |
| Webhook révoqué | Remplacer `DISCORD_WEBHOOK_URL` dans `~/.bashrc`, puis `source ~/.bashrc` |
| Z3 ne s'installe pas | `apt install libgmp-dev` puis `pip install z3-solver` à nouveau |
| `lscpu --extended` montre un mapping différent | Ajuster `passe_b_taskset_cpus` dans `benchmark.yaml` |
