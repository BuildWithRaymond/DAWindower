"""Read-only PE32 metadata/import/export inspection; Python standard library only."""

import argparse
import datetime
import hashlib
import json
import struct
from pathlib import Path

DOS_PE_HEADER_POINTER = 0x3C
PE_SIGNATURE_SIZE = 4
COFF_HEADER_SIZE = 20
PE32_DIRECTORY_OFFSET = 96
SECTION_HEADER_SIZE = 40
IMPORT_DESCRIPTOR_SIZE = 20
EXECUTABLE_SECTION = 0x20000000
ORDINAL_IMPORT = 0x80000000


class PE32:
    """Expose the on-disk PE32 headers needed by inspection and package checks."""

    def __init__(self, path):
        self.data = path.read_bytes()
        if self.data[:2] != b"MZ":
            raise ValueError("Not a DOS/PE image")
        pe_offset = self.unpack("<I", DOS_PE_HEADER_POINTER)[0]
        if self.data[pe_offset:pe_offset + PE_SIGNATURE_SIZE] != b"PE\0\0":
            raise ValueError("Missing PE signature")

        coff_offset = pe_offset + PE_SIGNATURE_SIZE
        self.machine, section_count, self.timestamp = self.unpack("<HHI", coff_offset)
        self.characteristics = self.unpack("<H", coff_offset + 18)[0]
        optional_header_size = self.unpack("<H", coff_offset + 16)[0]
        optional_offset = coff_offset + COFF_HEADER_SIZE
        if self.unpack("<H", optional_offset)[0] != 0x10B:
            raise ValueError("This focused inspector supports PE32 only")
        self.entry = self.unpack("<I", optional_offset + 16)[0]
        self.base = self.unpack("<I", optional_offset + 28)[0]
        self.headers = self.unpack("<I", optional_offset + 60)[0]
        # Only export and import directories are needed by this inspector.
        self.directories = [
            self.unpack("<II", optional_offset + PE32_DIRECTORY_OFFSET + index * 8)
            for index in range(2)
        ]
        self.sections = []
        for index in range(section_count):
            section_offset = optional_offset + optional_header_size + index * SECTION_HEADER_SIZE
            name, virtual_size, rva, raw_size, raw_offset = self.unpack("<8sIIII", section_offset)
            flags = self.unpack("<I", section_offset + 36)[0]
            self.sections.append(
                dict(
                    name=name.rstrip(b"\0").decode("ascii"),
                    rva=rva,
                    virtual_size=virtual_size,
                    raw_size=raw_size,
                    raw_offset=raw_offset,
                    executable=bool(flags & EXECUTABLE_SECTION),
                )
            )

    def unpack(self, fmt, offset):
        return struct.unpack_from(fmt, self.data, offset)

    def offset(self, rva):
        """Translate a relative virtual address into a file-backed byte offset."""
        if rva < self.headers:
            return rva
        for section in self.sections:
            relative = rva - section["rva"]
            if 0 <= relative < section["raw_size"]:
                return section["raw_offset"] + relative
        raise ValueError(f"RVA 0x{rva:x} has no file-backed bytes")

    def string(self, rva):
        start = self.offset(rva)
        end = self.data.find(b"\0", start)
        if end < 0:
            raise ValueError("Unterminated PE string")
        return self.data[start:end].decode("ascii")

    def exports(self):
        rva, size = self.directories[0]
        if not rva or not size:
            return []
        fields = self.unpack("<IIHHIIIIIII", self.offset(rva))
        ordinal_base, _, name_count, functions, names, ordinals = fields[5:]
        result = []
        for name_index in range(name_count):
            name = self.string(self.unpack("<I", self.offset(names) + name_index * 4)[0])
            function_index = self.unpack("<H", self.offset(ordinals) + name_index * 2)[0]
            address = self.unpack("<I", self.offset(functions) + function_index * 4)[0]
            section = next(
                (
                    section
                    for section in self.sections
                    if section["rva"] <= address
                    < section["rva"] + max(section["raw_size"], section["virtual_size"])
                ),
                None,
            )
            result.append(
                dict(
                    name=name,
                    ordinal=ordinal_base + function_index,
                    rva=hex(address),
                    section=section["name"] if section else None,
                    executable=section["executable"] if section else False,
                )
            )
        return result

    def imports(self):
        rva, size = self.directories[1]
        if not rva or not size:
            return []
        result = []
        for index in range(size // IMPORT_DESCRIPTOR_SIZE):
            descriptor_offset = self.offset(rva) + index * IMPORT_DESCRIPTOR_SIZE
            original_thunk, _, _, name_rva, first_thunk = self.unpack("<IIIII", descriptor_offset)
            if not name_rva:
                break
            symbols = []
            thunk_offset = self.offset(original_thunk or first_thunk)
            while True:
                value = self.unpack("<I", thunk_offset)[0]
                if not value:
                    break
                symbols.append(
                    {"ordinal": value & 0xFFFF}
                    if value & ORDINAL_IMPORT
                    else {"name": self.string(value + 2)}
                )
                thunk_offset += 4
            result.append(dict(dll=self.string(name_rva), symbols=symbols))
        return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("file", type=Path)
    args = parser.parse_args()
    pe = PE32(args.file)
    metadata = dict(
        file=args.file.name,
        bytes=len(pe.data),
        sha256=hashlib.sha256(pe.data).hexdigest(),
        machine=hex(pe.machine),
        characteristics=hex(pe.characteristics),
        timestamp_utc=datetime.datetime.fromtimestamp(
            pe.timestamp, datetime.timezone.utc
        ).isoformat(),
        image_base=hex(pe.base),
        entry_rva=hex(pe.entry),
        sections=pe.sections,
        exports=pe.exports(),
        imports=pe.imports(),
    )
    print(json.dumps(metadata, indent=2))


if __name__ == "__main__":
    main()
