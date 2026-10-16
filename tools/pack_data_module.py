#!/usr/bin/env python3
"""Package one read-only ELF32 ARM data segment and its ABS32 relocations.

No Vita metadata is synthesized: the linker input owns every record and symbol.
"""
import argparse
import struct
from pathlib import Path


def package(input_path, output_path, module_info_symbol):
    data = Path(input_path).read_bytes()
    assert data[:7] == b'\x7fELF\x01\x01\x01', 'requires ELF32 little endian'
    header = struct.unpack_from('<16sHHIIIIIHHHHHH', data)
    _, kind, machine, _, entry, phoff, shoff, flags, _, phsize, phnum, shsize, shnum, _ = header
    assert kind == 2 and machine == 40 and phsize == 32 and shsize == 40
    segments = [struct.unpack_from('<8I', data, phoff + i * phsize) for i in range(phnum)]
    loads = [s for s in segments if s[0] == 1 and s[5]]
    assert len(loads) == 1, 'requires exactly one nonempty load segment'
    _, offset, base, _, size, memsize, perms, _ = loads[0]
    assert size == memsize and perms in (4, 5)
    payload = data[offset:offset + size]
    sections = [struct.unpack_from('<10I', data, shoff + i * shsize) for i in range(shnum)]
    for section in sections:
        if section[2] & 2:
            assert not section[2] & 5, "writable or executable section forbidden"
            assert section[1] == 1 and base <= section[3] and section[3] + section[5] <= base + size
    symbols = {}
    module_address = None
    for index, section in enumerate(sections):
        if section[1] != 2:
            continue
        strings = sections[section[6]]
        names = data[strings[4]:strings[4] + strings[5]]
        table = []
        for pos in range(section[4], section[4] + section[5], 16):
            name, value, extent, info, other, owner = struct.unpack_from('<IIIBBH', data, pos)
            assert info & 15 != 2, 'function symbol forbidden in data-only module'
            label = names[name:].split(b'\0', 1)[0].decode()
            table.append((value, owner))
            if label == module_info_symbol:
                module_address = value
        symbols[index] = table
    assert module_address is not None and base <= module_address < base + size
    relocations = bytearray()
    seen = set()
    for section in sections:
        if section[1] != 9:
            continue
        target = sections[section[7]]
        if not target[2] & 2:
            continue
        for pos in range(section[4], section[4] + section[5], 8):
            address, info = struct.unpack_from('<II', data, pos)
            assert info & 255 == 2, 'only R_ARM_ABS32 supported'
            value, owner = symbols[section[6]][info >> 8]
            assert owner != 0 and base <= value < base + size
            patch = address - base
            assert 0 <= patch <= size - 4 and patch not in seen
            seen.add(patch)
            linked, = struct.unpack_from('<I', payload, patch)
            assert base <= linked < base + size
            relocations += struct.pack('<III', 2 << 8, linked - base, patch)
    assert relocations, 'requires explicit linker relocations'
    # Preserve a zero-size RW descriptor and the two empty SCE descriptors.
    start = 0xe0
    rela_offset = (start + size + 15) & ~15
    ident = b'\x7fELF\x01\x01\x01' + bytes(9)
    result = bytearray(struct.pack('<16sHHIIIIIHHHHHH', ident, 0xfe04, 40, 1,
        module_address - base, 52, 0, 0x05000000, 52, 32, 5, 0, 0, 0))
    records = [(1, start, base, 0, size, size, 0x0ee00005, 16),
               (1, rela_offset, 0, 0, 0, 0, 0x0fb00006, 16),
               (0x60000000, rela_offset, 0, 0, len(relocations), 0, 0, 16),
               (0x60000000, 0, 0, 0, 0, 0, 0x10000, 1),
               (0x6fffff01, 0, 0, 0, 0, 0, 0, 16)]
    for record in records:
        result += struct.pack('<8I', *record)
    result += bytes(start - len(result)) + payload
    result += bytes(rela_offset - len(result)) + relocations
    Path(output_path).write_bytes(result)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', help='linked ELF32 ARM read-only data object')
    parser.add_argument('output', help='output SCE RELEXEC data module')
    parser.add_argument('--module-info-symbol', required=True)
    args = parser.parse_args()
    package(args.input, args.output, args.module_info_symbol)
