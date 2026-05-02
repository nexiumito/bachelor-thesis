"""Impact du mode de construction de l'arbre (greedy vs linear) sur psw et temps.

P3 : refonte du layout pour le run 2.
- 2 figures separees au lieu de subplots ecrases (greedy_vs_linear_pswidth +
  greedy_vs_linear_time).
- Barres horizontales (labels lisibles a gauche).
- Echelle log sur l'axe valeurs (les ratios linear/greedy peuvent atteindre 10⁵).
- Tri par ratio decroissant pour mettre en avant les pires cas.
- Si plus de 25 instances, ne garde que le top 25 par ratio + indique le total.
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


_TOP_N = 25
_RATIO_COL = "ratio_linear_greedy"


def _build_pivot(structure: pd.DataFrame, value_col: str,
                  source_status_filter: bool = True) -> pd.DataFrame:
    """Construit un pivot instance_id x mode (greedy, linear) sur ``value_col``.

    Garde aussi la colonne `family` du cote greedy pour la coloration.
    """
    df = structure[(structure["runner"] == "dp") &
                   (structure["mode"].isin(["greedy", "linear"]))].copy()
    if source_status_filter:
        df = df[df["status"] == "ok"]
    df[value_col] = pd.to_numeric(df[value_col], errors="coerce")
    pivot = df.pivot_table(index="instance_id", columns="mode",
                            values=value_col, aggfunc="first")
    pivot = pivot.dropna(subset=["greedy", "linear"], how="any")
    # Famille : on prend celle d'une des deux lignes (elles sont egales).
    fam = (df.dropna(subset=["family"])
             .groupby("instance_id")["family"].first())
    pivot = pivot.join(fam.rename("family"), how="left")
    pivot[_RATIO_COL] = pivot["linear"] / pivot["greedy"]
    return pivot


def _make_one_plot(pivot: pd.DataFrame, title: str, xlabel: str,
                    output_dir: Path, name: str,
                    config: dict[str, Any]) -> None:
    """Trace un horizontal grouped bar chart {greedy, linear} pour une metrique."""
    if pivot.empty:
        logger.warning("%s: pivot vide, skip", name)
        return

    # Tri par ratio decroissant pour faire ressortir les pires cas.
    pivot = pivot.sort_values(_RATIO_COL, ascending=False)
    n_total = len(pivot)
    truncated = False
    if n_total > _TOP_N:
        pivot = pivot.head(_TOP_N)
        truncated = True

    n = len(pivot)
    indices = np.arange(n)
    height = 0.4

    fig_height = max(4.0, 0.32 * n + 1.5)
    fig, ax = plt.subplots(figsize=(8.5, fig_height))

    ax.barh(indices - height / 2, pivot["greedy"], height,
            color=_common.color_for("type3"), edgecolor="black",
            linewidth=0.4, label="greedy")
    ax.barh(indices + height / 2, pivot["linear"], height,
            color=_common.color_for("random"), edgecolor="black",
            linewidth=0.4, label="linear")

    ax.set_yticks(indices)
    ax.set_yticklabels(pivot.index, fontsize=8)
    ax.invert_yaxis()  # plus grand ratio en haut
    ax.set_xscale("log")
    ax.set_xlabel(xlabel)

    title_full = title
    if truncated:
        title_full += f"\n(top {_TOP_N} par ratio linear/greedy sur {n_total} instances)"
    ax.set_title(title_full)

    # Annotations : ratio linear/greedy a droite de chaque paire.
    xmax = float(max(pivot["greedy"].max(), pivot["linear"].max()))
    text_x = xmax * 1.05
    for i, (idx, row) in enumerate(pivot.iterrows()):
        ratio = row[_RATIO_COL]
        if pd.notna(ratio) and ratio > 0:
            ax.text(text_x, i, f"×{ratio:.1f}" if ratio < 100
                                  else f"×{ratio:.0f}",
                    va="center", fontsize=7, color="dimgray")

    ax.legend(loc="lower right", framealpha=0.9)
    ax.grid(True, alpha=0.3, axis="x")
    fig.tight_layout()

    _common.save_plot(
        fig, output_dir, name,
        formats=tuple(config.get("formats", ["pdf", "png"])),
        dpi_png=int(config.get("dpi_png", 200)),
    )


def make(input_dir: Path, output_dir: Path, config: dict[str, Any]) -> None:
    structure = _common.load_csv(input_dir / "structure.csv")
    timings = _common.load_csv(input_dir / "timings.csv")
    if structure.empty:
        logger.warning("greedy_vs_linear: structure.csv vide, skip")
        return

    # Plot 1 : ps-width
    pivot_psw = _build_pivot(structure, "ps_width", source_status_filter=True)
    _make_one_plot(
        pivot_psw,
        title="Impact du mode d'arbre sur la ps-width (greedy vs linear)",
        xlabel="ps-width",
        output_dir=output_dir, name="greedy_vs_linear_pswidth",
        config=config,
    )

    # Plot 2 : temps total (depuis timings.csv qui a la mediane sur 3 reps)
    if timings.empty:
        logger.warning("greedy_vs_linear: timings.csv vide, plot temps skip")
        # On genere quand meme l'ancien nom comme alias du psw plot pour ne pas
        # casser les liens dans le SUMMARY (le module make_all_plots loggue
        # quand meme [OK]).
        return
    df_tim = timings[(timings["runner"] == "dp") &
                     (timings["mode"].isin(["greedy", "linear"])) &
                     (timings["status"].isin(["ok", "partial"]))].copy()
    df_tim["time_total_ms_median"] = pd.to_numeric(
        df_tim["time_total_ms_median"], errors="coerce")
    pivot_t = df_tim.pivot_table(index="instance_id", columns="mode",
                                  values="time_total_ms_median",
                                  aggfunc="first")
    pivot_t = pivot_t.dropna(subset=["greedy", "linear"], how="any")
    fam = (df_tim.dropna(subset=["family"])
           .groupby("instance_id")["family"].first())
    pivot_t = pivot_t.join(fam.rename("family"), how="left")
    pivot_t[_RATIO_COL] = pivot_t["linear"] / pivot_t["greedy"]
    _make_one_plot(
        pivot_t,
        title="Impact du mode d'arbre sur le temps total DP (mediane sur 3)",
        xlabel="Temps DP (ms, mediane)",
        output_dir=output_dir, name="greedy_vs_linear_time",
        config=config,
    )

    # Compatibilite ascendante : conserver un fichier `greedy_vs_linear.pdf`
    # qui pointe sur le plot ps-width (plus interessant theoriquement). Les
    # liens existants dans SUMMARY.md continuent a fonctionner.
    _make_one_plot(
        pivot_psw,
        title="Impact du mode d'arbre sur la ps-width (greedy vs linear)",
        xlabel="ps-width",
        output_dir=output_dir, name="greedy_vs_linear",
        config=config,
    )
