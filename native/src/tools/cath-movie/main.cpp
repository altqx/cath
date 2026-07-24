#include "cath/media/movie_player.hpp"
#include "cath/platform/log.hpp"
#include "cath/platform/paths.hpp"
#include "cath/render/vulkan_renderer.hpp"

#include <SDL3/SDL.h>
#include <glm/mat4x4.hpp>

#include <cstring>
#include <filesystem>
#include <optional>
#include <string>

int main(int argc, char** argv) {
  std::optional<std::string> game_cli;
  std::filesystem::path movie_rel = "movie2/001_00.wmv";
  std::filesystem::path shaders = std::filesystem::path(argv[0]).parent_path() / "shaders";
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--movie") == 0 && i + 1 < argc) {
      movie_rel = argv[++i];
    } else if (std::strcmp(argv[i], "--game-dir") == 0 && i + 1 < argc) {
      game_cli = argv[++i];
    }
  }
  const std::filesystem::path game = cath::resolve_game_dir(game_cli);
  std::filesystem::path movie = movie_rel;
  if (movie.is_relative()) {
    movie = game / "data" / movie_rel;
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    CATH_LOG_ERROR("%s", SDL_GetError());
    return 1;
  }
  SDL_Window* window = SDL_CreateWindow("cath-movie", 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
  cath::VulkanRenderer renderer;
  std::string err;
  if (!renderer.init(window, shaders, &err)) {
    CATH_LOG_ERROR("%s", err.c_str());
    return 1;
  }
  cath::MoviePlayer player;
  if (!player.open(movie, &err)) {
    CATH_LOG_ERROR("open movie: %s", err.c_str());
    return 1;
  }
  cath::Mesh quad;
  quad.name = "q";
  quad.vertices = {{-1, -1, 0, 0, 0, 1, 0, 1}, {1, -1, 0, 0, 0, 1, 1, 1}, {1, 1, 0, 0, 0, 1, 1, 0}, {-1, 1, 0, 0, 0, 1, 0, 0}};
  quad.indices = {0, 1, 2, 0, 2, 3};
  cath::MovieFrame fr;
  if (!player.next_frame(fr, &err)) {
    CATH_LOG_ERROR("no frames: %s", err.c_str());
    return 1;
  }
  renderer.upload_mesh(quad, fr.image, &err);
  bool running = true;
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_QUIT || (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE)) {
        running = false;
      }
    }
    if (player.next_frame(fr, nullptr)) {
      renderer.update_texture(fr.image, nullptr);
      renderer.draw(glm::mat4(1.f), glm::mat4(1.f));
    } else {
      running = false;
    }
  }
  renderer.shutdown();
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
