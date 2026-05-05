"""Plot query-vs-Z3 : speedup d'une requete sur DAG compile face a Z3 froid.

Pour chaque requete CO/VA/CT/CE/IM/ME (1 modele), on calcule le ratio
``z3_solve_ms / query_X_ms`` sur toutes les instances OK de la passe C.

Affichage : boxplot horizontal d'une ligne par requete, points individuels
en strip avec couleur par famille, ligne verticale rouge a x=1 (egalite).

Au-dessus de x=1 -> requete plus rapide que Z3 (le DP gagne).
En-dessous       -> Z3 reste plus rapide meme contre une requete sur DAG.

Sortie : figures/query_vs_z3.{pdf,png}.
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


# Cle JSON -> libelle court pour l'axe.
QUERIES = [
    ("query_co_ms",         "CO"),
    ("query_va_ms",         "VA"),
    ("query_ct_ms",         "CT"),
    ("query_me_ms",         "ME-1"),
    ("query_ce_ms",         "CE"),
    ("query_im_ms",         "IM"),
    ("query_enum_first_ms", "ME-multi (1er)"),
]


def make(input_dir: Path, output_dir: Path, config: dict[str, Any]) -> None:
    structure = _common.load_csv(input_dir / "structure.csv")
    z3 = _common.load_csv(input_dir / "z3.csv")
    if structure.empty or z3.empty:
        logger.warning("query_vs_z3: structure.csv ou z3.csv vide, skip")
        return

    dp_q = structure[(structure["runner"] == "dp_query") &
                     (structure["status"] == "ok")].copy()
    if dp_q.empty:
        logger.warning("query_vs_z3: aucune ligne dp_query OK, skip")
        return

    z3_sat = z3[z3["z3_kind"] == "sat"][["instance_id", "z3_solve_ms"]]
    df = dp_q.merge(z3_sat, on="instance_id", how="inner")
    df["z3_solve"] = pd.to_numeric(df["z3_solve_ms"], errors="coerce")
    df = df.dropna(subset=["z3_solve"])
    df = df[df["z3_solve"] > 0]
    # Filtre instances UNSAT (dnnf_edges == 0) : sur ces instances les
    # requetes retournent en ~30 ns et les ratios z3_solve/t_q deviennent
    # artificiellement geants (10^4 - 10^5), polluant le boxplot IM/CE.
    df["dnnf_edges"] = pd.to_numeric(df.get("dnnf_edges"), errors="coerce")
    df = df[df["dnnf_edges"] > 0]
    if df.empty:
        logger.warning("query_vs_z3: pas de donnees apres filtrage, skip")
        return

    # Ratio par requete + tracking de la famille (pour les couleurs des points).
    rows = []
    for col, label in QUERIES:
        if col not in df.columns:
            continue
        sub = df.copy()
        sub["t_q"] = pd.to_numeric(sub[col], errors="coerce")
        sub = sub.dropna(subset=["t_q"])
        sub = sub[sub["t_q"] > 0]
        if sub.empty:
            continue
        for _, r in sub.iterrows():
            rows.append({
                "query":  label,
                "family": str(r.get("family", "misc")),
                "ratio":  float(r["z3_solve"] / r["t_q"]),
            })
    plot_df = pd.DataFrame(rows)
    if plot_df.empty:
        logger.warning("query_vs_z3: aucun ratio calculable, skip")
        return

    # Ordre des requetes (du plus rapide attendu au plus lent).
    query_order = [lbl for _, lbl in QUERIES if lbl in plot_df["query"].unique()]

    fig, ax = plt.subplots(figsize=(8.5, max(4.0, 0.6 * len(query_order) + 1.5)))
    box_data = [plot_df[plot_df["query"] == q]["ratio"].values for q in query_order]

    bp = ax.boxplot(
        box_data,
        vert=False,
        labels=query_order,
        showfliers=False,
        widths=0.6,
        patch_artist=True,
        medianprops={"color": "black", "linewidth": 1.5},
        boxprops={"facecolor": "#E0E0E0", "edgecolor": "#444444"},
    )

    # Strip plot : un point par instance, colore selon la famille.
    rng = np.random.default_rng(42)
    for i, q in enumerate(query_order):
        sub = plot_df[plot_df["query"] == q]
        y_jitter = rng.uniform(-0.15, 0.15, size=len(sub))
        for fam in sub["family"].unique():
            fam_sub = sub[sub["family"] == fam]
            ax.scatter(
                fam_sub["ratio"].values,
                np.full(len(fam_sub), i + 1) + y_jitter[:len(fam_sub)],
                color=_common.color_for(fam),
                alpha=0.55,
                s=18,
                edgecolors="black",
                linewidths=0.3,
                label=fam if i == 0 else None,
            )

    ax.axvline(1.0, color="red", linestyle="--", alpha=0.7,
               label="Z3 = query (egalite)")
    ax.set_xscale("log")
    ax.set_xlabel("Ratio z3_solve_ms / query_ms (>1 -> requete plus rapide que Z3)")
    ax.set_title("Speedup par requete sur DAG compile vs Z3 froid")

    # Annotation : % d'instances ou DP gagne par requete.
    for i, q in enumerate(query_order):
        sub = plot_df[plot_df["query"] == q]["ratio"].values
        pct_win = float(np.mean(sub > 1.0)) * 100.0
        ax.text(ax.get_xlim()[1], i + 1, f"  DP > Z3 : {pct_win:.0f}%",
                va="center", fontsize=8, color="#333333")

    # Legende compacte (familles + ligne d'egalite). On dedoublonne.
    handles, labels = ax.get_legend_handles_labels()
    seen = set()
    pruned = []
    for h, l in zip(handles, labels):
        if l in seen:
            continue
        seen.add(l)
        pruned.append((h, l))
    if pruned:
        ax.legend([h for h, _ in pruned], [l for _, l in pruned],
                  loc="lower right", fontsize=8, framealpha=0.85)

    fig.tight_layout()
    _common.save_plot(
        fig, output_dir, "query_vs_z3",
        formats=tuple(config.get("formats", ["pdf", "png"])),
        dpi_png=int(config.get("dpi_png", 200)),
    )
