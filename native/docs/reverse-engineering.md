# Catherine.exe reverse-engineering notes

Living doc for formats touched by the native port.

## Known

- PE32, Gamebryo NiD3D10, XAudio2, Steamworks, CRI (`cpk`/`csb`)
- NIF **20.6.0.0** with `NiMesh` / `NiDataStream` (INDEX `F_UINT16_1`, VERTEX `F_FLOAT32_3`)
- Movies: stock WMV (MPEG-4+WMA) or re-encoded H.264+AAC; native uses **ffmpeg**
- DDS: DXT1/DXT3/DXT5 (BC1/BC2/BC3) common in `data/title`

## PZLe puzzle maps (`data/puzzle/map/*.map`)

```
0x00  'PZLe'
0x04  u16be version (=1)
0x06  u16be (=4)
0x0C  u8 width, u8 depth(Y), u8 layers, u8 flags
0x14  f32be camera-ish params
0x38  u8 startX, startY, startZ, facing
0xE0  column table: width*height records × 136 bytes
        each record ≈ 8 × 17-byte layer slots
        solid markers observed: 0x11, 0x10, 0x15, 0x21, …
```

Floats in the header look **big-endian** (PS3 leftover). Native heuristic builds occupancy + places a Goal on the highest solid. Built-in stages cover Golden-path Stage 1–3 when decode is incomplete.

## BF scripts (`data/puzzle/script/*.bf`)

Contain `FLW0`, labels like `pzl_01_01_start` / `MSG_000`. `probe_bf` harvests labels; full opcode interpreter TBD (Ghidra).

## CRI CPK

Magic `CPK ` + `@UTF` TOC. `AudioEngine::list_cpk` scans name tokens (`.hca`, paths). HCA decode not yet wired.

## TEA / SP2

- `data/tea/*_StringPatch.xml` — string overrides (regex scrape)
- `*.sp2` — large sprite packs; may embed DDS; currently probed

## Saves

Original AppData format not fully RE’d. Native uses JSON `~/.local/share/cath/save0.json` (`script_index`, `checkpoint`, `ending`).

## Next RE targets

1. Complete PZLe block-type / sheep / trap fields
2. EVT/EVS/BMD command set (`MOVIE_PLAY`, lounge, mail)
3. Compatible binary save import
4. `NiSkinningMeshModifier` + KF transform tracks
