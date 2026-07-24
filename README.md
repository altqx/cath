# Catherine Classic — Playable Linux Packaging

Scripts and docs to make [Catherine Classic](https://store.steampowered.com/app/893180/) playable under **current Proton** on Linux: fix the New Game softlock and grey cutscenes/VA caused by missing Media Foundation codecs.

Game install (unchanged):  
`~/.local/share/Steam/steamapps/common/CatherineClassic`

## Requirements

- Steam with Catherine Classic installed (app id `893180`)
- A **current** Proton build (preferred):
  - Proton Experimental, latest GE-Proton, Proton-CachyOS Latest, or DW-Proton Latest
  - Fallback: **GE-Proton8-32** if New Game still softlocks on a bleeding-edge build
- `ffmpeg` / `ffprobe`
- `protontricks` (installed by `scripts/setup-prefix.sh` if missing)

Prefix tweaks live in `compatdata/893180` and apply whichever Proton you force in Steam.

## How we did it

Catherine’s 3D game usually runs under Proton; **cutscenes softlock or go grey** because they play `.wmv` files through **Windows Media Foundation**, and a stock Proton prefix cannot decode the shipped codecs (MPEG-4 + WMA).

We fixed that without patching `Catherine.exe`:

1. **Prefix** — `setup-prefix.sh` installs MF helpers (`mf_catherine.verb` + lavfilters) into the app’s Proton prefix.
2. **Movies** — `reencode-movies.sh` backups stock WMVs, then rewrites them as **H.264 + AAC** while keeping the same `.wmv` filenames (the game hardcodes those paths).
3. **Mode switch** — `apply-video-mode.sh h264` uses Proton’s **builtin** MF + winegstreamer for those re-encoded files.  
   The older path (`mf-wmv` + stock codecs + GE-Proton8-32) also works, but **mixing** mf_catherine DLLs with H.264-in-`.wmv` **crashes**.

Full write-up: [`docs/how-it-works.md`](docs/how-it-works.md).

## Quick start

```bash
# 1) Steam → Properties → Compatibility → force a current Proton
#    (Experimental / latest GE / CachyOS / DW). Launch options: empty.
#    Do NOT use PROTON_USE_WINED3D=1

# 2) Launch once so the Proton prefix exists, then:
./scripts/setup-prefix.sh

# 3) Required on current Proton (fixes hang on Shakespeare quote → first FMV):
./scripts/reencode-movies.sh --only movie2   # quick unblock
./scripts/reencode-movies.sh                 # all story cutscenes
./scripts/apply-video-mode.sh h264           # builtin MF + winegstreamer

# 4) If it crashes right after the quote, you mixed mf_catherine DLLs with H.264 —
#    re-run: ./scripts/apply-video-mode.sh h264

# 5) GE-Proton8-32 + stock WMV path instead:
#    ./scripts/restore-movies.sh
#    ./scripts/apply-video-mode.sh mf-wmv
```

On current Proton, stock MPEG-4/WMA `.wmv` files often softlock after the Shakespeare quote. Re-encode replaces them with H.264+AAC (same `.wmv` names). **Do not** leave `mf_catherine` DLL overrides active with those files — that combo crashes; use `apply-video-mode.sh h264`.

If video still crashes after `h264` mode and you use gamescope (or other wrappers), temporarily set launch options to just `%command%` to rule that out.

## Scripts

| Script | Purpose |
|--------|---------|
| `scripts/setup-prefix.sh` | `mf_catherine` + lavfilters + disable winegstreamer on prefix `893180` |
| `scripts/reencode-movies.sh` | Backup + re-encode all `data/movie*` WMVs (fallback for latest Proton) |
| `scripts/verify-codecs.sh` | Probe sample WMV codecs |
| `scripts/steam-launch-notes.sh` | Print Steam settings checklist |
| `scripts/restore-movies.sh` | Restore WMVs from `artifacts/movie-backup` |
| `scripts/fix-blurry-camera.sh` | Turn off DoF/Blur (sticky soft camera on Proton) |
| `scripts/apply-video-mode.sh` | Switch h264 vs mf-wmv DLL mode |

## Layout

```
.
  README.md
  vendor/mf_catherine.verb
  scripts/
  docs/how-it-works.md     # why cutscenes break + how the fix works
  docs/formats.md
  artifacts/               # local only (gitignored): logs, movie-backup, …
```

## Blurry camera / soft cutscenes

On Proton, **Depth of Field** and **Blur** often stick and leave the camera soft.

```bash
./scripts/fix-blurry-camera.sh
```

Or in-game: Options → Graphics → Depth of Field **Off**, Blur **Off**. Fully quit and relaunch.

If you use gamescope (or similar), match its output size to the in-game resolution so the image isn’t soft from upscaling.

## Undo

- **Prefix only:** delete `~/.local/share/Steam/steamapps/compatdata/893180` (Steam will recreate). Re-run `./scripts/setup-prefix.sh` after launching once.
- **Movies:** `./scripts/restore-movies.sh`

## Success criteria

- New Game proceeds past the first cutscene without softlock on a **current** Proton.
- Anime/story WMVs show video and audible audio (not grey/mute).
- GE-Proton8-32 remains a documented fallback, not a hard requirement.
