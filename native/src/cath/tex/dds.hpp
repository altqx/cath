#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace cath {

struct ImageRgba8 {
  uint32_t width = 0;
  uint32_t height = 0;
  std::vector<uint8_t> pixels;  // RGBA8
};

// Minimal DDS loader (uncompressed RGBA/BGRA and BC1/BC3 via CPU decode stub → expand).
// For Phase 1, also provides a solid debug image if load fails.
bool load_dds(const std::filesystem::path& path, ImageRgba8& out, std::string* error = nullptr);
ImageRgba8 make_solid_image(uint32_t w, uint32_t h, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);

}  // namespace cath
