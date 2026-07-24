# Native Linux / Vulkan port (Catherine-only)

Clean-room runtime lives in [`native/`](../native/). Phase 1: **NIF 20.6 viewer on Vulkan**.

## Status

| Item | State |
|------|--------|
| CMake + SDL3 + Vulkan scaffold | yes |
| NIF 20.6.0.0 mesh extract | yes (`cath-viewer`) |
| DDS BC textures | partial (solid debug color; uncompressed DDS only) |
| Full game / puzzle / TEA / CRI | not started |
| Proton packaging (`scripts/`) | still the playable full game |

## Build / run

See [`native/README.md`](../native/README.md).

## Later phases

1. KF/KFM animation, lounge/title
2. ffmpeg movie player (replace MF)
3. TEA UI / input / audio
4. Puzzle rules via RE of `Catherine.exe`
5. Story/events, saves, Steamworks stub

Gamebryo SDK: optional private reference only (`native/scripts/clone-gamebryo-ref.sh`).
