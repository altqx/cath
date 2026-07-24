# Architecture

```
cath (exe)
  └─ game::run_game
       ├─ event::StoryScript (title → movie2 → puzzle → lounge → ending)
       ├─ media::MoviePlayer (ffmpeg)
       ├─ puzzle::PuzzleWorld + pzl_map (PZLe / builtin)
       ├─ ui::Tea / Sp2 (menus, patches)
       ├─ audio::AudioEngine (SDL3 + CPK probe)
       ├─ render::VulkanRenderer (meshes, DDS, movie blit)
       └─ game::SaveGame (~/.local/share/cath/save0.json)
```

## Data root

Unchanged Steam install via `CATH_GAME_DIR`. Native never redistributes Atlus assets.

## Render path

1. Upload merged `Model` or procedural puzzle mesh + `ImageRgba8` (BC DDS or solid).
2. Per-frame UBO MVP; optional `update_texture` for movie frames.
3. Viewer keeps orbit camera; game uses mode-specific cameras.

## Story vertical slice

`make_story_one_ending_script()` hardcodes a Freedom-ending path while BF/EVT bytecode RE continues. Movie opcodes map to `data/movie` and `data/movie2`.
