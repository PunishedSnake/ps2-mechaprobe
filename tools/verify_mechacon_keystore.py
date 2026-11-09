#!/usr/bin/env python3
"""Verify locally supplied MechaCon/MagicGate key-file identities without exposing bytes."""
import argparse
import hashlib
from pathlib import Path

EXPECTED = {
    'civ.bin': (8, '5354987f01d9d5e1390ef9946342d1ecad6211b1', 'e7b02f4f8d99a58b96dbca4db81c5d666ea7c46fbf6e1d5c045eaba0ee25416a'),
    'cks.bin': (96, 'a793f867b0325e67982ba8e53f659480c1f512bb', 'fef2015096181409b25fb4c4cd0e0fc48ca73c6ea845c0ed785c06bf9becd84e'),
    'eks.bin': (1024, '5a3a3ac6f03cf100ef6e8bc92d22e91ec14e27cf', '04bcc6b13827829fb5cc8dbd86420d30f69a2bfd3b7719398b341e15368bd365'),
    'kek.bin': (16, '77227dbaaea1042f5972e65a56d2234024e8eff8', 'f7c9233b37a7662a882eca096f43b35af74b3de6030c1ad5185692332e96aecb'),
}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, required=True,
                        help='Directory holding privately managed key files, outside repository')
    args = parser.parse_args()
    failures = 0
    for filename, (size, sha1, sha256) in EXPECTED.items():
        path = args.directory / filename
        try:
            data = path.read_bytes()
        except OSError as exc:
            print(f'{filename}: MISSING/UNREADABLE ({exc.__class__.__name__})')
            failures += 1
            continue
        valid = (len(data) == size and hashlib.sha1(data).hexdigest() == sha1
                 and hashlib.sha256(data).hexdigest() == sha256)
        print(f'{filename}: {"MATCH" if valid else "MISMATCH"} ({len(data)} B)')
        failures += not valid
    if not failures:
        print('All four static keystore files match the documented reference set.')
    return int(bool(failures))

if __name__ == '__main__':
    raise SystemExit(main())
