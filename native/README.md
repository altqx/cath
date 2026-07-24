# cath — Catherine Classic clean-room native

Linux / **Vulkan** reimplementation that reads your Steam `CatherineClassic/data/` install. No Proton required for the native story path.

**Definition of done (this tree):** `./build/cath` can **New Game → cutscenes → puzzles → one ending** with save/load (`~/.local/share/cath/save0.json`).

Proton packaging under `../scripts` remains a full-fidelity interim path (Babel/Colosseum/online deferred on native).

## Build

```bash
./scripts/bootstrap.sh
# or:
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
```

Deps: `cmake`, `g++`, `sdl3`, `vulkan-headers`, `vulkan-icd-loader`, `shaderc` (glslc), `glm`, `ffmpeg` (libav*).

## Run

```bash
# Full game (title menu → New Game)
./build/cath

# Headless verification to Freedom ending
./build/cath --smoke

# Faster interactive autoplay
./build/cath --autoplay --skip-movies

# NIF / texture viewer
./build/cath-viewer --nif data/character/01/c01_00.nif
./build/cath-viewer --nif data/title/c02_00.nif

# Movie test player
./build/cath-movie --movie movie2/001_00.wmv
```

- `CATH_GAME_DIR` or `--game-dir` overrides the Steam path
- Default: `~/.local/share/Steam/steamapps/common/CatherineClassic`

### Controls

| Context | Keys / pad |
|---------|------------|
| Title | ↑↓ select, Enter/Z / A button → New Game |
| Movie | Enter/Space skip |
| Puzzle | WASD/arrows move, Shift/X pull, R reset |
| Lounge / stubs | Enter continue |

## Layout

| Module | Role |
|--------|------|
| `cath::nif` / `kf` / `tex` | NIF 20.6 meshes, KF probe, BC1/2/3 DDS |
| `cath::render` | Vulkan multi-mesh + dynamic movie textures |
| `cath::media` | ffmpeg decode → RGBA |
| `cath::audio` | SDL3 PCM/WAV; CRI CPK TOC probe (HCA stub) |
| `cath::ui` | TEA string patches, SP2 probe, title menu |
| `cath::puzzle` | PZLe map loader + tower gameplay |
| `cath::event` | Story script + BF label probe |
| `cath::game` | Mode FSM, save/load, `cath` executable |

## Success criteria

- [x] `./build/cath` on Linux/Vulkan without Proton
- [x] New Game → cutscenes (ffmpeg) → puzzles → ending `freedom`
- [x] Save/load across restart (`save0.json`)
- [x] Documented build (this README)

## Notes / honesty

- Puzzle maps use a **heuristic PZLe** decode (PS3-era column records) plus built-in completable Stage 1–3 towers. Full block-type parity needs more Ghidra RE.
- CRI HCA inside `.cpk` is TOC-probed; playback stubs to silence until a clean-room HCA path lands.
- TEA/SP2 drive the title menu labels; full SP2 atlas blit is still probed.
- KF skinning is Phase 2a (clip probe); characters draw as static multi-mesh for now.

See `docs/architecture.md` and `docs/reverse-engineering.md`.
