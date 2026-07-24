# Verification log

## Automated setup (2026-07-23)

| Step | Result |
|------|--------|
| Workspace `~/Work/cath` | Created |
| Vendored `mf_catherine.verb` | Patched `w_try_regsvr` → `w_try_regsvr32` for current winetricks |
| `setup-prefix.sh` on app `893180` | Success — MF DLLs in `syswow64` (`mfplat`, `evr`, `wmadmod`, `mp4sdecd`, …) |
| `lavfilters` | Already present / installed |
| `winegstreamer` override | Set to `builtin` (required for current Proton + H.264) |
| `mfplat.dll` override | `native` |
| Codec probe (stock) | `mpeg4` + `wmav2` in ASF |
| Re-encode `movie2` | Done — H.264+AAC under `.wmv` names (unblocks Shakespeare → Golden Playhouse) |
| Re-encode `movie*` full | Done — 219/219 files, 0 failures (H.264+AAC); originals in `artifacts/movie-backup/` |

Log: `artifacts/logs/setup-prefix-20260723-230928.log`  
Re-encode: `artifacts/logs/reencode-movies-*.log`

### Symptom confirmed by user

Stuck on Shakespeare quote screen (“All the world's a stage…”) on newer Proton = hang entering first `movie2` FMV. Fix: re-encode + `winegstreamer=builtin`.

### Crash right after quote (2026-07-23)

Cause: native `mf_catherine` DLLs + H.264-in-`.wmv` under Proton-CachyOS.  
Fix applied: `scripts/apply-video-mode.sh h264` (MF/evr/dxva2 → builtin, winegstreamer=builtin).  
Retest: New Game on Proton-CachyOS; if still crashes, drop gamescope from launch options temporarily.

## In-game checklist (run in Steam)

Force a **current** Proton first (not only GE-Proton8-32):

1. Steam → Catherine Classic → Compatibility → **Proton Experimental** or **Proton-CachyOS Latest** or **DW-Proton Latest**
2. Launch options: empty (no `PROTON_USE_WINED3D=1`)
3. New Game → first anime cutscene: video + voice audible
4. Logo / title movies play
5. One `movie2` tip video plays

| Proton tool | New Game | Cutscene video | VA audio | Notes |
|-------------|----------|----------------|----------|-------|
| Proton Experimental | _todo_ | _todo_ | _todo_ | Preferred |
| Proton-CachyOS Latest | _todo_ | _todo_ | _todo_ | |
| DW-Proton Latest | _todo_ | _todo_ | _todo_ | |
| GE-Proton8-32 | _todo_ | _todo_ | _todo_ | Fallback only |

If grey/mute on a current Proton after setup:

```bash
~/Work/cath/scripts/reencode-movies.sh
```

Then retest the same Proton before falling back to GE-Proton8-32.

## Restore

```bash
~/Work/cath/scripts/restore-movies.sh   # if re-encode was used
# or delete compatdata/893180 and re-run setup-prefix.sh after one launch
```

### Blurry camera (2026-07-23)
Set VIDEO_DOF=0 VIDEO_BLUR=0 in AppSettings.ini via fix-blurry-camera.sh.
Game was 1440x1080 under gamescope 2560x1080 — match resolutions if softness remains.
