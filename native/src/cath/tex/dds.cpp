#include "cath/tex/dds.hpp"

#include <cstring>
#include <fstream>

namespace cath {
namespace {

#pragma pack(push, 1)
struct DdsPixelFormat {
  uint32_t size;
  uint32_t flags;
  uint32_t fourcc;
  uint32_t rgb_bit_count;
  uint32_t r_mask, g_mask, b_mask, a_mask;
};
struct DdsHeader {
  uint32_t magic;
  uint32_t size;
  uint32_t flags;
  uint32_t height;
  uint32_t width;
  uint32_t pitch_or_linear;
  uint32_t depth;
  uint32_t mip_map_count;
  uint32_t reserved1[11];
  DdsPixelFormat pf;
  uint32_t caps, caps2, caps3, caps4, reserved2;
};
#pragma pack(pop)

constexpr uint32_t fourcc(char a, char b, char c, char d) {
  return uint32_t(uint8_t(a)) | (uint32_t(uint8_t(b)) << 8) | (uint32_t(uint8_t(c)) << 16) |
         (uint32_t(uint8_t(d)) << 24);
}

uint8_t expand5(uint8_t v) { return uint8_t((v << 3) | (v >> 2)); }
uint8_t expand6(uint8_t v) { return uint8_t((v << 2) | (v >> 4)); }

void decode_rgb565(uint16_t c, uint8_t& r, uint8_t& g, uint8_t& b) {
  r = expand5(uint8_t((c >> 11) & 0x1f));
  g = expand6(uint8_t((c >> 5) & 0x3f));
  b = expand5(uint8_t(c & 0x1f));
}

void decode_bc1_block(const uint8_t block[8], uint8_t out_rgba[16 * 4], bool bc1a) {
  const uint16_t c0 = uint16_t(block[0] | (block[1] << 8));
  const uint16_t c1 = uint16_t(block[2] | (block[3] << 8));
  uint8_t r[4], g[4], b[4], a[4];
  decode_rgb565(c0, r[0], g[0], b[0]);
  decode_rgb565(c1, r[1], g[1], b[1]);
  a[0] = a[1] = 255;
  if (c0 > c1 || !bc1a) {
    r[2] = uint8_t((2 * r[0] + r[1]) / 3);
    g[2] = uint8_t((2 * g[0] + g[1]) / 3);
    b[2] = uint8_t((2 * b[0] + b[1]) / 3);
    a[2] = 255;
    r[3] = uint8_t((r[0] + 2 * r[1]) / 3);
    g[3] = uint8_t((g[0] + 2 * g[1]) / 3);
    b[3] = uint8_t((b[0] + 2 * b[1]) / 3);
    a[3] = 255;
  } else {
    r[2] = uint8_t((r[0] + r[1]) / 2);
    g[2] = uint8_t((g[0] + g[1]) / 2);
    b[2] = uint8_t((b[0] + b[1]) / 2);
    a[2] = 255;
    r[3] = g[3] = b[3] = 0;
    a[3] = 0;
  }
  uint32_t bits = uint32_t(block[4] | (block[5] << 8) | (block[6] << 16) | (block[7] << 24));
  for (int i = 0; i < 16; ++i) {
    const uint32_t idx = (bits >> (2 * i)) & 3u;
    out_rgba[i * 4 + 0] = r[idx];
    out_rgba[i * 4 + 1] = g[idx];
    out_rgba[i * 4 + 2] = b[idx];
    out_rgba[i * 4 + 3] = a[idx];
  }
}

void decode_bc3_block(const uint8_t block[16], uint8_t out_rgba[16 * 4]) {
  uint8_t a[8];
  a[0] = block[0];
  a[1] = block[1];
  if (a[0] > a[1]) {
    for (int i = 1; i <= 6; ++i) {
      a[i + 1] = uint8_t(((7 - i) * a[0] + i * a[1]) / 7);
    }
  } else {
    for (int i = 1; i <= 4; ++i) {
      a[i + 1] = uint8_t(((5 - i) * a[0] + i * a[1]) / 5);
    }
    a[6] = 0;
    a[7] = 255;
  }
  uint64_t abits = 0;
  for (int i = 0; i < 6; ++i) {
    abits |= uint64_t(block[2 + i]) << (8 * i);
  }
  decode_bc1_block(block + 8, out_rgba, false);
  for (int i = 0; i < 16; ++i) {
    const uint32_t idx = uint32_t((abits >> (3 * i)) & 7u);
    out_rgba[i * 4 + 3] = a[idx];
  }
}

bool decode_bc_image(const uint8_t* src, size_t src_size, uint32_t w, uint32_t h, bool bc3, ImageRgba8& out,
                     std::string* error) {
  const uint32_t bw = (w + 3) / 4;
  const uint32_t bh = (h + 3) / 4;
  const size_t block_size = bc3 ? 16u : 8u;
  const size_t need = size_t(bw) * bh * block_size;
  if (src_size < need) {
    if (error) {
      *error = "BC DDS truncated";
    }
    return false;
  }
  out.width = w;
  out.height = h;
  out.pixels.assign(size_t(w) * h * 4, 0);
  uint8_t block_rgba[16 * 4];
  size_t off = 0;
  for (uint32_t by = 0; by < bh; ++by) {
    for (uint32_t bx = 0; bx < bw; ++bx) {
      if (bc3) {
        decode_bc3_block(src + off, block_rgba);
      } else {
        decode_bc1_block(src + off, block_rgba, true);
      }
      off += block_size;
      for (uint32_t py = 0; py < 4; ++py) {
        for (uint32_t px = 0; px < 4; ++px) {
          const uint32_t x = bx * 4 + px;
          const uint32_t y = by * 4 + py;
          if (x >= w || y >= h) {
            continue;
          }
          const size_t di = (size_t(y) * w + x) * 4;
          const size_t si = (size_t(py) * 4 + px) * 4;
          std::memcpy(out.pixels.data() + di, block_rgba + si, 4);
        }
      }
    }
  }
  return true;
}

}  // namespace

ImageRgba8 make_solid_image(uint32_t w, uint32_t h, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  ImageRgba8 img;
  img.width = w;
  img.height = h;
  img.pixels.resize(size_t(w) * h * 4);
  for (size_t i = 0; i < img.pixels.size(); i += 4) {
    img.pixels[i + 0] = r;
    img.pixels[i + 1] = g;
    img.pixels[i + 2] = b;
    img.pixels[i + 3] = a;
  }
  return img;
}

bool load_dds(const std::filesystem::path& path, ImageRgba8& out, std::string* error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    if (error) {
      *error = "open failed";
    }
    return false;
  }
  DdsHeader hdr{};
  in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
  if (!in || hdr.magic != fourcc('D', 'D', 'S', ' ')) {
    if (error) {
      *error = "bad DDS magic";
    }
    return false;
  }
  out.width = hdr.width;
  out.height = hdr.height;

  if (hdr.pf.rgb_bit_count == 32 && hdr.pf.fourcc == 0) {
    out.pixels.resize(size_t(out.width) * out.height * 4);
    std::vector<uint8_t> raw(out.pixels.size());
    in.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
    const bool bgra = hdr.pf.b_mask == 0x000000ff;
    for (size_t i = 0; i < out.pixels.size(); i += 4) {
      if (bgra) {
        out.pixels[i + 0] = raw[i + 2];
        out.pixels[i + 1] = raw[i + 1];
        out.pixels[i + 2] = raw[i + 0];
        out.pixels[i + 3] = raw[i + 3];
      } else {
        std::memcpy(out.pixels.data() + i, raw.data() + i, 4);
      }
    }
    return true;
  }

  const bool bc1 = hdr.pf.fourcc == fourcc('D', 'X', 'T', '1');
  const bool bc2 = hdr.pf.fourcc == fourcc('D', 'X', 'T', '3');
  const bool bc3 = hdr.pf.fourcc == fourcc('D', 'X', 'T', '5');
  if (bc1 || bc2 || bc3) {
    in.seekg(0, std::ios::end);
    const std::streamoff end = in.tellg();
    in.seekg(sizeof(DdsHeader), std::ios::beg);
    const size_t avail = size_t(end > std::streamoff(sizeof(DdsHeader)) ? end - std::streamoff(sizeof(DdsHeader)) : 0);
    std::vector<uint8_t> raw(avail);
    in.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
    // DXT2/3: treat like BC3 color + punch-through via BC1 path for now (color ok)
    if (bc2) {
      // DXT3: 8-byte explicit alpha + 8-byte BC1 color
      const uint32_t bw = (out.width + 3) / 4;
      const uint32_t bh = (out.height + 3) / 4;
      out.pixels.assign(size_t(out.width) * out.height * 4, 0);
      size_t off = 0;
      uint8_t block_rgba[16 * 4];
      for (uint32_t by = 0; by < bh; ++by) {
        for (uint32_t bx = 0; bx < bw; ++bx) {
          if (off + 16 > raw.size()) {
            break;
          }
          // alpha nibbles
          uint8_t alpha[16];
          for (int i = 0; i < 8; ++i) {
            alpha[i * 2 + 0] = uint8_t((raw[off + i] & 0x0f) * 17);
            alpha[i * 2 + 1] = uint8_t(((raw[off + i] >> 4) & 0x0f) * 17);
          }
          decode_bc1_block(raw.data() + off + 8, block_rgba, false);
          for (int i = 0; i < 16; ++i) {
            block_rgba[i * 4 + 3] = alpha[i];
          }
          off += 16;
          for (uint32_t py = 0; py < 4; ++py) {
            for (uint32_t px = 0; px < 4; ++px) {
              const uint32_t x = bx * 4 + px;
              const uint32_t y = by * 4 + py;
              if (x >= out.width || y >= out.height) {
                continue;
              }
              const size_t di = (size_t(y) * out.width + x) * 4;
              const size_t si = (size_t(py) * 4 + px) * 4;
              std::memcpy(out.pixels.data() + di, block_rgba + si, 4);
            }
          }
        }
      }
      return true;
    }
    return decode_bc_image(raw.data(), raw.size(), out.width, out.height, bc3, out, error);
  }

  if (error) {
    *error = "unsupported DDS format";
  }
  return false;
}

}  // namespace cath
