# How the Linux packaging works

Catherine Classic is a 32-bit Windows Gamebryo game. Gameplay and 3D rendering usually run under Proton; **story cutscenes do not**, unless Media Foundation (MF) can decode the shipped `.wmv` files.

This repo does not patch `Catherine.exe`. It only adjusts the **Proton prefix** and optionally **re-encodes movies in the Steam install** (with a local backup).

## The failure

New Game shows the Shakespeare quote, then tries to play the first FMV under `data/movie2/` (then later `data/movie/`).

Stock files are ASF/WMV with:

- video: MPEG-4 (`mpeg4`)
- audio: Windows Media Audio (`wmav2`)

Playback goes through **Windows Media Foundation**. On a stock Proton prefix, MF codecs / registration are incomplete, so the game **softlocks** (stuck on the quote) or shows a **grey frame with no voice**.

`PROTON_USE_WINED3D=1` makes A/V worse and is not used.

## What we change

### 1. Prefix setup (`setup-prefix.sh`)

Runs winetricks (via protontricks) against Steam app `893180`:

1. **`vendor/mf_catherine.verb`** — installs MF-related DLLs into the prefix and registers them. The verb is vendored and patched (`w_try_regsvr` → `w_try_regsvr32`) so current winetricks accepts it.
2. **`lavfilters`** — extra DirectShow filters that help some video paths.
3. Prefix DLL overrides tuned for whichever **video mode** you pick next (see below).

These live in `compatdata/893180`, so they apply to whatever Proton build you force in Steam for this app.

### 2. Two incompatible video modes (`apply-video-mode.sh`)

We found two working combinations. Mixing them crashes.

| Mode | Movies on disk | MF DLLs | `winegstreamer` | Typical Proton |
|------|----------------|---------|-----------------|----------------|
| **`h264`** (preferred) | Re-encoded H.264 + AAC, still named `.wmv` | **builtin** | **builtin** | Current (Experimental, CachyOS, recent GE, …) |
| **`mf-wmv`** | Stock MPEG-4 + WMA `.wmv` | **mf_catherine** (Windows DLLs) | disabled | Fallback: GE-Proton8-32 |

**Why re-encode keeps the `.wmv` name:** the game opens fixed paths like `movie2/001_00.wmv`. Renaming to `.mp4` would require binary/path hacks. Container/codec inside the file can change; the filename must not.

**Why `mf_catherine` + H.264-in-`.wmv` fails:** the Windows MF stack from the verb does not reliably play that hybrid under newer Proton and tends to crash right after the quote. Builtin MF + winegstreamer handles the re-encoded files.

### 3. Movie re-encode (`reencode-movies.sh`)

1. Copy originals to `artifacts/movie-backup/` (gitignored, local only).
2. `ffmpeg` each `data/movie*` / `movie*_jp` file to H.264 + AAC.
3. Write back over the same `.wmv` path.

`restore-movies.sh` copies the backup back if you want the stock codecs again (then switch to `mf-wmv`).

### 4. Soft camera (`fix-blurry-camera.sh`)

Separate from cutscene codecs: on Proton, **Depth of Field** / **Blur** in `AppSettings.ini` often stick and leave the view soft. The script sets `VIDEO_DOF=0` and `VIDEO_BLUR=0`. Also match any gamescope size to the in-game resolution so the frame isn’t upscaled.

## Recommended path (summary)

```
current Proton
    → setup-prefix.sh
    → reencode-movies.sh
    → apply-video-mode.sh h264
```

Fallback if that still softlocks:

```
GE-Proton8-32
    → restore-movies.sh          # if you re-encoded
    → apply-video-mode.sh mf-wmv
```

## What we did not do

- No game executable mods, no cracked binaries.
- No redistributing Atlus assets or Windows MF DLLs (users install DLLs into their own prefix via winetricks).
- No claim of perfect shader/DoF parity with Windows.

## Related notes

- Asset inventory: [`formats.md`](formats.md)
- Scripts: see root [`README.md`](../README.md)
