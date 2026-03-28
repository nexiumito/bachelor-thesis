import os

# Chemin absolu vers la racine du projet, peu importe où le script est exécuté
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../"))

# Dossiers relatifs à la racine du projet
GRAPH_DIR = os.path.join(PROJECT_ROOT, "outputs", "graphs")
DIMACS_DIR = os.path.join(PROJECT_ROOT, "data", "raw", "dimacs")
CSV_DIR = os.path.join(PROJECT_ROOT, "data", "raw", "csv")
LOGS_DIR = os.path.join(PROJECT_ROOT, "outputs", "logs")
