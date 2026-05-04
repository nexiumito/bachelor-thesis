"""Plot µs/arete par requete : cout empirique normalise par |D|.

Toutes les requetes polytime sur d-DNNF sont en O(|D|), mais leurs
constantes empiriques peuvent differer significativement :
  - CO / VA / CT : 1 passe count = la plus rapide
  - ME-1 (find_model)    : 1 passe count + 1 descent (legerement plus lent)
  - ME-multi (1er modele) : descente CPS (varie selon DAG)
  - CE          : 1 condition + 1 count = ~2x CT
  - IM          : 1 condition + 1 smooth + 1 count = ~3x CT

Calcul : pour chaque (instance, requete), ratio T_query / dnnf_edges
(microsecondes par arete). Box plot horizontal avec mediane + Q1/Q3 +
whiskers, annote du facteur multiplicatif vs CO (reference 1x).

Sortie : figures/query_per_edge.{pdf,png}.
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


QUERIES = [
    ("query_co_ms",         "CO"),
    ("query_ct_ms",         "CT"),
    ("query_va_ms",         "VA"),
    ("query_me_ms",         "ME-1"),
    ("query_enum_first_ms", "ME-multi (1er)"),
    ("query_ce_ms",         "CE"),
    ("query_im_ms",         "IM"),
]


def make(input_dir: Path, output_dir: Path, config: dict[str, Any]) -> None:
    structure = _common.load_csv(input_dir / "structure.csv")
    if structure.empty:
        logger.warning("query_per_edge: structure.csv vide, skip")
        return

    dp_q = structure[(structure["runner"] == "dp_query") &
                     (structure["status"] == "ok")].copy()
    if dp_q.empty:
        logger.warning("query_per_edge: aucune ligne dp_query OK, skip")
        return

    dp_q["dnnf_edges"] = pd.to_numeric(dp_q["dnnf_edges"], errors="coerce")
    dp_q = dp_q[dp_q["dnnf_edges"] > 0]
    if dp_q.empty:
        logger.warning("query_per_edge: dnnf_edges nul partout, skip")
        return

    rows = []
    for col, label in QUERIES:
        if col not in dp_q.columns:
            continue
        sub = dp_q.copy()
        sub["t_q"] = pd.to_numeric(sub[col], errors="coerce")
        sub = sub.dropna(subset=["t_q"])
        sub = sub[sub["t_q"] > 0]
        if sub.empty:
            continue
        # Ratio en microsecondes / arete : t_q est en ms, |D| en aretes.
        ratios = (sub["t_q"].values * 1000.0) / sub["dnnf_edges"].values
        for r in ratios:
            rows.append({"query": label, "us_per_edge": float(r)})
    plot_df = pd.DataFrame(rows)
    if plot_df.empty:
        logger.warning("query_per_edge: aucun ratio calculable, skip")
        return

    # Tri par mediane croissante, sauf qu'on garde CO en tete pour servir de
    # reference (1x) annotee aux autres requetes.
    medians = plot_df.groupby("query")["us_per_edge"].median().to_dict()
    co_med = medians.get("CO", None)

    order = [lbl for _, lbl in QUERIES if lbl in medians]
    if co_med is not None:
        # Place CO en tete, puis les autres tries par mediane.
        rest = sorted([q for q in order if q != "CO"], key=lambda q: medians[q])
        order = ["CO"] + rest

    fig, ax = plt.subplots(figsize=(8.5, max(3.5, 0.6 * len(order) + 1.5)))
    box_data = [plot_df[plot_df["query"] == q]["us_per_edge"].values for q in order]

    ax.boxplot(
        box_data, vert=False, labels=order, showfliers=False, widths=0.6,
        patch_artist=True,
        medianprops={"color": "black", "linewidth": 1.5},
        boxprops={"facecolor": "#E8F0F8", "edgecolor": "#446688"},
    )

    # Annotation : facteur multiplicatif vs CO (reference 1x).
    if co_med is not None and co_med > 0:
        x_max = max(plot_df["us_per_edge"].max(), 1e-9)
        for i, q in enumerate(order):
            med = medians[q]
            factor = med / co_med
            label = "1x (ref.)" if q == "CO" else f"{factor:.1f}x"
            ax.text(x_max * 1.05, i + 1, label, va="center",
                    fontsize=8, color="#333333")

    ax.set_xscale("log")
    ax.set_xlabel("Cout empirique (microsecondes / arete)")
    ax.set_title("Profil de cout par requete sur DAG compile\n"
                 "(toutes en O(|D|), constantes multiplicatives vs CO)")

    fig.tight_layout()
    _common.save_plot(
        fig, output_dir, "query_per_edge",
        formats=tuple(config.get("formats", ["pdf", "png"])),
        dpi_png=int(config.get("dpi_png", 200)),
    )
