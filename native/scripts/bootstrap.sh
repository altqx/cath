#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT}"

need() { command -v "$1" >/dev/null 2>&1 || { echo "missing: $1"; return 1; }; }

echo "Checking deps..."
need cmake
need g++
need glslc
need pkg-config
pkg-config --exists sdl3 || { echo "missing: sdl3 (pkg-config)"; exit 1; }
pkg-config --exists libavformat libavcodec libavutil libswscale libswresample || {
  echo "missing: ffmpeg devel (libav*)"; exit 1;
}
[[ -f /usr/include/vulkan/vulkan.h ]] || { echo "missing: vulkan-headers"; exit 1; }
[[ -f /usr/include/glm/glm.hpp ]] || { echo "missing: glm"; exit 1; }
[[ -f third_party/volk.h ]] || { echo "missing: third_party/volk.h — re-run fetch"; exit 1; }

echo "Configuring..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
echo "Building..."
cmake --build build -j"$(nproc)"
echo "OK: ${ROOT}/build/cath"
echo "Run:"
echo "  ${ROOT}/build/cath"
echo "  ${ROOT}/build/cath --smoke"
echo "  ${ROOT}/build/cath-viewer --nif data/character/01/c01_00.nif"
echo "  ${ROOT}/build/cath-movie --movie movie2/001_00.wmv"
