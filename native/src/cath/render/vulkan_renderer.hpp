#pragma once
#include "cath/nif/mesh.hpp"
#include "cath/tex/dds.hpp"

#include <SDL3/SDL.h>
#include <glm/mat4x4.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace cath {

class VulkanRenderer {
 public:
  VulkanRenderer() = default;
  ~VulkanRenderer();

  VulkanRenderer(const VulkanRenderer&) = delete;
  VulkanRenderer& operator=(const VulkanRenderer&) = delete;

  bool init(SDL_Window* window, const std::filesystem::path& shader_dir, std::string* error = nullptr);
  void shutdown();

  bool upload_mesh(const Mesh& mesh, const ImageRgba8& image, std::string* error = nullptr);
  // Merge all meshes into one draw (Phase 2 multi-mesh).
  bool upload_model(const Model& model, const ImageRgba8& image, std::string* error = nullptr);
  bool update_texture(const ImageRgba8& image, std::string* error = nullptr);
  void clear_geometry();

  void resize();
  void draw(const glm::mat4& mvp, const glm::mat4& model);
  void draw_clear(float r, float g, float b);

  bool ready() const { return ready_; }
  uint32_t index_count() const;

 private:
  struct Impl;
  Impl* impl_ = nullptr;
  bool ready_ = false;

  void destroy_mesh_resources();
  bool create_texture_from_image(const ImageRgba8& image, std::string* error);
  bool upload_geometry(const Mesh& mesh, std::string* error);
};

}  // namespace cath
