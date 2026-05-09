"""_common.py — helpers communs aux 8 plots.

setup_matplotlib() applique les rcParams selon la section ``plots`` de
benchmark.yaml. Palette et markers stables entre tous les plots.
"""

from __future__ import annotations

import logging
from pathlib import Path
from typing import Any, Iterable

import matplotlib

matplotlib.use("Agg")  # pas de display, on est en SSH
import matplotlib.pyplot as plt  # noqa: E402

logger = logging.getLogger(__name__)


PALETTE_FAMILY: dict[str, str] = {
    "type1":         "#4C72B0",
    "type2":         "#DD8452",
    "type3":         "#55A467",
    "random":        "#C44E52",
    "tseytin":       "#8172B3",
    "factorization": "#937860",
    "misc":          "#999999",
}

MARKER_FAMILY: dict[str, str] = {
    "type1": "o", "type2": "s", "type3": "D",
    "random": "X", "tseytin": "^", "factorization": "v", "misc": ".",
}


def setup_matplotlib(config: dict[str, Any]) -> None:
    """Applique les rcParams selon la section plots de benchmark.yaml."""
    plt.rcParams["figure.figsize"] = (
        float(config.get("figure_width_in", 6.4)),
        float(config.get("figure_height_in", 4.0)),
    )
    plt.rcParams["font.family"] = config.get("font_family", "serif")
    fs = int(config.get("font_size", 11))
    plt.rcParams["font.size"] = fs
    plt.rcParams["axes.labelsize"] = fs
    plt.rcParams["axes.titlesize"] = fs + 1
    plt.rcParams["legend.fontsize"] = max(8, fs - 1)
    plt.rcParams["xtick.labelsize"] = max(8, fs - 1)
    plt.rcParams["ytick.labelsize"] = max(8, fs - 1)
    plt.rcParams["axes.grid"] = True
    plt.rcParams["grid.alpha"] = 0.3

    style = config.get("matplotlib_style")
    if style:
        try:
            plt.style.use(style)
        except Exception as e:
            logger.warning("Style %s indisponible : %s", style, e)

    if config.get("use_latex", False):
        try:
            plt.rcParams["text.usetex"] = True
            plt.rcParams["text.latex.preamble"] = r"\usepackage{amsmath}"
        except Exception as e:
            logger.warning("LaTeX indisponible : %s", e)


def save_plot(fig: Any, output_dir: Path, name: str,
              formats: Iterable[str] = ("pdf", "png"),
              dpi_png: int = 200) -> list[Path]:
    """Sauvegarde la figure en plusieurs formats. Retourne les chemins crees."""
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    out_paths: list[Path] = []
    for fmt in formats:
        path = output_dir / f"{name}.{fmt}"
        kwargs: dict[str, Any] = {"bbox_inches": "tight"}
        if fmt == "png":
            kwargs["dpi"] = dpi_png
        fig.savefig(path, **kwargs)
        out_paths.append(path)
    plt.close(fig)
    return out_paths


def load_csv(path: Path) -> Any:
    """Wrapper pandas.read_csv avec gestion des champs vides comme NaN."""
    import pandas as pd
    if not path.exists():
        return pd.DataFrame()
    try:
        return pd.read_csv(path)
    except Exception as e:
        logger.warning("Lecture %s impossible: %s", path, e)
        import pandas as pd
        return pd.DataFrame()


def color_for(family: str) -> str:
    return PALETTE_FAMILY.get(family, "#444444")


def marker_for(family: str) -> str:
    return MARKER_FAMILY.get(family, ".")
