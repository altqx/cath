#include "cath/app/viewer_app.hpp"
#include "cath/platform/log.hpp"
#include "cath/platform/paths.hpp"

#include <cstring>
#include <filesystem>
#include <optional>
#include <string>

int main(int argc, char** argv) {
  std::optional<std::string> game_dir;
  std::filesystem::path nif = "data/character/01/c01_00.nif";
  std::filesystem::path shader_dir = "shaders";
  std::filesystem::path texture;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--game-dir") == 0 && i + 1 < argc) {
      game_dir = argv[++i];
    } else if (std::strcmp(argv[i], "--nif") == 0 && i + 1 < argc) {
      nif = argv[++i];
    } else if (std::strcmp(argv[i], "--tex") == 0 && i + 1 < argc) {
      texture = argv[++i];
    } else if (std::strcmp(argv[i], "--shaders") == 0 && i + 1 < argc) {
      shader_dir = argv[++i];
    } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
      CATH_LOG_INFO("Usage: cath-viewer [--game-dir PATH] [--nif REL_OR_ABS] [--tex DDS] [--shaders DIR]");
      return 0;
    }
  }

  cath::ViewerOptions opts;
  opts.game_dir = cath::resolve_game_dir(game_dir);
  opts.nif_path = nif;
  opts.texture_path = texture;
  opts.shader_dir = shader_dir;
  if (opts.shader_dir.is_relative()) {
    std::error_code ec;
    std::filesystem::path exe = std::filesystem::weakly_canonical(argv[0], ec);
    if (!ec) {
      opts.shader_dir = exe.parent_path() / opts.shader_dir;
    }
  }

  CATH_LOG_INFO("game_dir=%s", opts.game_dir.c_str());
  CATH_LOG_INFO("nif=%s", opts.nif_path.c_str());
  CATH_LOG_INFO("shaders=%s", opts.shader_dir.c_str());
  return cath::run_viewer(opts);
}
