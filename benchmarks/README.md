# benchmarks/ — harness de bench pour le solveur DP

Mesure rigoureuse des performances du solveur `sat_solver` (DP sur branch
decomposition, papier Saether/Telle/Vatshelle 2015 + extension d-DNNF papier
Bova/Capelli/Mengel/Slivovsky 2016). Comparaison avec Z3, validation
automatique des invariants theoriques, generation des figures pour le rapport.

## Prerequis

- Python >= 3.10
- gcc (pour compiler `sat_solver`)
- Dependances Python : voir `pyproject.toml`

```bash
cd ~/bachelor-thesis
python3 -m venv .venv-bench
source .venv-bench/bin/activate
pip install -e benchmarks
```

## Cibles

```bash
make help            # liste des cibles
make bench-smoke     # test a blanc sur 3 instances triviales
make bench           # run complet (passe A + passe B + plots + summary)
make plot            # regenerer les plots depuis les CSV
make verify          # re-verifier les invariants
make summary         # regenerer SUMMARY.md
```

## Architecture

- `orchestrator.py` : point d'entree, lit `config/`, ecrit `results/<date>/`.
- `runners/run_dp.py` : wrappe `sat_solver --json`, parse, retourne un dict.
- `runners/run_z3.py` : Z3 MaxSAT + Z3 SAT decision + statistiques.
- `runners/invariants.py` : verification des bornes theoriques apres passe A.
- `notifier.py` : notifications Discord (webhook) ou Telegram, fallback no-op.
- `plots/` : 8 plots PDF + helper commun + orchestrateur.
- `config/instances.yaml` : ~95 instances + metadonnees + bornes psw.
- `config/benchmark.yaml` : parallelisme, timeouts, modes, seeds.

## Notifications

Le harness peut envoyer des notifications Discord ou Telegram. Les
credentials sont lus exclusivement dans l'environnement, jamais committes :

- Discord : `DISCORD_WEBHOOK_URL`
- Telegram : `TELEGRAM_TOKEN` + `TELEGRAM_CHAT_ID`

Si les variables ne sont pas definies, le notifier passe en mode no-op
silencieux (le bench tourne normalement).

## Sortie

Chaque run cree `results/<YYYYMMDD_HHMMSS>/` (timestamp UTC) contenant :

- `structure.csv` : passe A, toutes les instances (parallele).
- `timings.csv` : passe B, medianes sur 3 repetitions (mono-coeur isole).
- `z3.csv` : runs Z3 (MaxSAT + SAT decision).
- `invariants.csv` : verifications theoriques (OK / FAIL / SKIPPED).
- `failures.log` : detail des echecs critiques.
- `progress.log` : heartbeat toutes les 5 minutes.
- `env.txt` : uname, lscpu, free, versions.
- `git_info.txt` : hash, branche, statut.
- `figures/*.pdf,*.png` : 8 plots.
- `SUMMARY.md` : rapport human-friendly auto-genere.

