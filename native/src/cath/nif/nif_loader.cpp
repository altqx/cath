#include "cath/nif/nif_loader.hpp"

#include "cath/platform/log.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <vector>

namespace cath {
namespace {

constexpr uint32_t kNifVersion206 = 0x14060000;
constexpr uint32_t kF_UINT16_1 = 0x00010215;
constexpr uint32_t kF_FLOAT32_2 = 0x00020436;
constexpr uint32_t kF_FLOAT32_3 = 0x00030437;
constexpr uint32_t kF_UINT8_4 = 0x00040108;  // blend indices (Catherine skinned meshes)

struct Cursor {
  const uint8_t* data = nullptr;
  size_t size = 0;
  size_t pos = 0;

  bool remain(size_t n) const { return pos + n <= size; }

  template <typename T>
  bool read(T& out) {
    if (!remain(sizeof(T))) {
      return false;
    }
    std::memcpy(&out, data + pos, sizeof(T));
    pos += sizeof(T);
    return true;
  }

  bool read_bytes(std::vector<uint8_t>& out, size_t n) {
    if (!remain(n)) {
      return false;
    }
    out.assign(data + pos, data + pos + n);
    pos += n;
    return true;
  }

  bool skip(size_t n) {
    if (!remain(n)) {
      return false;
    }
    pos += n;
    return true;
  }
};

uint32_t component_size(uint32_t fmt) {
  const uint32_t channels = (fmt >> 16) & 0xffu;
  const uint32_t bpc = (fmt >> 8) & 0xffu;
  return channels * bpc;
}

struct RawStream {
  std::vector<uint32_t> formats;
  std::vector<uint8_t> blob;
  uint32_t num_bytes = 0;
};

bool parse_datastream(const std::vector<uint8_t>& block, RawStream& out) {
  Cursor c{block.data(), block.size(), 0};
  uint32_t num_bytes = 0, cloning = 0, num_regions = 0, num_comp = 0;
  if (!c.read(num_bytes) || !c.read(cloning) || !c.read(num_regions)) {
    return false;
  }
  if (!c.skip(static_cast<size_t>(num_regions) * 8u)) {
    return false;
  }
  if (!c.read(num_comp)) {
    return false;
  }
  out.formats.resize(num_comp);
  for (uint32_t i = 0; i < num_comp; ++i) {
    if (!c.read(out.formats[i])) {
      return false;
    }
  }
  out.num_bytes = num_bytes;
  if (!c.read_bytes(out.blob, num_bytes)) {
    return false;
  }
  return true;
}

bool extract_meshes_from_streams(const std::vector<RawStream>& streams, Model& model) {
  for (size_t i = 0; i < streams.size(); ++i) {
    const auto& s = streams[i];
    if (s.formats.size() != 1 || s.formats[0] != kF_UINT16_1) {
      continue;
    }
    const RawStream* v = nullptr;
    for (size_t j = i + 1; j < streams.size() && j < i + 3; ++j) {
      for (uint32_t f : streams[j].formats) {
        if (f == kF_FLOAT32_3) {
          v = &streams[j];
          break;
        }
      }
      if (v) {
        break;
      }
    }
    if (!v) {
      continue;
    }

    const size_t index_count = s.num_bytes / 2;
    if (index_count < 3 || (index_count % 3) != 0) {
      continue;
    }
    std::vector<uint16_t> idx16(index_count);
    std::memcpy(idx16.data(), s.blob.data(), s.num_bytes);

    uint32_t stride = 0;
    for (uint32_t f : v->formats) {
      stride += component_size(f);
    }
    if (stride == 0 || (v->num_bytes % stride) != 0) {
      continue;
    }
    const uint32_t nverts = v->num_bytes / stride;

    uint16_t max_i = 0;
    for (uint16_t x : idx16) {
      max_i = std::max(max_i, x);
    }
    if (max_i >= nverts) {
      continue;
    }

    std::vector<uint32_t> offsets(v->formats.size());
    uint32_t off = 0;
    for (size_t ci = 0; ci < v->formats.size(); ++ci) {
      offsets[ci] = off;
      off += component_size(v->formats[ci]);
    }

    std::vector<size_t> f3s;
    size_t uv_i = SIZE_MAX;
    for (size_t ci = 0; ci < v->formats.size(); ++ci) {
      if (v->formats[ci] == kF_FLOAT32_3) {
        f3s.push_back(ci);
      }
      if (v->formats[ci] == kF_FLOAT32_2 && uv_i == SIZE_MAX) {
        uv_i = ci;
      }
    }
    if (f3s.empty()) {
      continue;
    }

    // Position = first float3. If UV leads the vertex layout (common on skinned
    // Catherine meshes), f3s[0] is still position. Normals = next float3.
    // Trailing float3 after UINT8_4 are blend weights — not positions.
    size_t pos_i = f3s[0];
    size_t nrm_i = f3s.size() > 1 ? f3s[1] : SIZE_MAX;
    bool has_bones = false;
    for (uint32_t f : v->formats) {
      if (f == kF_UINT8_4) {
        has_bones = true;
        break;
      }
    }

    Mesh mesh;
    mesh.name = "mesh_" + std::to_string(model.meshes.size());
    mesh.skinned = has_bones;
    mesh.vertices.resize(nverts);
    for (uint32_t vi = 0; vi < nverts; ++vi) {
      const uint8_t* base = v->blob.data() + static_cast<size_t>(vi) * stride;
      float pos[3]{};
      float nrm[3]{0.f, 1.f, 0.f};
      float uv[2]{0.f, 0.f};
      std::memcpy(pos, base + offsets[pos_i], sizeof(pos));
      if (nrm_i != SIZE_MAX) {
        std::memcpy(nrm, base + offsets[nrm_i], sizeof(nrm));
      }
      if (uv_i != SIZE_MAX) {
        std::memcpy(uv, base + offsets[uv_i], sizeof(uv));
      }
      mesh.vertices[vi] = {pos[0], pos[1], pos[2], nrm[0], nrm[1], nrm[2], uv[0], uv[1]};
    }
    mesh.indices.reserve(idx16.size());
    for (uint16_t x : idx16) {
      mesh.indices.push_back(x);
    }
    model.meshes.push_back(std::move(mesh));
  }
  return !model.meshes.empty();
}

}  // namespace

bool load_nif(const std::filesystem::path& path, Model& out, std::string* error) {
  out = {};
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    if (error) {
      *error = "failed to open " + path.string();
    }
    return false;
  }
  std::vector<uint8_t> file((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (file.size() < 64) {
    if (error) {
      *error = "file too small";
    }
    return false;
  }

  const char* header = "Gamebryo File Format, Version 20.6.0.0\n";
  if (std::memcmp(file.data(), header, std::strlen(header)) != 0) {
    if (error) {
      *error = "not a Gamebryo 20.6.0.0 NIF";
    }
    return false;
  }

  Cursor c{file.data(), file.size(), std::strlen(header)};
  uint32_t version = 0, user_version = 0, num_blocks = 0;
  uint8_t endian = 0;
  if (!c.read(version) || !c.read(endian) || !c.read(user_version) || !c.read(num_blocks)) {
    if (error) {
      *error = "truncated header";
    }
    return false;
  }
  if (version != kNifVersion206) {
    if (error) {
      *error = "unsupported NIF version";
    }
    return false;
  }

  uint16_t num_block_types = 0;
  if (!c.read(num_block_types)) {
    return false;
  }
  std::vector<std::string> types(num_block_types);
  for (uint16_t i = 0; i < num_block_types; ++i) {
    uint32_t n = 0;
    if (!c.read(n) || !c.remain(n)) {
      return false;
    }
    std::string name(reinterpret_cast<const char*>(c.data + c.pos), n);
    c.pos += n;
    const auto cut = name.find('\x01');
    if (cut != std::string::npos) {
      name.resize(cut);
    }
    types[i] = name;
  }

  std::vector<uint16_t> type_index(num_blocks);
  for (uint32_t i = 0; i < num_blocks; ++i) {
    if (!c.read(type_index[i])) {
      return false;
    }
  }
  std::vector<uint32_t> block_size(num_blocks);
  for (uint32_t i = 0; i < num_blocks; ++i) {
    if (!c.read(block_size[i])) {
      return false;
    }
  }

  uint32_t num_strings = 0, max_string_len = 0;
  if (!c.read(num_strings) || !c.read(max_string_len)) {
    return false;
  }
  for (uint32_t i = 0; i < num_strings; ++i) {
    uint32_t n = 0;
    if (!c.read(n) || !c.skip(n)) {
      return false;
    }
  }
  uint32_t num_groups = 0;
  if (!c.read(num_groups) || !c.skip(static_cast<size_t>(num_groups) * 4u)) {
    return false;
  }

  std::vector<RawStream> streams;
  for (uint32_t i = 0; i < num_blocks; ++i) {
    const uint16_t ti = type_index[i];
    if (ti >= types.size()) {
      return false;
    }
    std::vector<uint8_t> block;
    if (!c.read_bytes(block, block_size[i])) {
      return false;
    }
    if (types[ti] != "NiDataStream") {
      continue;
    }
    RawStream rs;
    if (!parse_datastream(block, rs)) {
      CATH_LOG_WARN("NiDataStream #%u parse failed", i);
      continue;
    }
    streams.push_back(std::move(rs));
  }

  if (!extract_meshes_from_streams(streams, out)) {
    if (error) {
      *error = "no triangle meshes extracted";
    }
    return false;
  }

  CATH_LOG_INFO("loaded %s — %zu meshes from %zu streams", path.filename().c_str(), out.meshes.size(),
                streams.size());
  return true;
}

const Mesh* largest_mesh(const Model& model) {
  const Mesh* best = nullptr;
  for (const auto& m : model.meshes) {
    if (!best || m.indices.size() > best->indices.size()) {
      best = &m;
    }
  }
  return best;
}

const Mesh* best_display_mesh(const Model& model) {
  // Prefer largest unskinned mesh — skinned bind-pose verts look like an explosion
  // until NiSkinningMeshModifier is implemented.
  const Mesh* best_rigid = nullptr;
  const Mesh* best_any = nullptr;
  for (const auto& m : model.meshes) {
    if (!best_any || m.indices.size() > best_any->indices.size()) {
      best_any = &m;
    }
    if (!m.skinned && (!best_rigid || m.indices.size() > best_rigid->indices.size())) {
      best_rigid = &m;
    }
  }
  return best_rigid ? best_rigid : best_any;
}

Model unskinned_model(const Model& model) {
  Model out;
  for (const auto& m : model.meshes) {
    if (!m.skinned) {
      out.meshes.push_back(m);
    }
  }
  return out;
}

}  // namespace cath
