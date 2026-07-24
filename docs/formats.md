# Catherine Classic — Asset / Engine Notes

Inventory from the Steam PC install for packaging work and a future native port.

## Binary

| Item | Value |
|------|-------|
| Executable | `Catherine.exe` |
| Format | PE32 (32-bit Windows GUI) |
| Engine | Gamebryo / NetImmerse (`NiD3D10*`, `NiD3DXEffect*`) |
| Graphics | Direct3D 10 |
| Audio | XAudio2 |
| Platform | Steamworks (`steam_api.dll`) |
| Middleware | CRI (`.cpk` archives) |

## Top-level data

Path: `<game>/data/`

| Directory | Role |
|-----------|------|
| `movie`, `movie_jp` | Story / anime FMV (`.wmv`) |
| `movie2`, `movie2_jp` | Shorter tip / UI videos (`.wmv`) |
| `character` | Models / anims (`.nif`, `.kf`, `.kfm`, `.dds`) |
| `puzzle`, `field`, `lounge`, … | Stage / scene content |
| `event` | Event scripts / text |
| `sound`, `sound_jp` | Audio (often inside `.cpk`) |
| `tea` | UI layouts / string patches (`.xml`) |
| `shaders` | HLSL / D3D shader data |
| `font` | Fonts |

## Extension counts (approx.)

| Ext | Count | Notes |
|-----|------:|-------|
| `.nif` | ~1627 | NetImmerse meshes |
| `.pac` | ~1481 | Packed archives (see CathLib) |
| `.bmd` | ~1151 | Scripts / binary data |
| `.dds` | ~853 | Textures |
| `.sp2` | ~579 | UI / sprite-related |
| `.kf` / `.kfm` | many | Animations |
| `.csb` | ~388 | Sound banks (CRI) |
| `.cpk` | ~176 | CRI package |
| `.wmv` | ~219 | FMV (ASF; mpeg4 + wmav2) |
| `.evt` / `.evs` / `.bf` | many | Event / flow |

## Video (Linux failure point)

Sample probe (`data/movie/005_00.wmv`):

- Container: Microsoft ASF / WMV
- Video: `mpeg4` @ 1280×720
- Audio: `wmav2` @ 128 kbps

Playback goes through Windows Media Foundation. Under Proton without MF DLLs/reg, New Game softlocks or shows a grey screen with no VA.

## Existing tooling

- [CathLib](https://github.com/marcussacana/CathLib) — translation helpers for BMD/PAC/BF/SP2 (not a runtime engine).
- NifSkope / Noesis — inspect Gamebryo NIF assets.

## Legal

Distribute only your own scripts/docs. Do not redistribute game assets, `Catherine.exe`, or Windows MF DLLs. Users must own the Steam copy; DLLs are installed into their private Proton prefix via winetricks.
