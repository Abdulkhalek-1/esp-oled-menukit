#!/usr/bin/env python3
"""Convert docs/img/*.pgm produced by host_render to scaled PNGs.

Reads each P5 PGM (128x64, 8-bit grayscale), upscales 4x nearest-neighbor
(-> 512x256 so the OLED pixel grid is visible in the README), and writes
docs/img/<name>.png. The PGM files are deleted after conversion.

Uses Pillow when available; otherwise falls back to a stdlib-only PNG
writer using zlib + struct + binascii.crc32.
"""

import binascii
import glob
import os
import struct
import sys
import zlib

SCALE = 4


def read_pgm(path):
    with open(path, "rb") as fh:
        data = fh.read()
    if not data.startswith(b"P5"):
        raise ValueError(f"{path}: not a P5 PGM")
    i = 2
    tokens = []
    while len(tokens) < 3:
        while i < len(data) and chr(data[i]).isspace():
            i += 1
        if i < len(data) and data[i:i + 1] == b"#":
            while i < len(data) and data[i:i + 1] != b"\n":
                i += 1
            continue
        j = i
        while j < len(data) and not chr(data[j]).isspace():
            j += 1
        tokens.append(data[i:j])
        i = j
    width = int(tokens[0])
    height = int(tokens[1])
    _maxval = int(tokens[2])
    i += 1  # single whitespace after maxval, then raw bytes
    pixels = data[i:i + width * height]
    if len(pixels) != width * height:
        raise ValueError(f"{path}: expected {width * height} pixel bytes, got {len(pixels)}")
    return width, height, pixels


def scale_nn(pixels, w, h, factor):
    sw, sh = w * factor, h * factor
    out = bytearray(sw * sh)
    for y in range(sh):
        src_y = y // factor
        row_src = pixels[src_y * w:(src_y + 1) * w]
        for x in range(sw):
            out[y * sw + x] = row_src[x // factor]
    return bytes(out), sw, sh


def write_png_stdlib(path, w, h, pixels):
    sig = b"\x89PNG\r\n\x1a\n"

    def chunk(tag, body):
        return (struct.pack(">I", len(body)) + tag + body
                + struct.pack(">I", binascii.crc32(tag + body) & 0xFFFFFFFF))

    ihdr = struct.pack(">IIBBBBB", w, h, 8, 0, 0, 0, 0)  # 8-bit grayscale
    raw = bytearray()
    for y in range(h):
        raw.append(0)  # filter byte: None
        raw += pixels[y * w:(y + 1) * w]
    idat = zlib.compress(bytes(raw), 9)
    with open(path, "wb") as fh:
        fh.write(sig)
        fh.write(chunk(b"IHDR", ihdr))
        fh.write(chunk(b"IDAT", idat))
        fh.write(chunk(b"IEND", b""))


def write_png_pil(path, w, h, pixels):
    from PIL import Image
    Image.frombytes("L", (w, h), pixels).save(path, "PNG")


try:
    from PIL import Image  # noqa: F401
    write_png = write_png_pil
    backend = "Pillow"
except ImportError:
    write_png = write_png_stdlib
    backend = "stdlib"


def main():
    img_dir = os.path.abspath(
        os.path.join(os.path.dirname(__file__), "..", "..", "docs", "img"))
    pgms = sorted(glob.glob(os.path.join(img_dir, "*.pgm")))
    if not pgms:
        print(f"No PGMs found in {img_dir}", file=sys.stderr)
        sys.exit(1)
    print(f"PNG backend: {backend}")
    for pgm in pgms:
        w, h, px = read_pgm(pgm)
        scaled, sw, sh = scale_nn(px, w, h, SCALE)
        png = pgm[:-4] + ".png"
        write_png(png, sw, sh, scaled)
        os.remove(pgm)
        print(f"{os.path.basename(pgm)} -> {os.path.basename(png)} ({sw}x{sh})")


if __name__ == "__main__":
    main()
