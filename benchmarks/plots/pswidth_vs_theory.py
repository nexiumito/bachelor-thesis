"""Verification empirique des bornes ps-width theoriques par famille.

Pour chaque instance avec une borne ``expected_psw_max`` connue, trace la
psw observee (barre) et la borne theorique (trait noir). Les barres rouges
indiquent un depassement.
"""

from __future__ import annotations

import logging
from pathlib import Path
from typing import Any

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import yaml

from . import _common

logger = logging.getLogger(__name__)


def _load_instance_bounds(repo_root: Path) -> dict[str, dict[str, Any]]:
    """Lit instances.yaml depuis benchmarks/config/ pour les bornes psw."""
    inst_yaml = repo_root / "benchmarks" / "config" / "instances.yaml"
    if not inst_yaml.exists():
        logger.warning("instances.yaml introuvable: %s", inst_yaml)
        return {}
    raw = yaml.safe_load(inst_yaml.read_text())
    out: dict[str, dict[str, Any]] = {}
    for entry in raw.get("instances", []):
        out[entry["id"]] = {
            "expected_psw_max": entry.get("expected_psw_max"),
            "expected_psw_severity": entry.get("expected_psw_severity", "none"),
            "family": entry.get("family"),
        }
    return out


def make(input_dir: Path, output_dir: Path, config: dict[str, Any]) -> None:
    structure = _common.load_csv(input_dir / "structure.csv")
    if structure.empty:
        logger.warning("pswidth_vs_theory: structure.csv vide, skip")
        return

    # Le repo_root est input_dir.parent.parent (results/<date>/.. = benchmarks/.. = repo_root).
    # En pratique on remonte au repo root via le chemin standard.
    repo_root = (input_dir.parent.parent).resolve()
    if not (repo_root / "benchmarks" / "config" / "instances.yaml").exists():
        # Fallback : remonter encore d'un cran (cas results/ direct sous benchmarks/).
        repo_root = (input_dir.parent.parent.parent).resolve()
    bounds = _load_instance_bounds(repo_root)

    df = structure[(structure["runner"] == "dp") &
                   (structure["status"] == "ok") &
                   (structure["mode"] == "greedy")].copy()
    df["ps_width"] = pd.to_numeric(df["ps_width"], errors="coerce")
    df = df.dropna(subset=["ps_width"])
    df["expected_psw_max"] = df["instance_id"].map(
        lambda i: bounds.get(i, {}).get("expected_psw_max"))
    df = df[df["expected_psw_max"].notna()]
    if df.empty:
        logger.warning("pswidth_vs_theory: aucune instance avec borne theorique")
        return

    # P4 : log-y obligatoire pour eviter qu'un outlier (psw~32000 sur type2
    # dans le run 1) ecrase visuellement toutes les autres barres. Filtre
    # defensif sur psw > 0 (log(0) n'est pas defini).
    df = df[df["ps_width"] > 0].copy()
    df = df[df["expected_psw_max"] > 0]
    if df.empty:
        logger.warning("pswidth_vs_theory: aucune instance avec psw > 0")
        return

    df = df.sort_values(["family", "instance_id"])
    indices = np.arange(len(df))

    fig, ax = plt.subplots(figsize=(10, max(4.0, 0.25 * len(df))))
    colors = [_common.color_for(str(fam)) for fam in df["family"]]
    bars = ax.bar(indices, df["ps_width"].values, color=colors,
                   edgecolor="black", linewidth=0.4)

    # Pour chaque barre, trace un trait noir epais a la borne theorique.
    bound_handle = None
    for i, (psw, exp) in enumerate(zip(df["ps_width"].values,
                                        df["expected_psw_max"].values)):
        h = ax.hlines(exp, i - 0.4, i + 0.4, colors="black", linewidth=2)
        if bound_handle is None:
            bound_handle = h
        if psw > exp:
            bars[i].set_edgecolor("red")
            bars[i].set_linewidth(2)

    ax.set_xticks(indices)
    ax.set_xticklabels(df["instance_id"].values, rotation=90)
    ax.set_ylabel("ps-width (log)")
    ax.set_yscale("log")
    ax.set_title("Verification empirique des bornes ps-width par famille (mode greedy)")

    # Legende des familles presentes + entree pour la borne theorique.
    seen: set[str] = set()
    handles = []
    for fam in df["family"]:
        if fam in seen:
            continue
        seen.add(fam)
        handles.append(plt.Rectangle((0, 0), 1, 1,
                                      facecolor=_common.color_for(str(fam)),
                                      edgecolor="black", label=str(fam)))
    # Ajout d'un proxy line pour les hlines (qu'on ne peut pas legender direct).
    handles.append(plt.Line2D([0], [0], color="black", linewidth=2,
                              label="borne theorique (par instance)"))
    if handles:
        ax.legend(handles=handles, loc="upper left", framealpha=0.9)

    fig.tight_layout()

    _common.save_plot(
        fig, output_dir, "pswidth_vs_theory",
        formats=tuple(config.get("formats", ["pdf", "png"])),
        dpi_png=int(config.get("dpi_png", 200)),
    )
