#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace cath {

// Minimal KF/KFM probe — Phase 2a records tracks; full skinning follows NiSkinningMeshModifier RE.
struct KfTrack {
  std::string name;
  uint32_t key_count = 0;
};

struct KfClip {
  std::string path;
  float duration = 0.f;
  std::vector<KfTrack> tracks;
  bool valid = false;
};

bool load_kf(const std::filesystem::path& path, KfClip& out, std::string* error = nullptr);

}  // namespace cath
