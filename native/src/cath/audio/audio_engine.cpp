#include "cath/audio/audio_engine.hpp"

#include "cath/platform/log.hpp"

#include <SDL3/SDL.h>

#include <cstring>
#include <fstream>
#include <vector>

namespace cath {
namespace {

#pragma pack(push, 1)
struct WavHdr {
  char riff[4];
  uint32_t size;
  char wave[4];
};
#pragma pack(pop)

}  // namespace

AudioEngine::~AudioEngine() { shutdown(); }

bool AudioEngine::init(std::string* error) {
  if (ready_) {
    return true;
  }
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    if (error) {
      *error = SDL_GetError();
    }
    return false;
  }
  SDL_AudioSpec spec{};
  spec.freq = 48000;
  spec.format = SDL_AUDIO_F32;
  spec.channels = 2;
  SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
  if (!stream) {
    if (error) {
      *error = SDL_GetError();
    }
    return false;
  }
  stream_ = stream;
  SDL_ResumeAudioStreamDevice(stream);
  ready_ = true;
  CATH_LOG_INFO("audio engine ready");
  return true;
}

void AudioEngine::shutdown() {
  if (stream_) {
    SDL_DestroyAudioStream(static_cast<SDL_AudioStream*>(stream_));
    stream_ = nullptr;
  }
  ready_ = false;
}

void AudioEngine::stop() {
  if (stream_) {
    SDL_ClearAudioStream(static_cast<SDL_AudioStream*>(stream_));
  }
}

void AudioEngine::play_pcm_f32(const float* interleaved, size_t frames, int sample_rate, int channels) {
  if (!ready_ || !stream_ || !interleaved || frames == 0) {
    return;
  }
  SDL_AudioStream* stream = static_cast<SDL_AudioStream*>(stream_);
  SDL_AudioSpec src{};
  src.freq = sample_rate;
  src.format = SDL_AUDIO_F32;
  src.channels = channels;
  SDL_SetAudioStreamFormat(stream, &src, nullptr);
  SDL_PutAudioStreamData(stream, interleaved, int(frames * size_t(channels) * sizeof(float)));
}

bool AudioEngine::play_wav(const std::filesystem::path& path, std::string* error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    if (error) {
      *error = "wav open failed";
    }
    return false;
  }
  WavHdr hdr{};
  in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
  if (std::strncmp(hdr.riff, "RIFF", 4) != 0 || std::strncmp(hdr.wave, "WAVE", 4) != 0) {
    if (error) {
      *error = "not a WAV";
    }
    return false;
  }
  uint16_t audio_format = 0, channels = 0, bits = 0;
  uint32_t rate = 0, data_size = 0;
  std::vector<uint8_t> data;
  while (in && !in.eof()) {
    char id[4];
    uint32_t sz = 0;
    in.read(id, 4);
    in.read(reinterpret_cast<char*>(&sz), 4);
    if (!in) {
      break;
    }
    if (std::strncmp(id, "fmt ", 4) == 0) {
      in.read(reinterpret_cast<char*>(&audio_format), 2);
      in.read(reinterpret_cast<char*>(&channels), 2);
      in.read(reinterpret_cast<char*>(&rate), 4);
      in.ignore(6);
      in.read(reinterpret_cast<char*>(&bits), 2);
      if (sz > 16) {
        in.ignore(sz - 16);
      }
    } else if (std::strncmp(id, "data", 4) == 0) {
      data_size = sz;
      data.resize(sz);
      in.read(reinterpret_cast<char*>(data.data()), sz);
      break;
    } else {
      in.ignore(sz);
    }
  }
  if (data.empty() || channels == 0 || rate == 0) {
    if (error) {
      *error = "wav parse failed";
    }
    return false;
  }
  std::vector<float> f32;
  if (audio_format == 1 && bits == 16) {
    const size_t n = data.size() / 2;
    f32.resize(n);
    for (size_t i = 0; i < n; ++i) {
      const int16_t s = int16_t(data[i * 2] | (data[i * 2 + 1] << 8));
      f32[i] = float(s) / 32768.f;
    }
  } else if (audio_format == 3 && bits == 32) {
    f32.resize(data.size() / 4);
    std::memcpy(f32.data(), data.data(), data.size());
  } else {
    if (error) {
      *error = "unsupported wav format";
    }
    return false;
  }
  play_pcm_f32(f32.data(), f32.size() / channels, int(rate), int(channels));
  return true;
}

bool AudioEngine::list_cpk(const std::filesystem::path& cpk, std::vector<std::string>& names, std::string* error) {
  names.clear();
  std::ifstream in(cpk, std::ios::binary);
  if (!in) {
    if (error) {
      *error = "cpk open failed";
    }
    return false;
  }
  char magic[4];
  in.read(magic, 4);
  if (std::strncmp(magic, "CPK ", 4) != 0) {
    if (error) {
      *error = "not a CPK";
    }
    return false;
  }
  // Scan for printable ASCII path-like tokens in the UTF TOC region (first 256KB).
  std::vector<char> buf(256 * 1024);
  in.read(buf.data(), std::streamsize(buf.size()));
  const size_t n = size_t(in.gcount());
  std::string cur;
  for (size_t i = 0; i < n; ++i) {
    const unsigned char c = (unsigned char)buf[i];
    if ((c >= 32 && c < 127) && c != '\\') {
      cur.push_back(char(c));
    } else {
      if (cur.size() >= 5 && (cur.find(".hca") != std::string::npos || cur.find(".adx") != std::string::npos ||
                              cur.find(".awb") != std::string::npos || cur.find('/') != std::string::npos)) {
        names.push_back(cur);
      }
      cur.clear();
    }
  }
  CATH_LOG_INFO("CPK %s: probed %zu name tokens (HCA decode stub)", cpk.filename().c_str(), names.size());
  return true;
}

}  // namespace cath
