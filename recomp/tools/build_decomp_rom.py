#!/usr/bin/env python3
"""Build kengriffey_decomp.z64: baserom.z64 with every code module decompressed and appended.

The boot segment's module table (RAM 0x80067DA8, 0x30-byte entries) lists each module as
{char name[16]; u32 code_file; u32 data_file; u32 code_vram; u32 data_vram;
 u32 bss_start; u32 bss_end; u32 unk; u32 unk}.
Module 0 ("toplevel") is a single file holding both code and data.

Each module's code+data is appended contiguously (in vram order) starting at ROM 0x1000000.
The script prints the ROM offsets so they can be checked against the splat yaml.

Usage: build_decomp_rom.py <baserom.z64> <output.z64>
"""
import hashlib
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from decompress_file import BLOCK_SIZE, RAM_TO_ROM, TABLE_RAM, mio0_block  # noqa: E402

MODULE_TABLE_RAM = 0x80067DA8
MODULE_COUNT = 10
APPEND_START = 0x1000000


def read_file(rom, index):
    entry = TABLE_RAM - RAM_TO_ROM + index * 12
    start, end, size = struct.unpack(">3I", rom[entry:entry + 12])
    if size < 2:
        return rom[start:end]
    data = bytearray()
    for pos in range(start, end, BLOCK_SIZE):
        mio0_block(rom[pos:min(pos + BLOCK_SIZE, end)], data)
    assert len(data) == size, f"file {index}: got {len(data):#x}, expected {size:#x}"
    return bytes(data)


def main():
    rom_path, out_path = sys.argv[1], sys.argv[2]
    rom = open(rom_path, "rb").read()
    assert len(rom) == APPEND_START
    out = bytearray(rom)

    for i in range(MODULE_COUNT):
        off = MODULE_TABLE_RAM - RAM_TO_ROM + i * 0x30
        name = rom[off:off + 16].split(b"\0")[0].decode()
        code_file, data_file, code_vram, data_vram, bss_start, bss_end = struct.unpack(
            ">6I", rom[off + 16:off + 40])

        blob = read_file(rom, code_file)
        if i != 0:
            assert code_vram + len(blob) == data_vram, name
            blob += read_file(rom, data_file)
        assert code_vram + len(blob) == bss_start, name

        rom_start = len(out)
        out += blob
        print(f"{name:9} rom {rom_start:#09x}-{len(out):#09x} vram {code_vram:#x} "
              f"data {data_vram:#x} (rom {rom_start + data_vram - code_vram:#x}) "
              f"bss {bss_start:#x}-{bss_end:#x} (size {bss_end - bss_start:#x})")

    open(out_path, "wb").write(out)
    print(f"{out_path}: {len(out):#x} bytes, sha1 {hashlib.sha1(out).hexdigest()}")


if __name__ == "__main__":
    main()
