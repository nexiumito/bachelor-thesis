import os

def get_experiment_paths(config_path: str) -> dict[str, str]:
    """
    Prend le chemin vers le fichier config.json d'une expérience
    et retourne un dictionnaire avec les chemins vers les sous-dossiers
    de cette expérience : dimacs, csv, logs, graphs.
    """
    base = os.path.dirname(os.path.abspath(config_path))
    return {
        "EXPERIMENT_DIR": base,
        "DIMACS_DIR": os.path.join(base, "dimacs"),
        "CSV_DIR": os.path.join(base, "csv"),
        "GRAPH_DIR": os.path.join(base, "graphs"),
        "LOGS_DIR": os.path.join(base, "logs")
    }
