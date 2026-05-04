.PHONY: bench bench-smoke bench-resume bench-query plot verify summary clean help

PYTHON ?= python3
ORCH = $(PYTHON) orchestrator.py

# Dernier dossier de resultats (par mtime). Vide si results/ inexistant ou vide.
LATEST := $(shell ls -t results/ 2>/dev/null | head -1)
# DIR peut etre surcharge en CLI : `make plot DIR=results/20260502_120000`.
DIR ?= results/$(LATEST)

# Cible par defaut : afficher l'aide.
.DEFAULT_GOAL := help

bench:
	@echo "==> Build sat_solver"
	$(MAKE) -C ../src
	@echo "==> Run orchestrator"
	$(ORCH)

bench-smoke:
	@echo "==> Build sat_solver"
	$(MAKE) -C ../src
	@echo "==> Smoke test (3 instances triviales)"
	$(ORCH) --instances type1_v20_c25,type3_n30_t3_s2,random_k3_v8_c20

bench-resume:
	@if [ -z "$(LATEST)" ] || [ ! -d "$(DIR)" ]; then \
	  echo "ERREUR : aucun run a reprendre dans results/ (DIR='$(DIR)')."; \
	  echo "         Lance 'make bench' une premiere fois."; \
	  exit 1; \
	fi
	@echo "==> Build sat_solver"
	$(MAKE) -C ../src
	@echo "==> Reprise du run $(DIR)"
	$(ORCH) --resume "$(DIR)"

bench-query:
	@if [ -z "$(LATEST)" ] || [ ! -d "$(DIR)" ]; then \
	  echo "ERREUR : aucun run dans results/ (DIR='$(DIR)'). Lance 'make bench' d'abord."; \
	  exit 1; \
	fi
	@echo "==> Build sat_solver"
	$(MAKE) -C ../src
	@echo "==> Passe C (bench des requetes sur DAG compile) sur $(DIR)"
	$(ORCH) --resume "$(DIR)" --only-passe-c

plot:
	@if [ -z "$(LATEST)" ] || [ ! -d "$(DIR)" ]; then \
	  echo "ERREUR : aucun run dans results/ (DIR='$(DIR)'). Lance 'make bench' d'abord."; \
	  exit 1; \
	fi
	@echo "==> Plots sur $(DIR)"
	$(ORCH) --resume "$(DIR)" --only-plots

verify:
	@if [ -z "$(LATEST)" ] || [ ! -d "$(DIR)" ]; then \
	  echo "ERREUR : aucun run dans results/ (DIR='$(DIR)'). Lance 'make bench' d'abord."; \
	  exit 1; \
	fi
	@echo "==> Invariants sur $(DIR)"
	$(ORCH) --resume "$(DIR)" --only-invariants

summary:
	@if [ -z "$(LATEST)" ] || [ ! -d "$(DIR)" ]; then \
	  echo "ERREUR : aucun run dans results/ (DIR='$(DIR)'). Lance 'make bench' d'abord."; \
	  exit 1; \
	fi
	@echo "==> Summary sur $(DIR)"
	$(ORCH) --resume "$(DIR)" --only-summary

clean:
	@echo "ATTENTION : ne supprime PAS results/. Utiliser a la main si besoin."
	rm -rf __pycache__ */__pycache__ */*/__pycache__

help:
	@echo "Cibles disponibles :"
	@echo "  bench         Lance la chaine complete (passes A + B + C + plots + summary)"
	@echo "  bench-smoke   Test a blanc sur 3 instances triviales (< 60s)"
	@echo "  bench-resume  Reprend le run le plus recent (utilise DIR=... pour cibler)"
	@echo "  bench-query   Lance uniquement la passe C (bench des requetes sur DAG compile)"
	@echo "  plot          Regenere les plots du dernier run (DIR=... pour cibler)"
	@echo "  verify        Re-verifie les invariants du dernier run (DIR=... pour cibler)"
	@echo "  summary       Regenere SUMMARY.md du dernier run (DIR=... pour cibler)"
	@echo "  clean         Nettoie les caches Python (PAS results/)"
	@echo ""
	@echo "Exemples :"
	@echo "  make bench-resume                          # reprend le dernier run"
	@echo "  make bench-query                           # ne relance que la passe C"
	@echo "  make plot DIR=results/20260502_120000      # regenere les plots d'un run specifique"
