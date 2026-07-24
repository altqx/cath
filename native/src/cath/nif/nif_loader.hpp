#pragma once
#include "cath/nif/mesh.hpp"

#include <filesystem>
#include <string>

namespace cath {

// Load Gamebryo NIF 20.6.0.0 (Catherine) into mesh IR.
// Extracts triangle meshes from NiDataStream POSITION/INDEX pairs.
bool load_nif(const std::filesystem::path& path, Model& out, std::string* error = nullptr);

// Pick the mesh with the most indices (usually the main body).
const Mesh* largest_mesh(const Model& model);

// Prefer largest rigid (unskinned) mesh — skinned bind-pose looks exploded
// until NiSkinningMeshModifier is implemented.
const Mesh* best_display_mesh(const Model& model);

// Copy only unskinned meshes (safe to merge for multi-mesh draw).
Model unskinned_model(const Model& model);

}  // namespace cath
