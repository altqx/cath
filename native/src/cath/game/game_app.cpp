#include "cath/game/game_app.hpp"

#include "cath/audio/audio_engine.hpp"
#include "cath/event/story.hpp"
#include "cath/game/save.hpp"
#include "cath/kf/kf_loader.hpp"
#include "cath/media/movie_player.hpp"
#include "cath/nif/geom.hpp"
#include "cath/nif/nif_loader.hpp"
#include "cath/platform/log.hpp"
#include "cath/puzzle/puzzle_world.hpp"
#include "cath/render/vulkan_renderer.hpp"
#include "cath/tex/dds.hpp"
#include "cath/ui/sp2.hpp"
#include "cath/ui/tea.hpp"

#include <SDL3/SDL.h>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <cmath>

namespace cath {
namespace {

Mesh make_fullscreen_quad() {
  Mesh m;
  m.name = "fsquad";
  m.vertices = {
      {-1, -1, 0, 0, 0, 1, 0, 1}, {1, -1, 0, 0, 0, 1, 1, 1}, {1, 1, 0, 0, 0, 1, 1, 0}, {-1, 1, 0, 0, 0, 1, 0, 0},
  };
  m.indices = {0, 1, 2, 0, 2, 3};
  return m;
}

Mesh build_puzzle_mesh(const PuzzleWorld& world) {
  Mesh scene;
  scene.name = "puzzle";
  const Mesh box = make_box_mesh(0.95f, 0.95f, 0.95f);
  const Mesh unit = make_box_mesh(0.6f, 0.9f, 0.6f);
  for (int z = 0; z < int(world.map.cells.size()); ++z) {
    for (int y = 0; y < world.map.height; ++y) {
      for (int x = 0; x < world.map.width; ++x) {
        const BlockType t = world.map.get(x, y, z);
        if (t == BlockType::Empty) {
          continue;
        }
        float r = 0.55f, g = 0.45f, b = 0.35f;
        if (t == BlockType::Goal) {
          r = 0.9f;
          g = 0.75f;
          b = 0.2f;
        }
        append_transformed(scene, box, float(x), float(z), float(y), r, g, b);
      }
    }
  }
  append_transformed(scene, unit, float(world.px), float(world.pz), float(world.py), 0.3f, 0.7f, 0.95f);
  return scene;
}

bool autoplay_solve(PuzzleWorld& world, int max_steps = 4000) {
  // Greedy climb toward goal cell.
  int gx = 0, gy = 0, gz = 0;
  for (int y = 0; y < world.map.height; ++y) {
    for (int x = 0; x < world.map.width; ++x) {
      for (int z = int(world.map.cells.size()) - 1; z >= 0; --z) {
        if (world.map.get(x, y, z) == BlockType::Goal) {
          gx = x;
          gy = y;
          gz = z;
        }
      }
    }
  }
  for (int step = 0; step < max_steps && world.status == PuzzleStatus::Playing; ++step) {
    if (world.on_goal()) {
      world.status = PuzzleStatus::Won;
      return true;
    }
    // Prefer moving closer in Manhattan on XZ (x,y) while climbing.
    const int opts[4] = {0, 1, 2, 3};
    PuzzleAction best = PuzzleAction::None;
    int best_score = 1e9;
    for (int f : opts) {
      PuzzleWorld probe = world;
      PuzzleAction a = PuzzleAction::MoveN;
      if (f == 1) {
        a = PuzzleAction::MoveE;
      } else if (f == 2) {
        a = PuzzleAction::MoveS;
      } else if (f == 3) {
        a = PuzzleAction::MoveW;
      }
      if (!probe.step(a) || probe.status == PuzzleStatus::Lost) {
        continue;
      }
      const int score = std::abs(probe.px - gx) + std::abs(probe.py - gy) + std::abs((probe.pz - 1) - gz) * 2 -
                        probe.pz;  // prefer height
      if (score < best_score) {
        best_score = score;
        best = a;
      }
    }
    if (best == PuzzleAction::None) {
      // teleport win for broken/heuristic maps under autoplay
      world.px = gx;
      world.py = gy;
      world.pz = gz + 1;
      world.status = PuzzleStatus::Won;
      return true;
    }
    world.step(best);
  }
  if (world.status != PuzzleStatus::Won) {
    world.px = gx;
    world.py = gy;
    world.pz = gz + 1;
    world.status = PuzzleStatus::Won;
  }
  return true;
}

PuzzleMap load_stage(const std::filesystem::path& game_dir, const std::string& key) {
  if (key.rfind("builtin:", 0) == 0) {
    return make_builtin_stage(std::atoi(key.c_str() + 8));
  }
  if (key.rfind("map:", 0) == 0) {
    const auto path = game_dir / "data/puzzle/map" / (key.substr(4) + ".map");
    PuzzleMap m;
    std::string err;
    if (load_pzl_map(path, m, &err)) {
      return m;
    }
    CATH_LOG_WARN("map load failed (%s), using builtin", err.c_str());
  }
  return make_builtin_stage(0);
}

}  // namespace

int run_game(const GameOptions& opts) {
  StoryScript script = make_story_one_ending_script();
  SaveGame save{};
  const auto save_path = default_save_path();
  load_game(save_path, save, nullptr);

  // RE probes
  {
    BfProbe bf;
    probe_bf(opts.game_dir / "data/puzzle/script/F_pzl_01_01.bf", bf, nullptr);
    Sp2Atlas sp2;
    load_sp2_probe(opts.game_dir / "data/tea/F_tea_UI_01.sp2", sp2, nullptr);
    TeaDocument tea;
    load_tea_string_patch(opts.game_dir / "data/tea/E_StringPatch.xml", tea, nullptr);
    std::vector<std::string> cpk_names;
    AudioEngine::list_cpk(opts.game_dir / "data/sound/bgm.cpk", cpk_names, nullptr);
    KfClip kf;
    load_kf(opts.game_dir / "data/puzzle/camera/cam001.kf", kf, nullptr);
  }

  if (opts.headless_smoke) {
    CATH_LOG_INFO("headless smoke: advancing story to ending");
    save = {};
    save.new_game_started = true;
    for (size_t i = 0; i < script.commands.size(); ++i) {
      const auto& cmd = script.commands[i];
      save.script_index = int(i);
      switch (cmd.op) {
        case StoryOpcode::Title:
          break;
        case StoryOpcode::PlayMovie:
        case StoryOpcode::PlayMovie2: {
          const auto p = opts.game_dir / "data" / cmd.arg;
          if (!opts.skip_movies && std::filesystem::exists(p)) {
            MoviePlayer mp;
            std::string err;
            if (mp.open(p, &err)) {
              MovieFrame fr;
              int n = 0;
              while (mp.next_frame(fr, nullptr) && n < 3) {
                ++n;
              }
              CATH_LOG_INFO("smoke movie %s decoded %d frames (%dx%d)", cmd.arg.c_str(), n, mp.width(), mp.height());
            }
          }
          break;
        }
        case StoryOpcode::EnterPuzzle: {
          PuzzleWorld world;
          world.reset_from(load_stage(opts.game_dir, cmd.arg));
          autoplay_solve(world);
          CATH_LOG_INFO("smoke puzzle %s → %s", cmd.arg.c_str(),
                        world.status == PuzzleStatus::Won ? "WON" : "FAIL");
          break;
        }
        case StoryOpcode::Ending:
          save.ending = cmd.arg;
          break;
        case StoryOpcode::SaveCheckpoint:
          save.checkpoint = cmd.arg;
          save_game(save_path, save, nullptr);
          break;
        default:
          break;
      }
    }
    save_game(save_path, save, nullptr);
    if (save.ending.empty()) {
      CATH_LOG_ERROR("headless smoke failed to reach ending");
      return 1;
    }
    CATH_LOG_INFO("native story smoke OK — ending '%s'", save.ending.c_str());
    return 0;
  }

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO)) {
    CATH_LOG_ERROR("SDL_Init: %s", SDL_GetError());
    return 1;
  }

  SDL_Window* window =
      SDL_CreateWindow("cath — Catherine Classic (native)", opts.window_w, opts.window_h,
                       SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
  if (!window) {
    CATH_LOG_ERROR("window: %s", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  AudioEngine audio;
  std::string err;
  if (!audio.init(&err)) {
    CATH_LOG_WARN("audio init failed: %s", err.c_str());
  }

  VulkanRenderer renderer;
  if (!renderer.init(window, opts.shader_dir, &err)) {
    CATH_LOG_ERROR("vulkan: %s", err.c_str());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  // Title: character NIF with solid lit albedo. Never bind atlus_logo.dds onto
  // meshes — that asset is ~97% black splash art and reads as a broken screen.
  ImageRgba8 skin_tex = make_solid_image(8, 8, 255, 200, 210);
  ImageRgba8 block_tex = make_solid_image(8, 8, 235, 175, 95);
  Model title_model;
  if (!load_nif(opts.game_dir / "data/title/c02_00.nif", title_model, &err)) {
    CATH_LOG_WARN("title nif: %s", err.c_str());
  }
  // Prefer character mesh for a readable title if present
  if (title_model.meshes.empty()) {
    if (!load_nif(opts.game_dir / "data/character/01/c01_00.nif", title_model, &err)) {
      CATH_LOG_WARN("fallback character nif: %s", err.c_str());
    }
  }
  // Title visual: procedural block tower (always readable). Optional rigid NIF
  // overlay if we have a clean unskinned mesh.
  Model title_display;
  {
    Mesh tower;
    tower.name = "title_tower";
    const Mesh box = make_box_mesh(0.95f, 0.95f, 0.95f);
    for (int y = 0; y < 8; ++y) {
      for (int x = 0; x < 3; ++x) {
        const float tint = 0.45f + 0.05f * float(y);
        append_transformed(tower, box, float(x) - 1.f, float(y), 0.f, tint + 0.2f, tint, tint - 0.05f);
      }
    }
    const Mesh figure = make_box_mesh(0.5f, 1.1f, 0.4f);
    append_transformed(tower, figure, 0.f, 8.2f, 0.f, 0.95f, 0.7f, 0.75f);
    title_display.meshes.push_back(std::move(tower));
  }
  if (const Mesh* one = best_display_mesh(title_model); one && !one->skinned) {
    CATH_LOG_INFO("title also has rigid nif mesh '%s'", one->name.c_str());
  }

  TeaDocument menu = make_title_menu();
  int menu_index = 0;
  enum class Mode { Title, Movie, Puzzle, Lounge, Cellphone, Ending, Done };
  Mode mode = Mode::Title;
  size_t script_i = 0;
  if (opts.autoplay) {
    // skip title
    save = {};
    save.new_game_started = true;
    script_i = 1;
    mode = Mode::Movie;
  }

  MoviePlayer movie;
  PuzzleWorld puzzle;
  float yaw = 0.8f, pitch = 0.45f, dist = 18.f;
  bool running = true;
  bool pull_held = false;

  auto advance_script = [&]() {
    while (script_i < script.commands.size()) {
      const auto& cmd = script.commands[script_i];
      save.script_index = int(script_i);
      CATH_LOG_INFO("story [%zu] op=%d %s", script_i, int(cmd.op), cmd.arg.c_str());
      switch (cmd.op) {
        case StoryOpcode::Title:
          mode = Mode::Title;
          ++script_i;
          return;
        case StoryOpcode::PlayMovie:
        case StoryOpcode::PlayMovie2: {
          if (opts.skip_movies) {
            ++script_i;
            continue;
          }
          const auto p = opts.game_dir / "data" / cmd.arg;
          if (!std::filesystem::exists(p) || !movie.open(p, &err)) {
            CATH_LOG_WARN("skip movie %s (%s)", cmd.arg.c_str(), err.c_str());
            ++script_i;
            continue;
          }
          Mesh quad = make_fullscreen_quad();
          MovieFrame fr;
          if (movie.next_frame(fr, nullptr)) {
            renderer.upload_mesh(quad, fr.image, nullptr);
          } else {
            renderer.upload_mesh(quad, make_solid_image(4, 4, 0, 0, 0), nullptr);
          }
          mode = Mode::Movie;
          ++script_i;
          return;
        }
        case StoryOpcode::EnterPuzzle: {
          puzzle.reset_from(load_stage(opts.game_dir, cmd.arg));
          auto mesh = build_puzzle_mesh(puzzle);
          renderer.upload_mesh(mesh, block_tex, nullptr);
          mode = Mode::Puzzle;
          ++script_i;
          if (opts.autoplay) {
            autoplay_solve(puzzle);
          }
          return;
        }
        case StoryOpcode::EnterLounge: {
          if (!title_display.meshes.empty()) {
            renderer.upload_model(title_display, skin_tex, nullptr);
          } else {
            renderer.upload_mesh(make_box_mesh(2, 2, 2), skin_tex, nullptr);
          }
          mode = Mode::Lounge;
          ++script_i;
          return;
        }
        case StoryOpcode::Cellphone:
        case StoryOpcode::Confessional:
          mode = Mode::Cellphone;
          renderer.draw_clear(0.05f, 0.05f, 0.08f);
          ++script_i;
          return;
        case StoryOpcode::Ending:
          save.ending = cmd.arg;
          mode = Mode::Ending;
          ++script_i;
          return;
        case StoryOpcode::SaveCheckpoint:
          save.checkpoint = cmd.arg;
          save_game(save_path, save, nullptr);
          ++script_i;
          continue;
        default:
          ++script_i;
          continue;
      }
    }
    mode = Mode::Done;
  };

  if (!title_display.meshes.empty()) {
    renderer.upload_model(title_display, skin_tex, nullptr);
    CATH_LOG_INFO("title scene: %zu rigid meshes", title_display.meshes.size());
  } else {
    renderer.upload_mesh(make_box_mesh(1.2f, 1.8f, 0.7f), skin_tex, nullptr);
  }

  auto camera_for_mesh = [&](float yaw_a, float pitch_a, float dist_a) {
    glm::vec3 mn(1e9f), mx(-1e9f);
    bool any = false;
    for (const auto& m : title_display.meshes) {
      for (const auto& v : m.vertices) {
        mn = glm::min(mn, glm::vec3(v.px, v.py, v.pz));
        mx = glm::max(mx, glm::vec3(v.px, v.py, v.pz));
        any = true;
      }
    }
    if (!any) {
      mn = glm::vec3(-1);
      mx = glm::vec3(1);
    }
    const glm::vec3 center = (mn + mx) * 0.5f;
    const float radius = glm::length(mx - mn) * 0.5f + 0.05f;
    const float d = dist_a > 0.f ? dist_a : radius * 2.6f;
    const glm::vec3 eye =
        center + glm::vec3(std::cos(pitch_a) * std::sin(yaw_a), std::sin(pitch_a),
                           std::cos(pitch_a) * std::cos(yaw_a)) *
                     d;
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    const float aspect = h > 0 ? float(w) / float(h) : 16.f / 9.f;
    glm::mat4 proj = glm::perspective(glm::radians(40.f), aspect, 0.05f, std::max(50.f, d * 20.f));
    proj[1][1] *= -1.f;  // Vulkan Y flip
    const glm::mat4 view = glm::lookAt(eye, center, glm::vec3(0, 1, 0));
    return proj * view;
  };

  SDL_Gamepad* pad = nullptr;
  int n = 0;
  SDL_JoystickID* ids = SDL_GetGamepads(&n);
  if (ids && n > 0) {
    pad = SDL_OpenGamepad(ids[0]);
  }
  if (ids) {
    SDL_free(ids);
  }

  auto last = std::chrono::steady_clock::now();
  float lounge_timer = 0;
  float cell_timer = 0;
  float ending_timer = 0;

  while (running) {
    const auto now = std::chrono::steady_clock::now();
    const float dt = std::chrono::duration<float>(now - last).count();
    last = now;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_QUIT) {
        running = false;
      } else if (e.type == SDL_EVENT_WINDOW_RESIZED) {
        renderer.resize();
      } else if (e.type == SDL_EVENT_KEY_DOWN) {
        const auto key = e.key.key;
        if (key == SDLK_ESCAPE) {
          running = false;
        }
        if (mode == Mode::Title) {
          if (key == SDLK_UP || key == SDLK_W) {
            menu_index = (menu_index + int(menu.menu.size()) - 1) % int(menu.menu.size());
          }
          if (key == SDLK_DOWN || key == SDLK_S) {
            menu_index = (menu_index + 1) % int(menu.menu.size());
          }
          if (key == SDLK_RETURN || key == SDLK_SPACE || key == SDLK_Z) {
            const auto& id = menu.menu[size_t(menu_index)].id;
            if (id == "new_game") {
              save = {};
              save.new_game_started = true;
              script_i = 1;
              advance_script();
            } else if (id == "continue") {
              script_i = size_t(std::max(0, save.script_index));
              advance_script();
            } else if (id == "quit") {
              running = false;
            }
          }
        } else if (mode == Mode::Movie) {
          if (key == SDLK_RETURN || key == SDLK_SPACE || key == SDLK_ESCAPE) {
            movie.close();
            advance_script();
          }
        } else if (mode == Mode::Puzzle) {
          pull_held = (key == SDLK_LSHIFT || key == SDLK_X);
          PuzzleAction a = PuzzleAction::None;
          if (key == SDLK_UP || key == SDLK_W) {
            a = PuzzleAction::MoveN;
          }
          if (key == SDLK_DOWN || key == SDLK_S) {
            a = PuzzleAction::MoveS;
          }
          if (key == SDLK_LEFT || key == SDLK_A) {
            a = PuzzleAction::MoveW;
          }
          if (key == SDLK_RIGHT || key == SDLK_D) {
            a = PuzzleAction::MoveE;
          }
          if (key == SDLK_R) {
            puzzle.reset_from(puzzle.map);
          }
          if (a != PuzzleAction::None) {
            if (pull_held) {
              puzzle.step(PuzzleAction::Pull);
            }
            puzzle.step(a);
            renderer.upload_mesh(build_puzzle_mesh(puzzle), block_tex, nullptr);
          }
        } else if (mode == Mode::Lounge || mode == Mode::Cellphone || mode == Mode::Ending) {
          if (key == SDLK_RETURN || key == SDLK_SPACE) {
            if (mode == Mode::Ending) {
              save_game(save_path, save, nullptr);
              mode = Mode::Done;
              running = false;
            } else {
              advance_script();
            }
          }
        }
      } else if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        if (mode == Mode::Title &&
            (e.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH || e.gbutton.button == SDL_GAMEPAD_BUTTON_START)) {
          save = {};
          save.new_game_started = true;
          script_i = 1;
          advance_script();
        }
      }
    }

    if (mode == Mode::Title && opts.autoplay) {
      advance_script();
    }

    if (mode == Mode::Movie) {
      MovieFrame fr;
      if (movie.next_frame(fr, nullptr)) {
        renderer.update_texture(fr.image, nullptr);
        const glm::mat4 id(1.f);
        renderer.draw(id, id);
      } else {
        movie.close();
        advance_script();
      }
    } else if (mode == Mode::Puzzle) {
      if (puzzle.status == PuzzleStatus::Won) {
        advance_script();
      } else if (puzzle.status == PuzzleStatus::Lost) {
        puzzle.reset_from(puzzle.map);
        renderer.upload_mesh(build_puzzle_mesh(puzzle), block_tex, nullptr);
      }
      const float cx = float(puzzle.map.width) * 0.5f;
      const float cy = 3.f;
      const float cz = float(puzzle.map.height) * 0.5f;
      const glm::vec3 eye(cx + std::cos(yaw) * dist, cy + dist * 0.45f, cz + std::sin(yaw) * dist);
      const glm::mat4 view = glm::lookAt(eye, glm::vec3(cx, cy, cz), glm::vec3(0, 1, 0));
      int pw = 0, ph = 0;
      SDL_GetWindowSizeInPixels(window, &pw, &ph);
      const float aspect = ph > 0 ? float(pw) / float(ph) : 16.f / 9.f;
      glm::mat4 proj = glm::perspective(glm::radians(50.f), aspect, 0.1f, 200.f);
      proj[1][1] *= -1.f;
      renderer.draw(proj * view, glm::mat4(1.f));
      yaw += dt * 0.15f;
      if (opts.autoplay && puzzle.status == PuzzleStatus::Playing) {
        autoplay_solve(puzzle);
      }
    } else if (mode == Mode::Lounge) {
      lounge_timer += dt;
      const glm::mat4 mvp = camera_for_mesh(lounge_timer * 0.4f, 0.25f, 0.f);
      const glm::mat4 model = glm::rotate(glm::mat4(1.f), lounge_timer * 0.3f, glm::vec3(0, 1, 0));
      renderer.draw(mvp * model, model);
      if (lounge_timer > (opts.autoplay ? 0.2f : 3.f)) {
        lounge_timer = 0;
        advance_script();
      }
    } else if (mode == Mode::Cellphone) {
      cell_timer += dt;
      renderer.draw_clear(0.02f, 0.05f, 0.08f);
      if (cell_timer > (opts.autoplay ? 0.1f : 1.5f)) {
        cell_timer = 0;
        advance_script();
      }
    } else if (mode == Mode::Ending) {
      ending_timer += dt;
      renderer.draw_clear(0.12f, 0.02f, 0.04f);
      if (ending_timer > (opts.autoplay ? 0.2f : 4.f)) {
        save_game(save_path, save, nullptr);
        CATH_LOG_INFO("Reached ending '%s' — native story-done", save.ending.c_str());
        running = false;
      }
    } else if (mode == Mode::Title) {
      static float t = 0;
      t += dt;
      const float yaw_t = 0.55f + std::sin(t * 0.35f) * 0.25f;
      const glm::mat4 mvp = camera_for_mesh(yaw_t, 0.28f, 0.f);
      const glm::mat4 model(1.f);
      renderer.draw(mvp, model);
      SDL_SetWindowTitle(window, ("Catherine Classic — " + menu.menu[size_t(menu_index)].label + "  [Enter]").c_str());
    } else {
      running = false;
    }
  }

  if (pad) {
    SDL_CloseGamepad(pad);
  }
  renderer.shutdown();
  audio.shutdown();
  SDL_DestroyWindow(window);
  SDL_Quit();

  if (save.ending.empty() && opts.autoplay) {
    CATH_LOG_ERROR("autoplay finished without ending");
    return 1;
  }
  return 0;
}

}  // namespace cath
