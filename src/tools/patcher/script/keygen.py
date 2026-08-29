#!/usr/bin/env python3
"""
keygen.py - generate an RSA keypair to REPLACE the WoW 4.3.4 connection-redirect
key (client VA 0x00B987A0, exponent 65537).

You keep the private key on your world/bnet server; you bake the public modulus
into the client with patch_modulus.py. The client only ever holds the public
half, so it can verify a redirect but never forge one - exactly the public/
private split you asked about.

Output: a JSON keyfile with
  - n_le_hex : 256-byte modulus, LITTLE-ENDIAN, ready for patch_modulus.py
  - n_be_hex : big-endian modulus (OpenSSL / int)
  - e, d, p, q : private material (KEEP SECRET)

Stdlib-only crypto is used so this runs on a bare server box. Pass --openssl to
use the `cryptography` package instead (faster, audited) if it is installed.
"""
import argparse, json, os, secrets, sys

E = 65537
KEY_BYTES = 256          # the client field is exactly 256 bytes
KEY_BITS = KEY_BYTES * 8 # 2048; the stock Blizzard modulus is 2043-bit, but a
                         # full 2048-bit modulus fits the same field and is fine.


def _is_probable_prime(n, rounds=40):
    if n < 2:
        return False
    for p in (2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37):
        if n % p == 0:
            return n == p
    d, r = n - 1, 0
    while d % 2 == 0:
        d //= 2
        r += 1
    for _ in range(rounds):
        a = secrets.randbelow(n - 3) + 2
        x = pow(a, d, n)
        if x == 1 or x == n - 1:
            continue
        for _ in range(r - 1):
            x = pow(x, 2, n)
            if x == n - 1:
                break
        else:
            return False
    return True


def _gen_prime(bits):
    while True:
        cand = secrets.randbits(bits) | (1 << (bits - 1)) | 1
        if cand % E != 1 and _is_probable_prime(cand):
            return cand


def gen_stdlib():
    half = KEY_BITS // 2
    while True:
        p = _gen_prime(half)
        q = _gen_prime(half)
        if p == q:
            continue
        n = p * q
        if n.bit_length() != KEY_BITS:      # force top bit set -> exactly 2048
            continue
        phi = (p - 1) * (q - 1)
        try:
            d = pow(E, -1, phi)
        except ValueError:
            continue
        return n, d, p, q


def gen_openssl():
    from cryptography.hazmat.primitives.asymmetric import rsa
    k = rsa.generate_private_key(public_exponent=E, key_size=KEY_BITS)
    pn = k.private_numbers()
    pub = k.public_key().public_numbers()
    return pub.n, pn.d, pn.p, pn.q


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-o", "--out", default="redirect_key.json",
                    help="output keyfile (default: redirect_key.json)")
    ap.add_argument("--openssl", action="store_true",
                    help="use the cryptography package instead of stdlib")
    args = ap.parse_args()

    n, d, p, q = (gen_openssl() if args.openssl else gen_stdlib())
    assert (n.bit_length() + 7) // 8 <= KEY_BYTES

    n_be = n.to_bytes(KEY_BYTES, "big")
    n_le = n_be[::-1]
    out = {
        "comment": "WoW 4.3.4 connection-redirect replacement key. KEEP d/p/q SECRET.",
        "e": E,
        "bits": n.bit_length(),
        "n_be_hex": n_be.hex().upper(),
        "n_le_hex": n_le.hex().upper(),   # <-- feed this to patch_modulus.py
        "d_hex": format(d, "x"),
        "p_hex": format(p, "x"),
        "q_hex": format(q, "x"),
    }
    with open(args.out, "w") as f:
        json.dump(out, f, indent=2)
    try:
        os.chmod(args.out, 0o600)
    except OSError:
        pass
    print("wrote %s  (%d-bit, e=%d)" % (args.out, n.bit_length(), E))
    print("public modulus (little-endian, for patch_modulus.py):")
    print(out["n_le_hex"])


if __name__ == "__main__":
    main()
