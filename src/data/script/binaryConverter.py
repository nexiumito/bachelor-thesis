def bin_to_dec(n: str):
    sum = 0
    for i, j in enumerate(n[::-1]):
        sum += int(j) * 2**i 
    return sum

def dec_to_bin(n: str):
    res = ''
    while n > 0:
        res = str(n % 2) + res
        n //= 2
    return res