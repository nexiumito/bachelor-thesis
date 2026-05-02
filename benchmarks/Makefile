.PHONY: bench bench-smoke plot verify summary clean help

PYTHON ?= python3
ORCH = $(PYTHON) orchestrator.py

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

plot:
	$(ORCH) --only-plots

verify:
	$(ORCH) --only-invariants

summary:
	$(ORCH) --only-summary

clean:
	@echo "ATTENTION : ne supprime PAS results/. Utiliser a la main si besoin."
	rm -rf __pycache__ */__pycache__ */*/__pycache__

help:
	@echo "Cibles disponibles :"
	@echo "  bench         Lance la chaine complete (passe A + B + plots + summary)"
	@echo "  bench-smoke   Test a blanc sur 3 instances triviales (< 60s)"
	@echo "  plot          Regenere les plots a partir des CSV existants"
	@echo "  verify        Re-verifie les invariants"
	@echo "  summary       Regenere SUMMARY.md"
	@echo "  clean         Nettoie les caches Python (PAS results/)"
