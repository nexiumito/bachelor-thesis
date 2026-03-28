import csv
from sympy import primerange
from notifications.discordNotification import send_discord_prime_notification, send_discord_notification
from utils.config import CSV_DIR

def find_n_bit_primes(n):
    """
    Trouve tous les nombres premiers ayant exactement n bits utiles.
    """
    lower_bound = 2**(n - 1)
    upper_bound = 2**n
    return primerange(lower_bound, upper_bound)



def primes_calculator(filepath: str, k_max: int = 31, block_size: int = 10**4):

    with open(filepath, 'x', newline='') as csvfile:
        fieldnames = ['size_bits', 'prime']
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames, dialect="unix")
        writer.writeheader()

        for bits in range(5, k_max + 1):
            lower = 2 ** (bits - 1)
            upper = 2 ** bits

            current = lower
            nb_prime = 0
            while current < upper and nb_prime < block_size:
                next_block = min(current + block_size, upper)
                primes = primerange(current, next_block)

                for p in primes:
                    if nb_prime >= block_size:
                        break
                    writer.writerow({'size_bits': bits, 'prime': p})
                    nb_prime += 1

                current = next_block


# Lire les nombres premiers d'un csv
def load_primes(csv_filename: str):
    primes_per_bits = {}
    K = 0
    with open(csv_filename, newline='') as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            size = int(row["size_bits"])
            K = size
            prime = int(row["prime"])
            if size not in primes_per_bits:
                primes_per_bits[size] = []
            primes_per_bits[size].append(prime)
    send_discord_notification(f"nombre premier chargé pour k = {size} !")
    return primes_per_bits