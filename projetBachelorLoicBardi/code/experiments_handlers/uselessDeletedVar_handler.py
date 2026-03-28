import csv
import os
from analysis.uselessDeletedVar_analysis import generate_specific_mult


def run_uselessDeletedVar_analysis(config: dict, primes_per_bits: dict, paths: dict):
    
    # Génération des pairs "faibles" <=> n'exploite pas z en entier
    pairs_xy = {}
    for size_bits, primes in primes_per_bits.items():
        pairs = generate_specific_mult(primes, size_bits, config["nb_mults"])
        pairs_xy[size_bits] = pairs
        
    with open(os.path.join(paths["CSV_DIR"], "used_pairs.csv"), "w") as f:
        fieldnames = ['size_bits', '(x, y)']
        writer = csv.DictWriter(f, fieldnames=fieldnames, dialect="unix")
        writer.writeheader()
        for bits, xy in pairs_xy.items():
            writer.writerow({'size_bits': bits, '(x, y)': xy})
            
    
