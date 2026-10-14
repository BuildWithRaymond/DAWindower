"""Verify a one-file x86 DAWindower package using the existing PE inspector."""

import argparse
import hashlib
import struct
from pathlib import Path

from inspect_pe import PE32

SYSTEM_DLLS = {"kernel32.dll", "user32.dll", "gdi32.dll", "comctl32.dll", "wtsapi32.dll"}
EXPORTS = {"DAWindowerInitialize", "DAWindowerShutdown", "DAWindowerGetStats"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path, help="CMake installation directory")
    args = parser.parse_args()
    if not args.directory.is_dir():
        parser.error("package directory does not exist")
    entries = list(args.directory.iterdir())
    if len(entries) != 1 or entries[0].name != "DAWindower.dll" or not entries[0].is_file():
        parser.error("package must contain exactly one file: DAWindower.dll")
    try:
        pe = PE32(entries[0])
        dependencies = {item["dll"].lower() for item in pe.imports()}
        exports = {item["name"] for item in pe.exports()}
    except (OSError, ValueError, struct.error) as error:
        parser.error(f"invalid PE image: {error}")
    if pe.machine != 0x14C:
        parser.error("DAWindower must be a native x86 (i386) DLL")
    if not pe.characteristics & 0x2000:  # IMAGE_FILE_DLL in the COFF header.
        parser.error("PE image is not marked as a DLL")
    unexpected = dependencies - SYSTEM_DLLS
    if unexpected:
        parser.error(f"noncontract imports: {', '.join(sorted(unexpected))}")
    if exports != EXPORTS:
        parser.error("optional export ABI differs from the standalone contract")
    print(f"Standalone package passed: {len(pe.data):,} bytes, PE32/i386, system imports only.")
    print(f"SHA256 {hashlib.sha256(pe.data).hexdigest()}")


if __name__ == "__main__":
    main()
