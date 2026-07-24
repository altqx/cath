#include "cath/app/viewer_app.hpp"

#include "cath/nif/nif_loader.hpp"
#include "cath/platform/log.hpp"
#include "cath/render/vulkan_renderer.hpp"
#include "cath/tex/dds.hpp"

#include <SDL3/SDL.h>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace cath {

int run_viewer(const ViewerOptions& opts) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    CATH_LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
    return 1;
  }

  SDL_Window* window = SDL_CreateWindow("cath-viewer", 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
  if (!window) {
    CATH_LOG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  std::filesystem::path nif = opts.nif_path;
  if (nif.is_relative()) {
    nif = opts.game_dir / nif;
  }

  Model model;
  std::string err;
  if (!load_nif(nif, model, &err)) {
    CATH_LOG_ERROR("NIF load failed: %s", err.c_str());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  if (model.meshes.empty()) {
    CATH_LOG_ERROR("no mesh in model");
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  size_t total_v = 0, total_i = 0;
  for (const auto& m : model.meshes) {
    total_v += m.vertices.size();
    total_i += m.indices.size();
  }
  CATH_LOG_INFO("displaying %zu meshes (%zu verts, %zu indices)", model.meshes.size(), total_v, total_i);

  // Prefer a solid lit albedo. Do NOT bind title splash DDS (mostly black) onto
  // character meshes — that produces a black screen with blue logo scraps.
  ImageRgba8 image = make_solid_image(8, 8, 220, 160, 175);
  if (!opts.texture_path.empty()) {
    std::filesystem::path tex = opts.texture_path;
    if (tex.is_relative()) {
      tex = opts.game_dir / tex;
    }
    if (load_dds(tex, image, &err)) {
      CATH_LOG_INFO("texture %s (%ux%u)", tex.filename().c_str(), image.width, image.height);
    } else {
      CATH_LOG_WARN("texture load failed (%s); using solid", err.c_str());
      image = make_solid_image(8, 8, 220, 160, 175);
    }
  }

  VulkanRenderer renderer;
  if (!renderer.init(window, opts.shader_dir, &err)) {
    CATH_LOG_ERROR("Vulkan init failed: %s", err.c_str());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  // Single best rigid mesh — merging parts without NiNode transforms looks broken.
  const Mesh* display_mesh = best_display_mesh(model);
  if (!display_mesh) {
    CATH_LOG_ERROR("no displayable mesh");
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  if (display_mesh->skinned) {
    CATH_LOG_WARN("showing skinned bind-pose mesh '%s' (skinning not implemented yet)",
                  display_mesh->name.c_str());
  } else {
    CATH_LOG_INFO("displaying rigid mesh '%s' (%zu verts, %zu indices)", display_mesh->name.c_str(),
                  display_mesh->vertices.size(), display_mesh->indices.size());
  }
  if (!renderer.upload_mesh(*display_mesh, image, &err)) {
    CATH_LOG_ERROR("upload failed: %s", err.c_str());
    renderer.shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  // Center camera on mesh bounds
  glm::vec3 mn(1e9f), mx(-1e9f);
  for (const auto& v : display_mesh->vertices) {
    mn = glm::min(mn, glm::vec3(v.px, v.py, v.pz));
    mx = glm::max(mx, glm::vec3(v.px, v.py, v.pz));
  }
  const glm::vec3 center = (mn + mx) * 0.5f;
  const float radius = glm::length(mx - mn) * 0.5f + 0.01f;

  float yaw = 0.6f;
  float pitch = 0.35f;
  float dist = radius * 2.8f;
  bool dragging = false;
  float last_x = 0, last_y = 0;

  bool running = true;
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_QUIT) {
        running = false;
      } else if (e.type == SDL_EVENT_WINDOW_RESIZED) {
        renderer.resize();
      } else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
        dragging = true;
        last_x = e.button.x;
        last_y = e.button.y;
      } else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT) {
        dragging = false;
      } else if (e.type == SDL_EVENT_MOUSE_MOTION && dragging) {
        yaw += (e.motion.x - last_x) * 0.01f;
        pitch += (e.motion.y - last_y) * 0.01f;
        pitch = glm::clamp(pitch, -1.2f, 1.2f);
        last_x = e.motion.x;
        last_y = e.motion.y;
      } else if (e.type == SDL_EVENT_MOUSE_WHEEL) {
        dist *= (e.wheel.y > 0) ? 0.9f : 1.1f;
        dist = glm::clamp(dist, radius * 0.5f, radius * 20.f);
      } else if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
        running = false;
      }
    }

    const glm::vec3 eye = center + glm::vec3(std::cos(pitch) * std::sin(yaw), std::sin(pitch),
                                             std::cos(pitch) * std::cos(yaw)) *
                                       dist;
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    const float aspect = h > 0 ? float(w) / float(h) : 1.f;
    const glm::mat4 view = glm::lookAt(eye, center, glm::vec3(0, 1, 0));
    const glm::mat4 proj = glm::perspective(glm::radians(50.f), aspect, 0.01f, dist * 20.f);
    // Vulkan clip space Y flip
    glm::mat4 proj_vk = proj;
    proj_vk[1][1] *= -1.f;
    const glm::mat4 model(1.f);
    const glm::mat4 mvp = proj_vk * view * model;
    renderer.draw(mvp, model);
  }

  renderer.shutdown();
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}

}  // namespace cath
