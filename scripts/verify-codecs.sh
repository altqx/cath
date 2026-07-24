#!/usr/bin/env bash
# Probe sample Catherine WMV codecs.
set -euo pipefail

GAME_DIR="${CATH_GAME_DIR:-${HOME}/.local/share/Steam/steamapps/common/CatherineClassic}"
DATA="${GAME_DIR}/data"

if ! command -v ffprobe >/dev/null 2>&1; then
  echo "ERROR: ffprobe required" >&2
  exit 1
fi

if [[ ! -d "${DATA}" ]]; then
  echo "ERROR: game data not found: ${DATA}" >&2
  exit 1
fi

samples=(
  "movie/005_00.wmv"
  "movie2/000_00.wmv"
)

echo "Game data: ${DATA}"
echo

for rel in "${samples[@]}"; do
  f="${DATA}/${rel}"
  if [[ ! -f "${f}" ]]; then
    echo "MISSING: ${rel}"
    continue
  fi
  echo "=== ${rel} ==="
  ffprobe -v error -show_entries format=format_name,duration \
    -show_entries stream=index,codec_type,codec_name,width,height,bit_rate \
    -of default=nw=1 "${f}"
  echo
done

echo "WMV counts:"
for d in movie movie2 movie_jp movie2_jp; do
  if [[ -d "${DATA}/${d}" ]]; then
    n="$(find "${DATA}/${d}" -maxdepth 1 -type f -iname '*.wmv' | wc -l)"
    echo "  ${d}: ${n}"
  fi
done
