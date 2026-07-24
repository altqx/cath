#include "cath/game/game_app.hpp"
#include "cath/platform/log.hpp"
#include "cath/platform/paths.hpp"

#include <cstring>
#include <filesystem>
#include <optional>
#include <string>

int main(int argc, char** argv) {
  std::optional<std::string> game_dir_cli;
  cath::GameOptions opts;
  opts.shader_dir = std::filesystem::path(argv[0]).parent_path() / "shaders";
  if (!std::filesystem::exists(opts.shader_dir)) {
    opts.shader_dir = std::filesystem::current_path() / "shaders";
  }

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--game-dir") == 0 && i + 1 < argc) {
      game_dir_cli = argv[++i];
    } else if (std::strcmp(argv[i], "--skip-movies") == 0) {
      opts.skip_movies = true;
    } else if (std::strcmp(argv[i], "--autoplay") == 0) {
      opts.autoplay = true;
    } else if (std::strcmp(argv[i], "--smoke") == 0) {
      opts.headless_smoke = true;
      opts.autoplay = true;
    } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
      CATH_LOG_INFO("Usage: cath [--game-dir DIR] [--skip-movies] [--autoplay] [--smoke]");
      return 0;
    }
  }
  opts.game_dir = cath::resolve_game_dir(game_dir_cli);

  CATH_LOG_INFO("cath native — game_dir=%s", opts.game_dir.string().c_str());
  return cath::run_game(opts);
}
