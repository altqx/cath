#pragma once
#include "cath/tex/dds.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace cath {

struct Sp2Sprite {
  std::string name;
  uint32_t x = 0, y = 0, w = 0, h = 0;
};

struct Sp2Atlas {
  ImageRgba8 image;
  std::vector<Sp2Sprite> sprites;
  bool valid = false;
};

// SP2: Atlus sprite pack. Probe header + embed first DDS/texture if present.
bool load_sp2_probe(const std::filesystem::path& path, Sp2Atlas& out, std::string* error = nullptr);

}  // namespace cath
