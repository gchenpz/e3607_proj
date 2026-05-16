#!/usr/bin/env python3

import sys

WSPR_SYMBOLS = 162

POLY1 = 0xF2D05351
POLY2 = 0xE4613C47

SYNC_VECTOR = [
    1,1,0,0,0,0,0,0,1,0,0,0,1,1,1,0,0,0,1,0,0,1,0,1,1,1,1,0,0,0,0,0,
    0,0,1,0,0,1,0,1,0,0,0,0,0,0,1,0,1,1,0,0,1,1,0,1,0,0,0,1,1,0,1,0,
    0,0,0,1,1,0,1,0,1,0,1,0,1,0,0,1,0,0,1,0,1,1,0,0,0,1,1,0,1,0,1,0,
    0,0,1,0,0,0,0,0,1,0,0,1,0,0,1,1,1,0,1,1,0,0,1,1,0,1,0,0,0,1,1,1,
    0,0,0,0,0,1,0,1,0,0,1,1,0,0,0,0,0,0,0,1,1,0,1,0,1,1,0,0,0,1,1,0,
    0,0
]

CHARS = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ '

def encode_callsign(call):
    call = call.upper().strip()

    if len(call) >= 2 and call[1].isdigit():
        call = (' ' + call).ljust(6)[:6]
    else:
        call = call.ljust(6)[:6]

    n = CHARS.index(call[0])
    n = n * 36 + CHARS.index(call[1])
    n = n * 10 + CHARS.index(call[2])
    n = n * 27 + (CHARS.index(call[3]) - 10)
    n = n * 27 + (CHARS.index(call[4]) - 10)
    n = n * 27 + (CHARS.index(call[5]) - 10)

    return n

def encode_locator_power(loc, pwr):
    loc = loc.upper().strip()

    m  = (179 - 10 * (ord(loc[0]) - ord('A')) - int(loc[2])) * 180
    m += 10 * (ord(loc[1]) - ord('A')) + int(loc[3])

    return m * 128 + pwr + 64

def pack_50_bits(n, m):
    combined = (n << 22) | m
    bits = []

    for i in range(49, -1, -1):
        bits.append((combined >> i) & 1)

    bits += [0] * 31
    return bits

def parity(x):
    x ^= x >> 16
    x ^= x >> 8
    x ^= x >> 4
    x ^= x >> 2
    x ^= x >> 1
    return x & 1

def conv_encode(bits):
    reg = 0
    out = []

    for b in bits:
        reg = ((reg << 1) | b) & 0xFFFFFFFF
        out.append(parity(reg & POLY1))
        out.append(parity(reg & POLY2))

    return out

def bit_reverse_8(i):
    out = 0

    for _ in range(8):
        out = (out << 1) | (i & 1)
        i >>= 1

    return out

def interleave(bits):
    dest = [0] * WSPR_SYMBOLS
    p = 0

    for i in range(256):
        j = bit_reverse_8(i)

        if j < WSPR_SYMBOLS:
            dest[j] = bits[p]
            p += 1

            if p == WSPR_SYMBOLS:
                break

    return dest

def encode_wspr(callsign, locator, power):
    n = encode_callsign(callsign)
    m = encode_locator_power(locator, power)

    bits = pack_50_bits(n, m)
    coded = conv_encode(bits)
    interl = interleave(coded)

    symbols = []

    for sync, data in zip(SYNC_VECTOR, interl):
        symbols.append(sync + 2 * data)

    return symbols

def main():
    if len(sys.argv) != 4:
        print("usage: python3 print_wspr_symbols.py CALLSIGN LOCATOR POWER")
        sys.exit(1)

    callsign = sys.argv[1]
    locator = sys.argv[2]
    power = int(sys.argv[3])

    symbols = encode_wspr(callsign, locator, power)

    print("/*")
    print(" * WSPR symbols for:")
    print(" *   callsign =", callsign)
    print(" *   locator  =", locator)
    print(" *   power    =", power)
    print(" */")
    print("static const uint8_t symbols[162] = {")

    for i in range(0, 162, 18):
        row = symbols[i:i+18]
        print("    " + ", ".join(str(x) for x in row) + ",")

    print("};")

if __name__ == "__main__":
    main()