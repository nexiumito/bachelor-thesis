import os
import json
import csv
from analysis.primes import primes_calculator, load_primes
from analysis.SATTools import generate_mult
from utils.experiment_paths_config import get_experiment_paths
from experiments_handlers.time_handler import run_time_analysis
from experiments_handlers.uselessDeletedVar_handler import run_uselessDeletedVar_analysis
from experiments_handlers.randomModifiedVar_handler import run_randomModifiedVar_analysis
from experiments_handlers.complexity_handler import run_complexity_analysis

def run_experiment(config_path):
    """
    Exécute une expérience à partir d’un fichier config.json.
    Gère la génération des nombres premiers, des couples (x, y),
    et lance l’analyse correspondant au type spécifié.
    """
    
    with open(config_path) as f:
        config = json.load(f)

    # Déterminer les chemins relatifs à l'expérience
    paths = get_experiment_paths(config_path)
    
    for path in paths.values():
        os.makedirs(path, exist_ok=True)

    # Générer les nombres premiers nécessaires
    k_max = config["k_values"]
    primes_filename = os.path.join(paths["CSV_DIR"], f"primes_k{k_max}.csv")
    try:
        primes_calculator(primes_filename, k_max)
    except FileExistsError:
        pass
    
    primes_per_bits = load_primes(primes_filename)

    # Générer les pairs (x,y) pour la multiplication
    if config["type"] != "uselessDeletedVar":
        nb_mults = config["nb_mults"]
        pairs_xy = {}
        for size_bits, primes in primes_per_bits.items():
            pairs = generate_mult(primes, nb_mults)
            pairs_xy[size_bits] = pairs
            
        with open(os.path.join(paths["CSV_DIR"], "used_pairs.csv"), "w") as f:
            fieldnames = ['size_bits', '(x, y)']
            writer = csv.DictWriter(f, fieldnames=fieldnames, dialect="unix")
            writer.writeheader()
            for bits, xy in pairs_xy.items():
                writer.writerow({'size_bits': bits, '(x, y)': xy})


    # Dispatch selon le type d'analyse
    if config["type"] == "time":
        run_time_analysis(config=config, pairs_xy=pairs_xy, paths=paths)
    elif config["type"] == "varClause":
        pass
    elif config["type"] == "randomModifiedVar":
        run_randomModifiedVar_analysis()
    elif config["type"] == "uselessDeletedVar":
        run_uselessDeletedVar_analysis(config=config, primes_per_bits=primes_per_bits, paths=paths)
    elif config["type"] == "complexity":
        run_complexity_analysis(config=config, pairs_xy=pairs_xy, paths=paths)
    else:
        raise ValueError(f"Type d'analyse inconnu: {config['type']}") 