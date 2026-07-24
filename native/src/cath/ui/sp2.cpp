#include "cath/ui/sp2.hpp"

#include "cath/platform/log.hpp"

#include <fstream>
#include <cstring>

namespace cath {

bool load_sp2_probe(const std::filesystem::path& path, Sp2Atlas& out, std::string* error) {
  out = {};
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

  // Search for embedded DDS
  for (size_t i = 0; i + 4 < sz; ++i) {
    if (data[i] == 'D' && data[i + 1] == 'D' && data[i + 2] == 'S' && data[i + 3] == ' ') {
      // Write temp decode via memory — reuse load_dds on a temp file path is heavy; inline minimal:
      // For Phase 4 we mark atlas valid with solid placeholder + sprite count estimate.
      break;
    }
  }
  out.image = make_solid_image(256, 256, 30, 30, 40);
  Sp2Sprite s;
  s.name = path.stem().string();
  s.w = 256;
  s.h = 256;
  out.sprites.push_back(s);
  out.valid = true;
  CATH_LOG_INFO("SP2 probe %s (%zu bytes) → %zu sprites", path.filename().c_str(), sz, out.sprites.size());
  return true;
}

}  // namespace cath
