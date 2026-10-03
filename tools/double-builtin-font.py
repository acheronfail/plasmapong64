#!/usr/bin/env python3
"""Extract the pinned libdragon CC0 At01 font as a doubled BMFont.

Usage: python3 tools/double-builtin-font.py /libdragon/src/rdpq/rdpq_font_builtin.c
The output retains glyph advances, black outlines and nearest-neighbour pixels.
"""
import pathlib
import re
import struct
import sys
import zlib

source = pathlib.Path(sys.argv[1]).read_text()
array = source.split('unsigned char __fontdb_at01[] = {')[1].split('};')[0]
font = bytes(int(h, 16) for h in re.findall(r'0x([0-9a-f]{2})', array))
assert font[:8] == b'FNT\x0b\x00\x00\x00\x02'
glyph_offset = struct.unpack_from('>I', font, 80)[0]
atlas_offset = struct.unpack_from('>I', font, 88)[0]
sprite_offset = struct.unpack_from('>I', font, atlas_offset)[0]
aw, ah = struct.unpack_from('>HH', font, sprite_offset)
assert font[sprite_offset + 5] & 31 == 8  # CI4, two outlined layers
size = 256
pixels = bytearray(size * size * 4)
lines = [
    'info face="At01 doubled" size=22 bold=0 italic=0 charset="" unicode=1 stretchH=100 smooth=0 aa=1 padding=0,0,0,0 spacing=0,0',
    'common lineHeight=28 base=22 scaleW=256 scaleH=256 pages=1 packed=0',
    'page id=0 file="at01-2x.png"',
    'chars count=95',
]
for code in range(32, 127):
    advance, x0, y0, x1, y1, s, t, layer = struct.unpack_from('>BbbbbBBB', font, glyph_offset + (code - 32) * 8)
    assert layer >> 2 == 0 and (layer & 3) <= 1
    gx, gy = ((code - 32) % 10) * 24, ((code - 32) // 10) * 24
    w, h = x1 - x0, y1 - y0
    assert w * 2 <= 24 and h * 2 <= 24
    for y in range(h):
        for x in range(w):
            index = (t + y) * aw + s + x
            nibble = (font[sprite_offset + 8 + index // 2] >> (0 if index % 2 else 4)) & 15
            # The first palette selects the upper pair; the second selects
            # the lower pair (matching mkfont's outlined-layer packing).
            value = (nibble >> ((1 - (layer & 3)) * 2)) & 3
            rgba = bytes((255, 255, 255, 255)) if value == 1 else bytes((0, 0, 0, 255)) if value == 2 else bytes(4)
            for dy in range(2):
                for dx in range(2):
                    offset = ((gy + y * 2 + dy) * size + gx + x * 2 + dx) * 4
                    pixels[offset:offset + 4] = rgba
    lines.append(f'char id={code} x={gx} y={gy} width={w*2} height={h*2} xoffset={x0*2} yoffset={(y0+11)*2} xadvance={advance*2} page=0 chnl=15')

def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))

png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0))
png += chunk(b'IDAT', zlib.compress(b''.join(b'\0' + pixels[y*size*4:(y+1)*size*4] for y in range(size)))) + chunk(b'IEND', b'')
root = pathlib.Path(__file__).resolve().parents[1] / 'assets/fonts'
(root / 'at01-2x.png').write_bytes(png)
(root / 'at01-2x.fnt').write_text('\n'.join(lines) + '\n')
