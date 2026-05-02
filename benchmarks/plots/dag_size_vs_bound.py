"""Taille du DAG d-DNNF rapportee a la borne theorique 7*k^3*(n+m).

Borne issue du Lemme 7 du papier Bova/Capelli/Mengel/Slivovsky 2016.
Ratio observe attendu << 1.
"""

from __future__ import annotations

import logging
from pathlib import Path
from typing import Any

import matplotlib.pyplot as plt

from . import _common

logger = logging.getLogger(__name__)


def make(input_dir: Path, output_dir: Path, config: dict[str, Any]) -> None:
    structure = _common.load_csv(input_dir / "structure.csv")
    if structure.empty:
        logger.warning("dag_size_vs_bound: structure.csv vide, skip")
        return

    df = structure[(structure["runner"] == "dp") &
                   (structure["status"] == "ok") &
                   (structure["mode"] == "greedy")].copy()
    df = df.dropna(subset=["dnnf_nodes", "dnnf_bound_7k3nm", "ps_width"])
    # Conversion : "overflow" -> NaN.
    for col in ("dnnf_nodes", "dnnf_bound_7k3nm", "ps_width"):
        import pandas as pd
        df[col] = pd.to_numeric(df[col], errors="coerce")
    df = df.dropna(subset=["dnnf_nodes", "dnnf_bound_7k3nm", "ps_width"])
    df = df[df["dnnf_bound_7k3nm"] > 0]
    if df.empty:
        logger.warning("dag_size_vs_bound: aucun point exploitable")
        return

    df["ratio"] = df["dnnf_nodes"] / df["dnnf_bound_7k3nm"]

    fig, ax = plt.subplots()
    for family, sub in df.groupby("family"):
        ax.scatter(
            sub["ps_width"], sub["ratio"],
            color=_common.color_for(str(family)),
            marker=_common.marker_for(str(family)),
            label=str(family), s=40, edgecolors="black", linewidths=0.4,
        )

    ax.axhline(1.0, color="red", linestyle="--", alpha=0.6, label="borne theorique (=1)")
    ax.set_yscale("log")
    ax.set_xlabel("ps-width")
    ax.set_ylabel(r"$|D| \,/\, 7 \cdot \mathrm{psw}^3 \cdot (n+m)$")
    ax.set_title("Taille du DAG d-DNNF rapportee a la borne theorique")
    ax.legend(loc="best", framealpha=0.9)

    _common.save_plot(
        fig, output_dir, "dag_size_vs_bound",
        formats=tuple(config.get("formats", ["pdf", "png"])),
        dpi_png=int(config.get("dpi_png", 200)),
    )
