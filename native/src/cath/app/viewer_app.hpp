#pragma once
#include <filesystem>
#include <string>

namespace cath {

struct ViewerOptions {
  std::filesystem::path game_dir;
  std::filesystem::path nif_path;  // absolute or relative to game_dir
  std::filesystem::path shader_dir;
  std::filesystem::path texture_path;  // optional DDS; empty → solid lit albedo
};

int run_viewer(const ViewerOptions& opts);

}  // namespace cath
