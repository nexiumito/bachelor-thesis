"""Impact du mode de construction de l'arbre (greedy vs linear) sur psw et temps."""

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
    timings = _common.load_csv(input_dir / "timings.csv")
    if structure.empty:
        logger.warning("greedy_vs_linear: structure.csv vide, skip")
        return

    df_struct = structure[(structure["runner"] == "dp") &
                          (structure["status"] == "ok") &
                          (structure["mode"].isin(["greedy", "linear"]))].copy()
    df_struct["ps_width"] = pd.to_numeric(df_struct["ps_width"], errors="coerce")

    pivot_psw = df_struct.pivot_table(index="instance_id", columns="mode",
                                       values="ps_width", aggfunc="first")
    pivot_psw = pivot_psw.dropna(subset=["greedy", "linear"], how="any")

    if timings.empty:
        pivot_time = pd.DataFrame()
    else:
        df_tim = timings[(timings["runner"] == "dp") &
                         (timings["mode"].isin(["greedy", "linear"]))].copy()
        df_tim["time_total_ms_median"] = pd.to_numeric(
            df_tim["time_total_ms_median"], errors="coerce")
        pivot_time = df_tim.pivot_table(index="instance_id", columns="mode",
                                         values="time_total_ms_median", aggfunc="first")
        pivot_time = pivot_time.dropna(subset=["greedy", "linear"], how="any")

    if pivot_psw.empty:
        logger.warning("greedy_vs_linear: pas d'instances en greedy+linear")
        return

    # Tri par n_vars (lu depuis structure).
    n_by_inst = df_struct.dropna(subset=["n_vars"]).groupby("instance_id")["n_vars"].first()
    pivot_psw = pivot_psw.join(n_by_inst.rename("n_vars"), how="left").sort_values("n_vars")
    pivot_psw = pivot_psw[["greedy", "linear"]]

    fig, axes = plt.subplots(1, 2, figsize=(11, 4.2))
    ax_psw, ax_time = axes

    indices = np.arange(len(pivot_psw))
    width = 0.4
    ax_psw.bar(indices - width / 2, pivot_psw["greedy"], width,
               color=_common.color_for("type3"), label="greedy")
    ax_psw.bar(indices + width / 2, pivot_psw["linear"], width,
               color=_common.color_for("random"), label="linear")
    ax_psw.set_xticks(indices)
    ax_psw.set_xticklabels(pivot_psw.index, rotation=45, ha="right")
    ax_psw.set_ylabel("ps-width")
    ax_psw.set_title("(a) ps-width")
    ax_psw.legend()

    if not pivot_time.empty:
        pivot_time = pivot_time.reindex(pivot_psw.index).dropna()
        if not pivot_time.empty:
            indices_t = np.arange(len(pivot_time))
            ax_time.bar(indices_t - width / 2, pivot_time["greedy"], width,
                        color=_common.color_for("type3"), label="greedy")
            ax_time.bar(indices_t + width / 2, pivot_time["linear"], width,
                        color=_common.color_for("random"), label="linear")
            ax_time.set_xticks(indices_t)
            ax_time.set_xticklabels(pivot_time.index, rotation=45, ha="right")
            ax_time.set_yscale("log")
            ax_time.set_ylabel("Temps DP (ms, mediane)")
            ax_time.set_title("(b) Temps total")
            ax_time.legend()
    else:
        ax_time.text(0.5, 0.5, "Pas de timings disponibles",
                     transform=ax_time.transAxes, ha="center", va="center")
        ax_time.set_axis_off()

    fig.suptitle("Impact du mode de construction de l'arbre")
    fig.tight_layout()

    _common.save_plot(
        fig, output_dir, "greedy_vs_linear",
        formats=tuple(config.get("formats", ["pdf", "png"])),
        dpi_png=int(config.get("dpi_png", 200)),
    )
