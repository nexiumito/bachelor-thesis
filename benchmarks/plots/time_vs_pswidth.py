"""Temps DP en fonction de la ps-width, par famille (mode greedy).

Met en evidence la croissance ~ k^3 attendue (Theoreme 2 du papier
Saether/Telle/Vatshelle 2015).
"""

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
    structure = _common.load_csv(input_dir / "structure.csv")
    if timings.empty or structure.empty:
        logger.warning("time_vs_pswidth: donnees insuffisantes, skip")
        return

    tim = timings[(timings["runner"] == "dp") &
                  (timings["status"].isin(["ok", "partial"])) &
                  (timings["mode"] == "greedy")].copy()
    struct = structure[(structure["runner"] == "dp") &
                       (structure["status"] == "ok") &
                       (structure["mode"] == "greedy")].copy()

    if tim.empty or struct.empty:
        logger.warning("time_vs_pswidth: pas de donnees mode greedy ok")
        return

    # `family` est deja dans `tim` (orchestrateur) ; le re-inclure cote droit
    # creerait `family_x` / `family_y` et casserait le groupby plus bas.
    merged = tim.merge(
        struct[["instance_id", "mode", "seed", "ps_width"]],
        on=["instance_id", "mode", "seed"], how="left",
    )
    merged = merged.dropna(subset=["ps_width", "time_total_ms_median"])
    if merged.empty:
        logger.warning("time_vs_pswidth: jointure vide")
        return

    fig, ax = plt.subplots()
    for family, sub in merged.groupby("family"):
        ax.scatter(
            sub["ps_width"], sub["time_total_ms_median"],
            color=_common.color_for(str(family)),
            marker=_common.marker_for(str(family)),
            label=str(family), s=40, edgecolors="black", linewidths=0.4,
        )

    ax.set_xscale("log" if merged["ps_width"].max() > 50 else "linear")
    ax.set_yscale("log")
    ax.set_xlabel("ps-width")
    ax.set_ylabel("Temps DP (ms, mediane sur 3 repetitions)")
    ax.set_title("Temps DP en fonction de la ps-width (mode greedy)")
    ax.legend(loc="upper left", framealpha=0.9)

    # Reference y = c * x^3 ajustee en log-log (si assez de points).
    if len(merged) >= 5:
        try:
            x = np.log(merged["ps_width"].astype(float).values)
            y = np.log(merged["time_total_ms_median"].astype(float).values)
            mask = np.isfinite(x) & np.isfinite(y)
            if mask.sum() >= 3:
                slope, intercept = np.polyfit(x[mask], y[mask], 1)
                xx = np.linspace(merged["ps_width"].min(), merged["ps_width"].max(), 100)
                yy = np.exp(intercept) * xx ** 3.0
                ax.plot(xx, yy, "--", color="gray", alpha=0.6,
                        label=f"y=c.x^3 (slope obs.={slope:.2f})")
                ax.legend(loc="upper left", framealpha=0.9)
        except Exception as e:
            logger.debug("regression log-log: %s", e)

    _common.save_plot(
        fig, output_dir, "time_vs_pswidth",
        formats=tuple(config.get("formats", ["pdf", "png"])),
        dpi_png=int(config.get("dpi_png", 200)),
    )
