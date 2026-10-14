#!/usr/bin/env python3
"""Decompress a file from Ken Griffey Jr.'s Slugfest's ROM file table.

Mirrors the boot loader (main / mio0_decompress_chunk / mio0_decode):
the table at RAM 0x80066EB8 has 12-byte entries {rom_start, rom_end, out_size}.
A file with out_size >= 2 is stored as consecutive 0x2000-byte ROM blocks, each holding
an 8-byte header {u16 unk, u16 out_size, u16 comp_off, u16 uncomp_off} followed by an
MIO0 body (offsets in the header are relative to the body, i.e. MIO0 offset - 8).

Usage: decompress_file.py <baserom.z64> <file index> <output.bin>
"""
import struct
import sys

TABLE_RAM = 0x80066EB8
RAM_TO_ROM = 0x80060600  # main's .data: RAM 0x80066EA0 <-> ROM 0x68A0
BLOCK_SIZE = 0x2000


def mio0_block(blk, out):
    """Decode one block, appending to out (back-references may reach into earlier blocks)."""
    _, out_size, comp_off, uncomp_off = struct.unpack(">4H", blk[:8])
    if comp_off == 0:
        # Stored block: the loader just copies out_size bytes after the header.
        out += blk[8:8 + out_size]
        return
    # Offsets are relative to a MIO0 header placed 8 bytes before the block.
    layout = 0x10 - 8
    comp = comp_off
    uncomp = uncomp_off
    end = len(out) + out_size
    bits = 0
    nbits = 0
    while len(out) < end:
        if nbits == 0:
            bits = struct.unpack(">I", blk[layout:layout + 4])[0]
            layout += 4
            nbits = 32
        if bits & 0x80000000:
            out.append(blk[uncomp])
            uncomp += 1
        else:
            v = struct.unpack(">H", blk[comp:comp + 2])[0]
            comp += 2
            dist = (v & 0xFFF) + 1
            for _ in range((v >> 12) + 3):
                out.append(out[-dist])
        bits = (bits << 1) & 0xFFFFFFFF
        nbits -= 1


def main():
    rom_path, index, out_path = sys.argv[1], int(sys.argv[2], 0), sys.argv[3]
    rom = open(rom_path, "rb").read()
    entry = TABLE_RAM - RAM_TO_ROM + index * 12
    start, end, size = struct.unpack(">3I", rom[entry:entry + 12])

    if size < 2:
        data = rom[start:end]
    else:
        data = bytearray()
        for pos in range(start, end, BLOCK_SIZE):
            mio0_block(rom[pos:min(pos + BLOCK_SIZE, end)], data)
        if len(data) != size:
            sys.exit(f"size mismatch: got {len(data):#x}, table says {size:#x}")

    open(out_path, "wb").write(data)
    print(f"file {index}: ROM {start:#x}-{end:#x} -> {len(data):#x} bytes -> {out_path}")


if __name__ == "__main__":
    main()
