#pragma once
#include "cath/tex/dds.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace cath {

struct MovieFrame {
  ImageRgba8 image;
  double pts = 0.0;
};

class MoviePlayer {
 public:
  MoviePlayer();
  ~MoviePlayer();

  MoviePlayer(const MoviePlayer&) = delete;
  MoviePlayer& operator=(const MoviePlayer&) = delete;

  bool open(const std::filesystem::path& path, std::string* error = nullptr);
  void close();

  // Decode next video frame into RGBA. Returns false at EOF / error.
  bool next_frame(MovieFrame& out, std::string* error = nullptr);

  // Pull planar PCM (interleaved float) if audio stream present; may return empty.
  bool next_audio(std::vector<float>& interleaved, int& sample_rate, int& channels);

  bool opened() const { return opened_; }
  double duration() const { return duration_; }
  int width() const { return width_; }
  int height() const { return height_; }

 private:
  struct Impl;
  Impl* impl_ = nullptr;
  bool opened_ = false;
  double duration_ = 0.0;
  int width_ = 0;
  int height_ = 0;
};

}  // namespace cath
