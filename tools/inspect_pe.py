"""Read-only PE32 metadata/import/export inspection; Python standard library only."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import struct


class PE32:
    def __init__(self, path):
        self.data = path.read_bytes()
        if self.data[:2] != b'MZ':
            raise ValueError('Not a DOS/PE image')
        nt = self.unpack('<I', 0x3c)[0]
        if self.data[nt:nt + 4] != b'PE\0\0':
            raise ValueError('Missing PE signature')
        self.machine, count, self.timestamp = self.unpack('<HHI', nt + 4)
        self.characteristics = self.unpack('<H', nt + 22)[0]
        optional_size = self.unpack('<H', nt + 20)[0]
        optional = nt + 24
        if self.unpack('<H', optional)[0] != 0x10b:
            raise ValueError('This focused inspector supports PE32 only')
        self.entry = self.unpack('<I', optional + 16)[0]
        self.base = self.unpack('<I', optional + 28)[0]
        self.headers = self.unpack('<I', optional + 60)[0]
        self.directories = [self.unpack('<II', optional + 96 + n * 8) for n in range(2)]
        self.sections = []
        for n in range(count):
            offset = optional + optional_size + n * 40
            name, virtual_size, rva, size, raw = self.unpack('<8sIIII', offset)
            flags = self.unpack('<I', offset + 36)[0]
            self.sections.append(dict(name=name.rstrip(b'\0').decode('ascii'),
                                      rva=rva, virtual_size=virtual_size, raw_size=size,
                                      raw_offset=raw, executable=bool(flags & 0x20000000)))

    def unpack(self, fmt, offset):
        return struct.unpack_from(fmt, self.data, offset)

    def offset(self, rva):
        if rva < self.headers:
            return rva
        for section in self.sections:
            relative = rva - section['rva']
            if 0 <= relative < section['raw_size']:
                return section['raw_offset'] + relative
        raise ValueError(f'RVA 0x{rva:x} has no file-backed bytes')

    def string(self, rva):
        at = self.offset(rva)
        end = self.data.find(b'\0', at)
        if end < 0:
            raise ValueError('Unterminated PE string')
        return self.data[at:end].decode('ascii')

    def exports(self):
        rva, size = self.directories[0]
        if not rva or not size:
            return []
        fields = self.unpack('<IIHHIIIIIII', self.offset(rva))
        ordinal_base, _, count, functions, names, ordinals = fields[5:]
        result = []
        for n in range(count):
            name = self.string(self.unpack('<I', self.offset(names) + n * 4)[0])
            index = self.unpack('<H', self.offset(ordinals) + n * 2)[0]
            address = self.unpack('<I', self.offset(functions) + index * 4)[0]
            section = next((s for s in self.sections if s['rva'] <= address < s['rva'] + max(s['raw_size'], s['virtual_size'])), None)
            result.append(dict(name=name, ordinal=ordinal_base + index, rva=hex(address),
                               section=section['name'] if section else None,
                               executable=section['executable'] if section else False))
        return result

    def imports(self):
        rva, size = self.directories[1]
        if not rva or not size:
            return []
        result = []
        for n in range(size // 20):
            original, _, _, name, first = self.unpack('<IIIII', self.offset(rva) + n * 20)
            if not name:
                break
            symbols, at = [], self.offset(original or first)
            while True:
                value = self.unpack('<I', at)[0]
                if not value:
                    break
                symbols.append({'ordinal': value & 0xffff} if value & 0x80000000
                               else {'name': self.string(value + 2)})
                at += 4
            result.append(dict(dll=self.string(name), symbols=symbols))
        return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('file', type=Path)
    args = parser.parse_args()
    pe = PE32(args.file)
    print(json.dumps(dict(file=args.file.name, bytes=len(pe.data),
                         sha256=hashlib.sha256(pe.data).hexdigest(), machine=hex(pe.machine),
                         characteristics=hex(pe.characteristics),
                         timestamp_utc=datetime.datetime.fromtimestamp(pe.timestamp, datetime.timezone.utc).isoformat(),
                         image_base=hex(pe.base), entry_rva=hex(pe.entry), sections=pe.sections,
                         exports=pe.exports(), imports=pe.imports()), indent=2))


if __name__ == '__main__':
    main()
