from analysis.SATTools import *
from notifications.discordNotification import send_discord_notification

def time_evaluation(pairs: dict[int, list[tuple[int, int]]], dimacs_dir: str, simplifie: bool = False, bloat: bool = False) -> dict[int, list[float]]:

    time_per_bits = {}
    for size_bits, pair in pairs.items():
        times = []
        for x, y in pair:
            ts, _, _, _ = SAT_producer(x, y, simplifie=simplifie, bloat=bloat, repository=dimacs_dir)
            elapsed = time_to_solve(ts.dimacs)
            times.append(elapsed)
        time_per_bits[size_bits] = times
        send_discord_notification(f"évaluation du temps terminée pour k = {size_bits} !")
        
    return time_per_bits