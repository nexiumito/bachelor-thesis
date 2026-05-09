"""Profil temporel par phase (top 20 instances les plus lentes, mode greedy).

Barres empilees horizontales : Phase 0 (construction arbre) + Procedure 1 +
Procedure 2 + Procedure 3 (DP + construction du DAG d-DNNF).
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
    timings = _common.load_csv(input_dir / "timings.csv")
    if timings.empty:
        logger.warning("phase_breakdown: timings.csv vide, skip")
        return

    df = timings[(timings["runner"] == "dp") &
                 (timings["mode"] == "greedy") &
                 (timings["status"].isin(["ok", "partial"]))].copy()
    cols = ["time_phase0_ms_median", "time_phase1_ms_median",
            "time_phase2_ms_median", "time_phase3_ms_median",
            "time_total_ms_median"]
    for c in cols:
        df[c] = pd.to_numeric(df[c], errors="coerce")
    df = df.dropna(subset=cols)
    if df.empty:
        logger.warning("phase_breakdown: pas de donnees mode greedy")
        return

    df = df.sort_values("time_total_ms_median", ascending=False).head(20)
    df = df.iloc[::-1]  # top en haut sur graph horizontal

    labels = df["instance_id"].tolist()
    p0 = df["time_phase0_ms_median"].values
    p1 = df["time_phase1_ms_median"].values
    p2 = df["time_phase2_ms_median"].values
    p3 = df["time_phase3_ms_median"].values

    indices = np.arange(len(labels))
    height = 0.7

    fig, ax = plt.subplots(figsize=(8.0, max(4.0, 0.3 * len(labels))))
    ax.barh(indices, p0, height, label="Construction arbre (P0)",
            color="#a6cee3")
    ax.barh(indices, p1, height, left=p0, label="Procedure 1",
            color="#1f78b4")
    ax.barh(indices, p2, height, left=p0 + p1, label="Procedure 2",
            color="#b2df8a")
    ax.barh(indices, p3, height, left=p0 + p1 + p2,
            label="Procedure 3 (DP+DAG)", color="#33a02c")
    ax.set_yticks(indices)
    ax.set_yticklabels(labels)
    ax.set_xscale("log")
    ax.set_xlabel("Temps (ms, mediane)")
    ax.set_title("Profil temporel par phase (top 20, mode greedy)")
    ax.legend(loc="lower right", framealpha=0.9)
    fig.tight_layout()

    _common.save_plot(
        fig, output_dir, "phase_breakdown",
        formats=tuple(config.get("formats", ["pdf", "png"])),
        dpi_png=int(config.get("dpi_png", 200)),
    )
