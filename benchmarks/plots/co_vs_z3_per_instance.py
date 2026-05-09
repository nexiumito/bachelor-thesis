"""Comparaison rigoureuse CO vs Z3 SAT decision sur la meme formule.

C'est la seule comparaison fair-and-square du bench : meme question
(F est-elle SAT ?) resolue par deux algorithmes distincts.
  - CO  : dnnf_consistency() sur DAG deja compile (in-process,
          mediane sur 5 repetitions, depuis structure.csv runner=dp_query)
  - Z3  : Solver.check() sur F entiere (depuis z3.csv z3_kind=sat)

Selection des instances : pour chaque famille, on prend l'instance mediane
sur dnnf_edges (proxy de taille du DAG, qui reflete la difficulte vue par
le DP). 5-7 instances representatives au total.

Sortie : figures/co_vs_z3_per_instance.{pdf,png}.
"""

from __future__ import annotations

import logging
from pathlib import Path
from typing import Any

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

from . import _common

logger = logging.getLogger(__name__)


_FAMILY_ORDER = ["type1", "type2", "type3", "random",
                 "tseytin", "factorization", "misc"]


def make(input_dir: Path, output_dir: Path, config: dict[str, Any]) -> None:
    structure = _common.load_csv(input_dir / "structure.csv")
    z3 = _common.load_csv(input_dir / "z3.csv")
    if structure.empty or z3.empty:
        logger.warning("co_vs_z3_per_instance: structure.csv ou z3.csv vide, skip")
        return

    dp_q = structure[(structure["runner"] == "dp_query") &
                     (structure["status"] == "ok")].copy()
    if dp_q.empty:
        logger.warning("co_vs_z3_per_instance: aucune ligne dp_query OK, skip "
                       "(passe C non lancee ?)")
        return

    # Filtre UNSAT (DAG reduit a FALSE) : sur ces instances CO retourne
    # immediatement (~30 ns) sans rien faire, ce qui donnerait un speedup
    # artificiel.
    dp_q["dnnf_edges"] = pd.to_numeric(dp_q["dnnf_edges"], errors="coerce")
    dp_q = dp_q[dp_q["dnnf_edges"] > 0]
    dp_q["query_co_ms"] = pd.to_numeric(dp_q["query_co_ms"], errors="coerce")
    dp_q = dp_q.dropna(subset=["query_co_ms"])
    dp_q = dp_q[dp_q["query_co_ms"] > 0]

    z3_sat = z3[(z3["z3_kind"] == "sat") &
                (z3["z3_status"].isin(["sat", "unsat"]))][
                ["instance_id", "z3_solve_ms"]].copy()
    z3_sat["z3_solve_ms"] = pd.to_numeric(z3_sat["z3_solve_ms"], errors="coerce")
    z3_sat = z3_sat.dropna(subset=["z3_solve_ms"])
    z3_sat = z3_sat[z3_sat["z3_solve_ms"] > 0]

    df = dp_q.merge(z3_sat, on="instance_id", how="inner")
    if df.empty:
        logger.warning("co_vs_z3_per_instance: pas d'intersection dp_query / z3, skip")
        return

    # Selection : 1 instance mediane sur dnnf_edges par famille presente.
    selected: list[dict[str, Any]] = []
    for fam in _FAMILY_ORDER:
        fam_df = df[df["family"] == fam].sort_values("dnnf_edges").reset_index(drop=True)
        if fam_df.empty:
            continue
        med_row = fam_df.iloc[len(fam_df) // 2]
        selected.append({
            "family":      fam,
            "instance_id": str(med_row["instance_id"]),
            "co_ms":       float(med_row["query_co_ms"]),
            "z3_ms":       float(med_row["z3_solve_ms"]),
            "dnnf_edges":  int(med_row["dnnf_edges"]),
        })

    if len(selected) < 2:
        logger.warning("co_vs_z3_per_instance: <2 familles eligibles, skip")
        return

    # Conversion en us pour l'axe (les valeurs ms sont souvent < 1).
    labels   = [f"{r['instance_id']}\n[{r['family']}]" for r in selected]
    co_us    = [r["co_ms"] * 1000.0 for r in selected]
    z3_us    = [r["z3_ms"] * 1000.0 for r in selected]
    speedups = [r["z3_ms"] / r["co_ms"] if r["co_ms"] > 0 else 0.0
                for r in selected]

    n = len(selected)
    y = np.arange(n)
    bar_height = 0.38

    fig, ax = plt.subplots(figsize=(9.5, max(4.0, 0.85 * n + 1.6)))
    co_color = "#4C72B0"
    z3_color = "#C44E52"

    ax.barh(y - bar_height / 2, co_us, height=bar_height,
            color=co_color, edgecolor="black", linewidth=0.4,
            label="CO (DAG compile)")
    ax.barh(y + bar_height / 2, z3_us, height=bar_height,
            color=z3_color, edgecolor="black", linewidth=0.4,
            label="Z3 SAT decision")

    ax.set_yticks(y)
    ax.set_yticklabels(labels, fontsize=9)
    ax.invert_yaxis()  # premiere instance en haut.
    ax.set_xscale("log")
    ax.set_xlabel("Temps (microsecondes, log)")

    # Annotation : speedup CO sur Z3 a droite de chaque paire.
    x_max = max(max(co_us), max(z3_us))
    for i, sp in enumerate(speedups):
        ax.text(x_max * 1.2, i, f"x{sp:.0f}" if sp >= 1 else f"x{sp:.2f}",
                va="center", fontsize=9, color="#222222")

    ax.legend(loc="lower right", fontsize=9, framealpha=0.85)
    ax.grid(True, axis="x", alpha=0.3)

    # Titre principal + note methodologique sous-titre.
    fig.suptitle("CO sur DAG compile vs Z3 SAT decision (meme formule)",
                 fontsize=12, fontweight="bold", y=0.985)
    fig.text(
        0.5, 0.93,
        "CO est la seule requete comparable a Z3 SAT decision (meme question logique).\n"
        "Les autres requetes (VA/CT/CE/IM/ME) repondent a des questions que Z3\n"
        "ne traite pas nativement et ne sont pas dans ce plot.",
        ha="center", va="top", fontsize=8, color="#444444",
    )
    # Reserve ~12% en haut pour titre + note (3 lignes).
    fig.tight_layout(rect=[0, 0, 1, 0.88])
    _common.save_plot(
        fig, output_dir, "co_vs_z3_per_instance",
        formats=tuple(config.get("formats", ["pdf", "png"])),
        dpi_png=int(config.get("dpi_png", 200)),
    )
