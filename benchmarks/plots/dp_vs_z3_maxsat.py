"""Comparaison directe DP vs Z3 sur MaxSAT (log-log + diagonale y=x)."""

from __future__ import annotations

import logging
from pathlib import Path
from typing import Any

import matplotlib.pyplot as plt
import numpy as np

from . import _common

logger = logging.getLogger(__name__)


def make(input_dir: Path, output_dir: Path, config: dict[str, Any]) -> None:
    timings = _common.load_csv(input_dir / "timings.csv")
    z3 = _common.load_csv(input_dir / "z3.csv")
    if timings.empty or z3.empty:
        logger.warning("dp_vs_z3_maxsat: donnees insuffisantes, skip")
        return

    dp = timings[(timings["runner"] == "dp") &
                 (timings["mode"] == "greedy") &
                 (timings["seed"] == 0) &
                 (timings["status"].isin(["ok", "partial"]))]
    z3m = z3[(z3["z3_kind"] == "maxsat") & (z3["z3_status"] == "sat")]
    if dp.empty or z3m.empty:
        logger.warning("dp_vs_z3_maxsat: pas de runs sat des deux cotes")
        return

    merged = dp.merge(
        z3m[["instance_id", "z3_solve_ms", "family"]],
        on="instance_id", how="inner", suffixes=("", "_z3"),
    )
    merged = merged.dropna(subset=["time_total_ms_median", "z3_solve_ms"])
    if merged.empty:
        logger.warning("dp_vs_z3_maxsat: aucun point apres jointure")
        return

    fig, ax = plt.subplots()
    for family, sub in merged.groupby("family"):
        ax.scatter(
            sub["time_total_ms_median"], sub["z3_solve_ms"],
            color=_common.color_for(str(family)),
            marker=_common.marker_for(str(family)),
            label=str(family), s=40, edgecolors="black", linewidths=0.4,
        )

    # Diagonale y = x.
    lo = float(min(merged["time_total_ms_median"].min(), merged["z3_solve_ms"].min()))
    hi = float(max(merged["time_total_ms_median"].max(), merged["z3_solve_ms"].max()))
    if lo > 0 and hi > lo:
        xx = np.geomspace(lo, hi, 50)
        ax.plot(xx, xx, "--", color="gray", alpha=0.7, label="y = x")

    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("Temps DP (ms, mediane)")
    ax.set_ylabel("Temps Z3 MaxSAT (ms)")
    ax.set_title("Temps de resolution DP vs Z3 (MaxSAT, log-log)")
    ax.legend(loc="lower right", framealpha=0.9)

    # Annotation : nombre de points au-dessus / en-dessous de la diagonale.
    above = (merged["z3_solve_ms"] > merged["time_total_ms_median"]).sum()
    below = (merged["z3_solve_ms"] < merged["time_total_ms_median"]).sum()
    ax.text(0.02, 0.98,
            f"DP plus rapide : {above}\nZ3 plus rapide : {below}",
            transform=ax.transAxes, va="top", ha="left",
            bbox=dict(facecolor="white", alpha=0.7, edgecolor="gray"))

    _common.save_plot(
        fig, output_dir, "dp_vs_z3_maxsat",
        formats=tuple(config.get("formats", ["pdf", "png"])),
        dpi_png=int(config.get("dpi_png", 200)),
    )
