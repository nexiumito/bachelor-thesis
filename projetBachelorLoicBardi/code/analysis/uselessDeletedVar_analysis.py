import random
from analysis.SATTools import SAT_producer
from notifications.discordNotification import send_discord_notification
from transformation.tseytin import TseytinTransformation
from transformation.binaryConverter import dec_to_bin, bin_to_dec

def generate_specific_mult(primes_list: list[int], size_xy: int, nb_primes: int) -> list[tuple[int, int]]:
    """
    Génère des couples (x, y) tel que x * y ne couvrent pas tous les bits de z. Certains bits de poids fort de z 
    ne sont pas utilisé
    """
    mults = set()
    tries = 0
    n = 2 ** (size_xy * 2 - 1)
    print(n)
    while len(mults) < nb_primes and tries < 10000:
        x, y = random.sample(primes_list, 2)
        if x * y < n:
            pair = tuple(sorted((x, y)))
            mults.add(pair)
        tries += 1
    return list(mults)


def variable_deletion(dimacs_file: str, max_var_num: int):
    pass

def time_evaluation(pairs: dict[int, list[tuple[int, int]]], dimacs_dir: str, simplifie: bool = False, bloat: bool = False) -> dict[int, list[float]]:

    time_per_bits = {}
    for size_bits, pair in pairs.items():
        times = []
        for x, y in pair:
            ts, _, _, _ = SAT_producer(x, y, simplifie=simplifie, bloat=bloat, repository=dimacs_dir)
            elapsed = 1#time_to_solve(ts.dimacs)
            times.append(elapsed)
        time_per_bits[size_bits] = times
        send_discord_notification(f"évaluation du temps terminée pour k = {size_bits} !")
        
    return time_per_bits