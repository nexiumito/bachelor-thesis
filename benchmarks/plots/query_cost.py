"""Cout de la requete count sur le DAG d-DNNF en fonction de sa taille |D|.

Version courante : on utilise time_phase3_ms comme proxy haut (la phase 3
inclut la DP plus la verification dnnf_count). Une version ulterieure du
flag --json pourra ajouter un timer dedie pour les requetes individuelles
(count / consistency / validity / find_model).
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


def make(input_dir: Path, output_dir: Path, config: dict[str, Any]) -> None:
    structure = _common.load_csv(input_dir / "structure.csv")
    if structure.empty:
        logger.warning("query_cost: structure.csv vide, skip")
        return

    df = structure[(structure["runner"] == "dp") &
                   (structure["status"] == "ok") &
                   (structure["mode"] == "greedy")].copy()
    df["dnnf_nodes"] = pd.to_numeric(df["dnnf_nodes"], errors="coerce")
    # On utilise time_phase3_ms comme proxy : la requete count fait partie de
    # la phase 3 (construction + verification interne dnnf_count_match). C'est
    # un proxy haut, car la phase 3 inclut aussi la DP. v2 ajoutera un timer
    # dedie pour count seul.
    df["time_phase3_ms"] = pd.to_numeric(df["time_phase3_ms"], errors="coerce")
    df = df.dropna(subset=["dnnf_nodes", "time_phase3_ms"])
    df = df[df["dnnf_nodes"] > 0]
    if df.empty:
        logger.warning("query_cost: pas de donnees")
        return

    fig, ax = plt.subplots()
    for family, sub in df.groupby("family"):
        ax.scatter(
            sub["dnnf_nodes"], sub["time_phase3_ms"],
            color=_common.color_for(str(family)),
            marker=_common.marker_for(str(family)),
            label=str(family), s=40, edgecolors="black", linewidths=0.4,
        )

    # Reference y = c * x (lineaire).
    if len(df) >= 5:
        try:
            x = df["dnnf_nodes"].values.astype(float)
            y = df["time_phase3_ms"].values.astype(float)
            coef = np.median(y / np.maximum(x, 1.0))
            xx = np.geomspace(max(1.0, df["dnnf_nodes"].min()),
                              df["dnnf_nodes"].max(), 50)
            ax.plot(xx, coef * xx, "--", color="gray", alpha=0.6,
                    label=f"y = {coef:.3e} * x")
        except Exception as e:
            logger.debug("query_cost reference line: %s", e)

    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("|D| (taille DAG d-DNNF)")
    ax.set_ylabel("Temps phase 3 (ms, proxy count)")
    ax.set_title("Cout de la requete count sur le DAG d-DNNF (v1, proxy phase 3)")
    ax.legend(loc="best", framealpha=0.9)

    _common.save_plot(
        fig, output_dir, "query_cost",
        formats=tuple(config.get("formats", ["pdf", "png"])),
        dpi_png=int(config.get("dpi_png", 200)),
    )
