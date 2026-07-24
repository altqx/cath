#pragma once
#include "cath/nif/mesh.hpp"

#include <glm/vec3.hpp>

namespace cath {

inline Mesh make_box_mesh(float sx, float sy, float sz, float u_scale = 1.f) {
  Mesh m;
  m.name = "box";
  const float hx = sx * 0.5f, hy = sy * 0.5f, hz = sz * 0.5f;
  const MeshVertex corners[8] = {
      {-hx, -hy, -hz, 0, 0, 0, 0, 0}, {hx, -hy, -hz, 0, 0, 0, u_scale, 0},
      {hx, hy, -hz, 0, 0, 0, u_scale, u_scale}, {-hx, hy, -hz, 0, 0, 0, 0, u_scale},
      {-hx, -hy, hz, 0, 0, 0, 0, 0},  {hx, -hy, hz, 0, 0, 0, u_scale, 0},
      {hx, hy, hz, 0, 0, 0, u_scale, u_scale},  {-hx, hy, hz, 0, 0, 0, 0, u_scale},
  };
  const int faces[6][4] = {{0, 1, 2, 3}, {4, 7, 6, 5}, {0, 4, 5, 1}, {2, 6, 7, 3}, {0, 3, 7, 4}, {1, 5, 6, 2}};
  const glm::vec3 norms[6] = {{0, 0, -1}, {0, 0, 1}, {0, -1, 0}, {0, 1, 0}, {-1, 0, 0}, {1, 0, 0}};
  for (int f = 0; f < 6; ++f) {
    const uint32_t base = uint32_t(m.vertices.size());
    for (int i = 0; i < 4; ++i) {
      MeshVertex v = corners[faces[f][i]];
      v.nx = norms[f].x;
      v.ny = norms[f].y;
      v.nz = norms[f].z;
      m.vertices.push_back(v);
    }
    m.indices.insert(m.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
  }
  return m;
}

inline void append_transformed(Mesh& dst, const Mesh& src, float ox, float oy, float oz, float r, float g, float b) {
  const uint32_t base = uint32_t(dst.vertices.size());
  for (auto v : src.vertices) {
    v.px += ox;
    v.py += oy;
    v.pz += oz;
    // Keep UVs in 0..1 so solid textures sample stably; encode tint in normal length bias unused.
    // Slight normal tweak per-tint keeps faces from looking perfectly flat-identical.
    v.nx = v.nx * 0.9f + (r - 0.5f) * 0.1f;
    v.ny = v.ny * 0.9f + (g - 0.5f) * 0.1f;
    v.nz = v.nz * 0.9f + (b - 0.5f) * 0.1f;
    dst.vertices.push_back(v);
  }
  for (uint32_t idx : src.indices) {
    dst.indices.push_back(base + idx);
  }
}

}  // namespace cath
