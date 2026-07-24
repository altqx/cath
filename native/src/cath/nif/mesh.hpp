#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace cath {

struct MeshVertex {
  float px, py, pz;
  float nx, ny, nz;
  float u, v;
};

struct Mesh {
  std::string name;
  std::vector<MeshVertex> vertices;
  std::vector<uint32_t> indices;
  bool skinned = false;  // has bone indices; needs NiSkinningMeshModifier to look right
};

struct Model {
  std::vector<Mesh> meshes;
};

}  // namespace cath
