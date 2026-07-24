#include "cath/puzzle/pzl_map.hpp"

#include "cath/platform/log.hpp"

#include <fstream>
#include <cstring>

namespace cath {
namespace {

bool is_block_marker(uint8_t b) {
  // Observed solid markers in PZLe column records (PS3-era BE layout leftovers).
  return b == 0x11 || b == 0x10 || b == 0x15 || b == 0x21 || b == 0x01;
}

}  // namespace

bool load_pzl_map(const std::filesystem::path& path, PuzzleMap& out, std::string* error) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in) {
    if (error) {
      *error = "open failed";
    }
    return false;
  }
  const auto sz = size_t(in.tellg());
  in.seekg(0);
  std::vector<uint8_t> data(sz);
  in.read(reinterpret_cast<char*>(data.data()), std::streamsize(sz));
  if (sz < 0x100 || std::memcmp(data.data(), "PZLe", 4) != 0) {
    if (error) {
      *error = "not PZLe";
    }
    return false;
  }
  out = {};
  out.source = path.string();
  out.width = data[12];
  out.height = data[13];
  out.layers = data[14];
  if (out.width <= 0 || out.height <= 0 || out.width > 64 || out.height > 64) {
    if (error) {
      *error = "bad dimensions";
    }
    return false;
  }
  out.start_x = data[0x38];
  out.start_y = data[0x39];
  out.start_z = data[0x3a];
  out.facing = data[0x3b];
  if (out.start_z < 1) {
    out.start_z = 1;
  }

  constexpr size_t kColStart = 0xE0;
  constexpr size_t kRec = 136;
  constexpr size_t kLayerBytes = 17;
  const size_t cols = size_t(out.width * out.height);
  const size_t need = kColStart + cols * kRec;
  if (sz < need) {
    CATH_LOG_WARN("PZLe %s shorter than column table (%zu < %zu); partial load", path.filename().c_str(), sz, need);
  }

  int max_z = out.layers > 0 ? out.layers : 8;
  if (max_z < 8) {
    max_z = 8;
  }
  out.cells.assign(size_t(max_z), std::vector<BlockType>(cols, BlockType::Empty));

  for (size_t col = 0; col < cols; ++col) {
    const size_t base = kColStart + col * kRec;
    if (base + kRec > sz) {
      break;
    }
    const int x = int(col % size_t(out.width));
    const int y = int(col / size_t(out.width));
    for (int L = 0; L < 8; ++L) {
      const uint8_t* layer = data.data() + base + size_t(L) * kLayerBytes;
      bool solid = false;
      for (int i = 0; i < 8; ++i) {
        if (is_block_marker(layer[i])) {
          solid = true;
          break;
        }
      }
      if (solid) {
        out.set(x, y, L, BlockType::Solid);
      }
    }
  }

  // Ensure a floor under the start column so stages remain enterable.
  if (out.get(out.start_x, out.start_y, 0) == BlockType::Empty) {
    out.set(out.start_x, out.start_y, 0, BlockType::Solid);
  }
  // Place a goal marker on the highest solid block.
  int gx = out.start_x, gy = out.start_y, gz = 0;
  for (int y = 0; y < out.height; ++y) {
    for (int x = 0; x < out.width; ++x) {
      for (int z = max_z - 1; z >= 0; --z) {
        if (out.get(x, y, z) != BlockType::Empty) {
          if (z > gz) {
            gz = z;
            gx = x;
            gy = y;
          }
          break;
        }
      }
    }
  }
  out.set(gx, gy, gz, BlockType::Goal);
  CATH_LOG_INFO("PZLe %s %dx%dx%d start=(%d,%d,%d) goal~(%d,%d,%d)", path.filename().c_str(), out.width, out.height,
                max_z, out.start_x, out.start_y, out.start_z, gx, gy, gz);
  return true;
}

PuzzleMap make_builtin_stage(int stage_index) {
  PuzzleMap m;
  m.source = "builtin:" + std::to_string(stage_index);
  // Small staircase towers — Stage 1–3 completable under native.
  if (stage_index <= 0) {
    m.width = 5;
    m.height = 5;
    m.layers = 6;
    m.start_x = 2;
    m.start_y = 0;
    m.start_z = 1;
    m.cells.assign(6, std::vector<BlockType>(25, BlockType::Empty));
    for (int x = 0; x < 5; ++x) {
      m.set(x, 0, 0, BlockType::Solid);
    }
    m.set(2, 1, 0, BlockType::Solid);
    m.set(2, 1, 1, BlockType::Solid);
    m.set(2, 2, 0, BlockType::Solid);
    m.set(2, 2, 1, BlockType::Solid);
    m.set(2, 2, 2, BlockType::Solid);
    m.set(2, 3, 0, BlockType::Solid);
    m.set(2, 3, 1, BlockType::Solid);
    m.set(2, 3, 2, BlockType::Solid);
    m.set(2, 3, 3, BlockType::Solid);
    m.set(2, 4, 0, BlockType::Solid);
    m.set(2, 4, 1, BlockType::Solid);
    m.set(2, 4, 2, BlockType::Solid);
    m.set(2, 4, 3, BlockType::Solid);
    m.set(2, 4, 4, BlockType::Goal);
    // pushable block to fill a gap
    m.set(1, 1, 1, BlockType::Solid);
  } else if (stage_index == 1) {
    m.width = 6;
    m.height = 6;
    m.layers = 8;
    m.start_x = 1;
    m.start_y = 0;
    m.start_z = 1;
    m.cells.assign(8, std::vector<BlockType>(36, BlockType::Empty));
    for (int x = 0; x < 6; ++x) {
      m.set(x, 0, 0, BlockType::Solid);
    }
    for (int y = 0; y < 5; ++y) {
      for (int z = 0; z <= y; ++z) {
        m.set(3, y + 1, z, BlockType::Solid);
      }
    }
    m.set(3, 5, 5, BlockType::Goal);
    m.set(2, 1, 1, BlockType::Solid);
    m.set(4, 2, 2, BlockType::Solid);
  } else {
    m.width = 7;
    m.height = 7;
    m.layers = 8;
    m.start_x = 3;
    m.start_y = 0;
    m.start_z = 1;
    m.cells.assign(8, std::vector<BlockType>(49, BlockType::Empty));
    for (int x = 0; x < 7; ++x) {
      m.set(x, 0, 0, BlockType::Solid);
    }
    // Spiral ascending
    const int path[][2] = {{3, 1}, {3, 2}, {4, 2}, {5, 2}, {5, 3}, {5, 4}, {4, 4}, {3, 4}, {2, 4}, {2, 5}, {3, 5}, {4, 5}};
    for (size_t i = 0; i < sizeof(path) / sizeof(path[0]); ++i) {
      for (size_t z = 0; z <= i / 2 + 1; ++z) {
        m.set(path[i][0], path[i][1], int(z), BlockType::Solid);
      }
    }
    m.set(4, 5, 6, BlockType::Goal);
    m.set(1, 1, 1, BlockType::Solid);
    m.set(2, 2, 2, BlockType::Solid);
  }
  return m;
}

}  // namespace cath
