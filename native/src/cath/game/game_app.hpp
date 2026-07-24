#pragma once
#include <filesystem>
#include <string>

namespace cath {

struct GameOptions {
  std::filesystem::path game_dir;
  std::filesystem::path shader_dir;
  bool skip_movies = false;
  bool autoplay = false;   // solve puzzles + advance story to ending
  bool headless_smoke = false;  // no window; run story logic only
  int window_w = 1280;
  int window_h = 720;
};

int run_game(const GameOptions& opts);

}  // namespace cath
