from runner import run_experiment
import sys
import os
from analysis.randomModifiedVar_analysis import flip_variable_in_dimacs

""" from z3 import *
flip_variable_in_dimacs("data/dimacs/old/4x4.dimacs")

solver = Solver()
solver.from_file("data/dimacs/old/4x4.dimacs")
res = solver.check()
print(res) """

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 code/main.py <path/to/config.json>")
        exit(1)

    config_path = sys.argv[1]

    run_experiment(config_path)
