"""Correlation entre conflicts Z3 (proxy backtracks) et ps-width DP.

Affiche le coefficient de Spearman dans le titre. Une correlation positive
indique que les instances structurellement difficiles pour la DP sont aussi
difficiles pour un solveur SAT classique.
"""

from __future__ import annotations

import logging
from pathlib import Path
from typing import Any

import matplotlib.pyplot as plt
import pandas as pd

from . import _common

logger = logging.getLogger(__name__)


def make(input_dir: Path, output_dir: Path, config: dict[str, Any]) -> None:
    structure = _common.load_csv(input_dir / "structure.csv")
    z3 = _common.load_csv(input_dir / "z3.csv")
    if structure.empty or z3.empty:
        logger.warning("z3_conflicts_vs_pswidth: donnees insuffisantes, skip")
        return

    df = structure[(structure["runner"] == "dp") &
                   (structure["status"] == "ok") &
                   (structure["mode"] == "greedy")].copy()
    df["ps_width"] = pd.to_numeric(df["ps_width"], errors="coerce")
    df = df.dropna(subset=["ps_width"])

    z3m = z3[z3["z3_kind"].isin(["maxsat", "sat"])].copy()
    z3m["z3_conflicts"] = pd.to_numeric(z3m["z3_conflicts"], errors="coerce")
    z3m = z3m.dropna(subset=["z3_conflicts"])
    # Si plusieurs lignes par instance (maxsat + sat), prendre la max(conflicts).
    z3m = z3m.groupby("instance_id", as_index=False)["z3_conflicts"].max()

    merged = df.merge(z3m, on="instance_id", how="inner")
    if merged.empty:
        logger.warning("z3_conflicts_vs_pswidth: aucune jointure")
        return

    fig, ax = plt.subplots()
    for family, sub in merged.groupby("family"):
        ax.scatter(
            sub["ps_width"], sub["z3_conflicts"],
            color=_common.color_for(str(family)),
            marker=_common.marker_for(str(family)),
            label=str(family), s=40, edgecolors="black", linewidths=0.4,
        )

    if merged["z3_conflicts"].max() / max(merged["z3_conflicts"].min(), 1) > 100:
        ax.set_yscale("log")

    # Coefficient de Spearman.
    rho_str = ""
    if len(merged) >= 5:
        try:
            rho = merged["ps_width"].corr(merged["z3_conflicts"], method="spearman")
            rho_str = f" — rho_Spearman = {rho:.2f}"
        except Exception:
            pass

    ax.set_xlabel("ps-width (DP, mode greedy)")
    ax.set_ylabel("Conflicts Z3 (proxy backtracks)")
    ax.set_title(f"Conflicts Z3 vs ps-width DP{rho_str}")
    ax.legend(loc="best", framealpha=0.9)

    _common.save_plot(
        fig, output_dir, "z3_conflicts_vs_pswidth",
        formats=tuple(config.get("formats", ["pdf", "png"])),
        dpi_png=int(config.get("dpi_png", 200)),
    )
