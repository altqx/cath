#!/usr/bin/env python3
"""Debug helper: extract largest valid triangle mesh from a Catherine NIF 20.6 file."""
from __future__ import annotations
import argparse, struct
from pathlib import Path

F_UINT16_1 = 0x00010215
F_FLOAT32_2 = 0x00020436
F_FLOAT32_3 = 0x00030437


def parse_nif(path: Path):
    b = path.read_bytes()
    o = b.index(b"\n") + 1
    o += 4 + 1 + 4
    nb = struct.unpack_from("<I", b, o)[0]
    o += 4
    nbt = struct.unpack_from("<H", b, o)[0]
    o += 2
    type_raws = []
    for _ in range(nbt):
        n = struct.unpack_from("<I", b, o)[0]
        o += 4
        type_raws.append(b[o : o + n])
        o += n
    idxs = [struct.unpack_from("<H", b, o + i * 2)[0] for i in range(nb)]
    o += 2 * nb
    sizes = [struct.unpack_from("<I", b, o + i * 4)[0] for i in range(nb)]
    o += 4 * nb
    ns = struct.unpack_from("<I", b, o)[0]
    o += 4
    o += 4  # max string length
    for _ in range(ns):
        n = struct.unpack_from("<I", b, o)[0]
        o += 4 + n
    ng = struct.unpack_from("<I", b, o)[0]
    o += 4 + 4 * ng
    blocks = []
    for i in range(nb):
        name = type_raws[idxs[i]].split(b"\x01")[0].decode("ascii")
        blocks.append((name, b[o : o + sizes[i]]))
        o += sizes[i]
    return blocks


def parse_datastream(data: bytes):
    o = 0
    num_bytes = struct.unpack_from("<I", data, o)[0]
    o += 8
    num_regions = struct.unpack_from("<I", data, o)[0]
    o += 4 + 8 * num_regions
    num_comp = struct.unpack_from("<I", data, o)[0]
    o += 4
    formats = [struct.unpack_from("<I", data, o + i * 4)[0] for i in range(num_comp)]
    o += 4 * num_comp
    return formats, data[o : o + num_bytes]


def comp_size(fmt: int) -> int:
    return ((fmt >> 16) & 0xFF) * ((fmt >> 8) & 0xFF)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("nif")
    ap.add_argument("-o", "--out", required=True)
    args = ap.parse_args()
    blocks = parse_nif(Path(args.nif))
    streams = []
    for name, data in blocks:
        if name != "NiDataStream":
            continue
        formats, blob = parse_datastream(data)
        streams.append((formats, blob))
    meshes = []
    for i, (fmts, blob) in enumerate(streams):
        if fmts != [F_UINT16_1]:
            continue
        v = None
        for j in range(i + 1, min(i + 3, len(streams))):
            if F_FLOAT32_3 in streams[j][0]:
                v = streams[j]
                break
        if not v:
            continue
        idx = list(struct.unpack_from("<%dH" % (len(blob) // 2), blob))
        formats, vblob = v
        stride = sum(comp_size(f) for f in formats)
        nverts = len(vblob) // stride
        if not idx or len(idx) % 3 or max(idx) >= nverts:
            continue
        offs, o = [], 0
        for f in formats:
            offs.append(o)
            o += comp_size(f)
        f3s = [ci for ci, f in enumerate(formats) if f == F_FLOAT32_3]
        pos = []
        for vi in range(nverts):
            pos.extend(struct.unpack_from("<3f", vblob, vi * stride + offs[f3s[0]]))
        meshes.append((nverts, pos, idx))
    m = max(meshes, key=lambda t: len(t[2]))
    out = Path(args.out)
    with out.open("w") as f:
        for i in range(m[0]):
            f.write("v %f %f %f\n" % tuple(m[1][i * 3 : i * 3 + 3]))
        for i in range(0, len(m[2]), 3):
            f.write("f %d %d %d\n" % (m[2][i] + 1, m[2][i + 1] + 1, m[2][i + 2] + 1))
    print("wrote", out, "verts", m[0], "idx", len(m[2]))


if __name__ == "__main__":
    main()
