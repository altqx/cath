#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace cath {

// SDL3 audio device + CRI CPK TOC probe. Full HCA decode may stub to silence until FOSS decoder lands.
class AudioEngine {
 public:
  AudioEngine() = default;
  ~AudioEngine();

  bool init(std::string* error = nullptr);
  void shutdown();

  bool play_wav(const std::filesystem::path& path, std::string* error = nullptr);
  void play_pcm_f32(const float* interleaved, size_t frames, int sample_rate, int channels);
  void stop();

  // List files inside a CRI CPK (TOC only).
  static bool list_cpk(const std::filesystem::path& cpk, std::vector<std::string>& names, std::string* error = nullptr);

  bool ready() const { return ready_; }

 private:
  bool ready_ = false;
  void* stream_ = nullptr;  // SDL_AudioStream*
  void* device_ = nullptr;  // opaque device id holder
};

}  // namespace cath
