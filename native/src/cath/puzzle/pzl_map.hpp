#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace cath {

enum class BlockType : uint8_t {
  Empty = 0,
  Solid = 1,
  Goal = 2,
  Ice = 3,
  Heavy = 4,
};

struct PuzzleMap {
  int width = 0;
  int height = 0;  // depth in Catherine terms (Y)
  int layers = 0;  // Z height
  int start_x = 0, start_y = 0, start_z = 1;
  int facing = 0;
  std::string source;
  // cells[z][y * width + x]
  std::vector<std::vector<BlockType>> cells;

  BlockType get(int x, int y, int z) const {
    if (x < 0 || y < 0 || z < 0 || x >= width || y >= height || z >= int(cells.size())) {
      return BlockType::Empty;
    }
    return cells[size_t(z)][size_t(y * width + x)];
  }
  void set(int x, int y, int z, BlockType t) {
    if (x < 0 || y < 0 || z < 0 || x >= width || y >= height) {
      return;
    }
    while (int(cells.size()) <= z) {
      cells.emplace_back(size_t(width * height), BlockType::Empty);
    }
    cells[size_t(z)][size_t(y * width + x)] = t;
  }
};

bool load_pzl_map(const std::filesystem::path& path, PuzzleMap& out, std::string* error = nullptr);

// Built-in completable early stages (Golden-path tutorials) when map decode is incomplete.
PuzzleMap make_builtin_stage(int stage_index);

}  // namespace cath
