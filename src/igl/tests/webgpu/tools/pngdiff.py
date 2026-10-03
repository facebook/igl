#!/usr/bin/env fbpython
# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

"""Compares 8-bit RGB(A) PNGs (as written by IGL_RENDER_SNAPSHOT_DIR) without third-party modules.

usage:
  pngdiff.py <a.png> <b.png>          maxAbs (largest channel difference) and % of differing pixels
  pngdiff.py <dir a> <dir b> [--max-abs N]
                                      the same for every PNG present in both directories; exits 1
                                      if any pair differs by more than N (default: report only)
  pngdiff.py <a.png>                  size, first and center pixel
"""

import os
import struct
import sys
import zlib


def paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    return a if pa <= pb and pa <= pc else b if pb <= pc else c


def unfilter(filt, line, prev, bpp):
    for x in range(len(line)):
        a = line[x - bpp] if x >= bpp else 0
        b = prev[x]
        c = prev[x - bpp] if x >= bpp else 0
        predictor = (0, a, b, (a + b) // 2, paeth(a, b, c))[filt]
        line[x] = (line[x] + predictor) & 255


def read_png(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path} is not a PNG")
    pos, idat = 8, b""
    width = height = bpp = 0
    while pos < len(data):
        (n,) = struct.unpack(">I", data[pos : pos + 4])
        kind, chunk = data[pos + 4 : pos + 8], data[pos + 8 : pos + 8 + n]
        pos += 12 + n
        if kind == b"IHDR":
            width, height, depth, color, _, _, interlace = struct.unpack(
                ">IIBBBBB", chunk
            )
            if depth != 8 or interlace != 0 or color not in (2, 6):
                raise ValueError(
                    f"{path}: only 8-bit non-interlaced RGB/RGBA is supported"
                )
            bpp = 4 if color == 6 else 3
        elif kind == b"IDAT":
            idat += chunk
    raw = zlib.decompress(idat)
    stride = width * bpp
    out, prev = bytearray(), bytearray(stride)
    for row in range(height):
        start = row * (stride + 1)
        line = bytearray(raw[start + 1 : start + 1 + stride])
        unfilter(raw[start], line, prev, bpp)
        out += line
        prev = line
    if bpp == 3:
        out = bytearray(
            v for j in range(0, len(out), 3) for v in (*out[j : j + 3], 255)
        )
    return width, height, out


def diff(path_a, path_b):
    wa, ha, pa = read_png(path_a)
    wb, hb, pb = read_png(path_b)
    if (wa, ha) != (wb, hb):
        raise ValueError(f"size mismatch: {wa}x{ha} vs {wb}x{hb}")
    max_abs, differing = 0, 0
    for i in range(0, len(pa), 4):
        d = max(abs(pa[i + k] - pb[i + k]) for k in range(4))
        max_abs = max(max_abs, d)
        differing += d > 0
    return max_abs, 100.0 * differing / (wa * ha)


def diff_dirs(dir_a, dir_b, max_abs_allowed):
    names = sorted(
        n
        for n in os.listdir(dir_a)
        if n.endswith(".png") and os.path.exists(os.path.join(dir_b, n))
    )
    failed = 0
    print("| image | maxAbs | differing |")
    print("|---|---|---|")
    for name in names:
        m, pct = diff(os.path.join(dir_a, name), os.path.join(dir_b, name))
        bad = max_abs_allowed is not None and m > max_abs_allowed
        failed += bad
        print(f"| {name[:-4]} | {m} | {pct:.4f}%{' FAIL' if bad else ''} |")
    return failed


def main(argv):
    if len(argv) == 2:
        w, h, p = read_png(argv[1])
        center = (h // 2 * w + w // 2) * 4
        print(
            w, h, "first pixel", tuple(p[:4]), "center", tuple(p[center : center + 4])
        )
        return 0
    if len(argv) >= 3 and os.path.isdir(argv[1]):
        limit = int(argv[4]) if len(argv) == 5 and argv[3] == "--max-abs" else None
        return 1 if diff_dirs(argv[1], argv[2], limit) else 0
    if len(argv) == 3:
        m, pct = diff(argv[1], argv[2])
        print(f"maxAbs={m} differing={pct:.4f}%")
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
