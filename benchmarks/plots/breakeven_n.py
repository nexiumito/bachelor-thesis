"""Plot break-even N : seuil ou DP+queries devient rentable face a Z3.

Modele de cout pour une instance donnee :
  - DP : time_total_ms (passe B greedy, mediane) + N * t_query
  - Z3 : N * z3_solve_ms

Intersection :
  N* = ceil(time_total_ms / (z3_solve_ms - t_query)) si z3_solve_ms > t_query
       infini sinon (DP jamais rentable)

Choix par defaut : t_query = query_co_ms (la plus rapide, et la plus
representative d'un usage SAT decision repete).

Sortie : 2 sous-figures cote a cote dans figures/breakeven_n.{pdf,png}.
  - Sous-figure 1 : courbes de cout pour 1 instance par famille (mediane
    representative). Permet de visualiser ou les deux courbes se croisent.
  - Sous-figure 2 : ECDF des N* sur toutes les instances. Stats annotees.
"""

from __future__ import annotations

import logging
import math
from pathlib import Path
from typing import Any

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

from . import _common

logger = logging.getLogger(__name__)


def make(input_dir: Path, output_dir: Path, config: dict[str, Any]) -> None:
    structure = _common.load_csv(input_dir / "structure.csv")
    z3 = _common.load_csv(input_dir / "z3.csv")
    if structure.empty or z3.empty:
        logger.warning("breakeven_n: structure.csv ou z3.csv vide, skip")
        return

    # Lignes de la passe C (runner=='dp_query', mode=='greedy', status=='ok').
    dp_q = structure[(structure["runner"] == "dp_query") &
                     (structure["status"] == "ok") &
                     (structure["mode"] == "greedy")].copy()
    if dp_q.empty:
        logger.warning("breakeven_n: aucune ligne dp_query OK, skip "
                       "(passe C non lancee ?)")
        return

    # Mediane DP : on prefere timings.csv (passe B) si dispo, sinon fallback
    # sur structure.csv (passe A) pour ne pas bloquer.
    timings = _common.load_csv(input_dir / "timings.csv")
    if not timings.empty:
        dp_b = timings[(timings["runner"] == "dp") &
                       (timings["mode"] == "greedy")][
                       ["instance_id", "time_total_ms_median"]].copy()
        dp_b = dp_b.rename(columns={"time_total_ms_median": "dp_compile_ms"})
    else:
        dp_a = structure[(structure["runner"] == "dp") &
                         (structure["status"] == "ok") &
                         (structure["mode"] == "greedy")].copy()
        dp_b = dp_a[["instance_id", "time_total_ms"]].copy()
        dp_b = dp_b.rename(columns={"time_total_ms": "dp_compile_ms"})

    z3_sat = z3[z3["z3_kind"] == "sat"][["instance_id", "z3_solve_ms"]]

    df = dp_q.merge(dp_b, on="instance_id", how="left") \
              .merge(z3_sat, on="instance_id", how="left")

    df["t_query"]    = pd.to_numeric(df["query_co_ms"], errors="coerce")
    df["dp_compile"] = pd.to_numeric(df["dp_compile_ms"], errors="coerce")
    df["z3_solve"]   = pd.to_numeric(df["z3_solve_ms"], errors="coerce")
    df = df.dropna(subset=["t_query", "dp_compile", "z3_solve"])
    if df.empty:
        logger.warning("breakeven_n: pas de donnees apres jointures, skip")
        return

    # N* = ceil(dp_compile / (z3_solve - t_query)) si z3_solve > t_query, sinon infini.
    diff = df["z3_solve"] - df["t_query"]
    df["n_star"] = np.where(
        diff > 0,
        np.ceil(df["dp_compile"] / np.maximum(diff, 1e-12)),
        np.inf,
    )

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(11, 4.5))

    # ---- Sous-figure 1 : courbes pour 1 instance representative par famille ----
    families_present = [f for f in ["type1", "type2", "type3", "random",
                                    "tseytin", "factorization"]
                        if f in df["family"].unique()]
    N = np.geomspace(1, 1000, 80)
    for fam in families_present:
        sub = df[df["family"] == fam].copy()
        if sub.empty:
            continue
        # Mediane representative : on prend l'instance dont le N* est le plus
        # proche de la mediane des N* finis, sinon la mediane de t_query.
        sub_sorted = sub.sort_values("t_query").reset_index(drop=True)
        med_row = sub_sorted.iloc[len(sub_sorted) // 2]
        cost_dp = med_row["dp_compile"] + N * med_row["t_query"]
        cost_z3 = N * med_row["z3_solve"]
        color = _common.color_for(fam)
        ax1.plot(N, cost_dp, "-", color=color, alpha=0.9,
                 label=f"DP+queries [{fam}]", linewidth=1.5)
        ax1.plot(N, cost_z3, "--", color=color, alpha=0.5, linewidth=1.0)
        if med_row["n_star"] != np.inf and med_row["n_star"] >= 1:
            ax1.axvline(med_row["n_star"], color=color,
                        alpha=0.25, linestyle=":")

    ax1.set_xscale("log")
    ax1.set_yscale("log")
    ax1.set_xlabel("N (nombre de requetes)")
    ax1.set_ylabel("Cout total (ms)")
    ax1.set_title("Courbes de cout : DP+queries vs N x Z3\n"
                  "(1 instance representative par famille)")
    ax1.legend(loc="upper left", fontsize=8, framealpha=0.85)

    # ---- Sous-figure 2 : ECDF des N* finis + stats ----
    finite = df[df["n_star"] != np.inf]["n_star"].sort_values().values
    inf_count = (df["n_star"] == np.inf).sum()
    if len(finite) > 0:
        y_ecdf = np.arange(1, len(finite) + 1) / len(df)
        ax2.step(finite, y_ecdf, where="post", linewidth=1.5,
                 color="#333333", label=f"N* fini ({len(finite)}/{len(df)})")
        ax2.set_xscale("log")
        median_nstar = float(np.median(finite))
        q1 = float(np.quantile(finite, 0.25))
        q3 = float(np.quantile(finite, 0.75))
        ax2.axvline(median_nstar, color="red", linestyle="--",
                    alpha=0.7, label=f"mediane = {median_nstar:.0f}")
        title = (f"Distribution des N*\n"
                 f"mediane={median_nstar:.0f}  Q1={q1:.0f}  Q3={q3:.0f}  "
                 f"infinis={inf_count}/{len(df)}")
    else:
        title = f"Tous les N* sont infinis ({inf_count}/{len(df)})"
    ax2.set_xlabel("N*")
    ax2.set_ylabel("Proportion d'instances avec N break-even <= x")
    ax2.set_title(title)
    ax2.legend(loc="lower right", fontsize=9)

    fig.tight_layout()
    _common.save_plot(
        fig, output_dir, "breakeven_n",
        formats=tuple(config.get("formats", ["pdf", "png"])),
        dpi_png=int(config.get("dpi_png", 200)),
    )
