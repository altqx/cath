#include "cath/kf/kf_loader.hpp"

#include <fstream>
#include <cstring>

namespace cath {

bool load_kf(const std::filesystem::path& path, KfClip& out, std::string* error) {
  out = {};
  out.path = path.string();
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    if (error) {
      *error = "open failed";
    }
    return false;
  }
  std::string header(40, '\0');
  in.read(header.data(), 40);
  // Gamebryo KF starts with version string like "Gamebryo File Format, Version 20.6.0.0"
  if (header.find("Gamebryo") == std::string::npos && header.find("NetImmerse") == std::string::npos) {
    // still accept; some KF are binary-only after short header
  }
  in.seekg(0, std::ios::end);
  const auto sz = in.tellg();
  out.duration = 1.f;
  out.valid = sz > 64;
  // Placeholder track list from filename until full NiTransformInterpolator parse lands.
  KfTrack t;
  t.name = path.stem().string();
  t.key_count = 1;
  out.tracks.push_back(t);
  return out.valid;
}

}  // namespace cath
