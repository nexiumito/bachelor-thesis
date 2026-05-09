"""make_all_plots.py — orchestrateur des plots, robuste aux donnees partielles.

Chaque plot est tente independamment ; un crash ne bloque pas les autres
(try/except global).
"""

from __future__ import annotations

import argparse
import logging
import sys
from pathlib import Path
from typing import Any

import yaml

from . import (
    _common,
    breakeven_n,
    co_vs_z3_per_instance,
    dag_size_vs_bound,
    dp_vs_z3_maxsat,
    greedy_vs_linear,
    phase_breakdown,
    pswidth_vs_theory,
    query_cost,
    query_per_edge,
    query_vs_z3,
    time_vs_pswidth,
    z3_conflicts_vs_pswidth,
)

logger = logging.getLogger(__name__)

PLOT_MODULES = [
    time_vs_pswidth,
    dp_vs_z3_maxsat,
    dag_size_vs_bound,
    greedy_vs_linear,
    z3_conflicts_vs_pswidth,
    phase_breakdown,
    query_cost,
    pswidth_vs_theory,
    # Plots dependants de la passe C (donnees query_*). Skip silencieux si
    # absentes.
    breakeven_n,
    query_vs_z3,
    query_per_edge,
    co_vs_z3_per_instance,
]


def run(input_dir: Path, plots_config: dict[str, Any]) -> None:
    _common.setup_matplotlib(plots_config)
    output_dir = Path(input_dir) / "figures"
    for module in PLOT_MODULES:
        name = module.__name__.split(".")[-1]
        try:
            module.make(input_dir=Path(input_dir), output_dir=output_dir,
                        config=plots_config)
            logger.info("[OK] %s", name)
        except Exception as e:
            logger.exception("[FAIL] %s: %s", name, e)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True, help="results/<date>/")
    ap.add_argument("--config", default="config/benchmark.yaml")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    )

    cfg_path = Path(args.config)
    if not cfg_path.is_absolute():
        cfg_path = Path(__file__).resolve().parent.parent / args.config
    plots_cfg = yaml.safe_load(cfg_path.read_text()).get("plots", {})

    run(input_dir=Path(args.input), plots_config=plots_cfg)
    return 0


if __name__ == "__main__":
    sys.exit(main())
