import matplotlib.pyplot as plt
import numpy  as np
import os

def plot_figure(plots: list[dict], save_path: str, layout: tuple = (1, 1), title: str = ""):
    """
    Crée une figure avec plusieurs sous-graphes.

    Args:
        plots (list[dict]): Liste des graphes à tracer. Chaque dict doit contenir :
            - type: 'line', 'box', 'bar'
            - title: titre du sous-graphique
            - data: {
                'labels': [x1, x2, ...],
                'series': { "Nom courbe": [y1, y2, ...], ... }
              }
            - xlabel (str)
            - ylabel (str)
            - legend (bool)
        save_path (str): Chemin de sauvegarde de la figure
        layout (tuple): (n_lignes, n_colonnes)
        title (str): Titre principal de la figure
    """

    n_rows, n_cols = layout
    fig, axes = plt.subplots(n_rows, n_cols, figsize=(6 * n_cols, 4.5 * n_rows))
    axes = np.array(axes).reshape(-1) if isinstance(axes, (np.ndarray, list)) else [axes]

    for idx, plot in enumerate(plots):
        if idx >= len(axes):
            break

        ax = axes[idx]
        ptype = plot.get("type", "line")
        pdata = plot.get("data", {})
        labels = pdata.get("labels", [])
        series = pdata.get("series", {})

        if ptype == "line":
            for label, values in series.items():
                ax.plot(labels, values, marker='o', label=label)
        elif ptype == "box" and len(series) == 1:
            data = list(series.values())[0]
            ax.boxplot(data, positions=labels, widths=0.6, patch_artist=True)
        elif ptype == "bar":
            width = 0.35 / max(1, len(series))
            x = np.arange(len(labels))
            for i, (label, values) in enumerate(series.items()):
                offset = (i - len(series)/2) * width
                ax.bar(x + offset, values, width=width, label=label)
            ax.set_xticks(x)
            ax.set_xticklabels(labels)

        ax.set_title(plot.get("title", ""))
        ax.set_xlabel(plot.get("xlabel", ""))
        ax.set_ylabel(plot.get("ylabel", ""))
        if plot.get("legend", True) and ax.get_legend_handles_labels()[0]:
            ax.legend()
        ax.grid(True)

    fig.suptitle(title)
    plt.tight_layout(rect=[0, 0, 1, 0.95])

    plt.savefig(save_path)
    plt.close()
    print(f"✅ Graphe sauvegardé dans {save_path}")
