#!/usr/bin/env bash
# Backup and re-encode Catherine Classic WMVs for current Proton (9+/Experimental/GE/CachyOS).
#
# Stock files are MPEG-4 ASP + WMA in ASF. Newer Proton often softlocks on the
# Shakespeare → Golden Playhouse handoff. Community fix: re-encode to H.264+AAC
# (MP4 bitstream) but keep the .wmv filename so the game still finds them.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GAME_DIR="${CATH_GAME_DIR:-${HOME}/.local/share/Steam/steamapps/common/CatherineClassic}"
DATA="${GAME_DIR}/data"
BACKUP_ROOT="${ROOT}/artifacts/movie-backup"
LOG_DIR="${ROOT}/artifacts/logs"
STAMP="$(date +%Y%m%d-%H%M%S)"
LOG="${LOG_DIR}/reencode-movies-${STAMP}.log"
FORCE=0
ONLY=""
PRESET="${FFMPEG_PRESET:-veryfast}"
CRF="${FFMPEG_CRF:-20}"
DIRS=(movie movie2 movie_jp movie2_jp)

usage() {
  cat <<EOF
Usage: $(basename "$0") [--force] [--only DIR]

Backup originals to ${BACKUP_ROOT}/<dir>/ then replace each .wmv with
H.264 + AAC content (MP4) kept under the same .wmv name.

  --force       Re-encode even if a backup already exists
  --only DIR    Only process one of: movie movie2 movie_jp movie2_jp
EOF
}

log() { printf '%s\n' "$*" | tee -a "${LOG}"; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --force) FORCE=1; shift ;;
    --only)
      ONLY="${2:-}"
      [[ -n "${ONLY}" ]] || { echo "--only needs a directory name" >&2; exit 1; }
      shift 2
      ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage; exit 1 ;;
  esac
done

mkdir -p "${LOG_DIR}" "${BACKUP_ROOT}"

if ! command -v ffmpeg >/dev/null 2>&1; then
  echo "ERROR: ffmpeg required" >&2
  exit 1
fi

if [[ ! -d "${DATA}" ]]; then
  echo "ERROR: game data not found: ${DATA}" >&2
  exit 1
fi

is_h264_named_wmv() {
  local f="$1"
  local v a
  v="$(ffprobe -v error -select_streams v:0 -show_entries stream=codec_name -of csv=p=0 "${f}" 2>/dev/null || true)"
  a="$(ffprobe -v error -select_streams a:0 -show_entries stream=codec_name -of csv=p=0 "${f}" 2>/dev/null || true)"
  [[ "${v}" == "h264" && "${a}" == "aac" ]]
}

reencode_one() {
  local src="$1"
  local dir_name="$2"
  local base dest_backup tmp
  base="$(basename "${src}")"
  dest_backup="${BACKUP_ROOT}/${dir_name}/${base}"
  tmp="$(mktemp "${src}.XXXXXX.tmp.mp4")"

  if [[ -f "${dest_backup}" && "${FORCE}" -eq 0 ]]; then
    if is_h264_named_wmv "${src}"; then
      log "SKIP (already h264/aac): ${dir_name}/${base}"
      rm -f "${tmp}"
      return 0
    fi
  fi

  mkdir -p "${BACKUP_ROOT}/${dir_name}"
  if [[ ! -f "${dest_backup}" ]]; then
    cp -a -- "${src}" "${dest_backup}"
    log "BACKED UP: ${dir_name}/${base}"
  elif [[ "${FORCE}" -eq 1 ]] && ! is_h264_named_wmv "${src}"; then
    # keep first/original backup; do not overwrite with a previous bad encode
    :
  fi

  # Encode from backup when present so --force is repeatable from pristine source
  local encode_src="${src}"
  if [[ -f "${dest_backup}" ]]; then
    encode_src="${dest_backup}"
  fi

  log "REENCODE: ${dir_name}/${base}"
  if ! ffmpeg -y -hide_banner -loglevel error -i "${encode_src}" \
      -c:v libx264 -preset "${PRESET}" -crf "${CRF}" -pix_fmt yuv420p \
      -c:a aac -b:a 192k \
      -movflags +faststart \
      -f mp4 "${tmp}"; then
    log "ERROR: ffmpeg failed for ${dir_name}/${base}"
    rm -f "${tmp}"
    return 1
  fi

  mv -f -- "${tmp}" "${src}"
}

main() {
  exec > >(tee -a "${LOG}") 2>&1
  log "=== reencode-movies ${STAMP} ==="
  log "DATA=${DATA}"
  log "BACKUP_ROOT=${BACKUP_ROOT}"
  log "FORCE=${FORCE}"
  log "PRESET=${PRESET} CRF=${CRF}"
  log "ONLY=${ONLY:-all}"

  local failures=0 total=0
  local dirs=("${DIRS[@]}")
  if [[ -n "${ONLY}" ]]; then
    dirs=("${ONLY}")
  fi

  for d in "${dirs[@]}"; do
    local dir="${DATA}/${d}"
    [[ -d "${dir}" ]] || { log "WARN: missing ${dir}"; continue; }
    shopt -s nullglob
    local files=("${dir}"/*.wmv "${dir}"/*.WMV)
    shopt -u nullglob
    for f in "${files[@]}"; do
      [[ -f "${f}" ]] || continue
      total=$((total + 1))
      if ! reencode_one "${f}" "${d}"; then
        failures=$((failures + 1))
      fi
    done
  done

  log "Finished: ${total} files considered, ${failures} failures"
  log "Restore with: ${ROOT}/scripts/restore-movies.sh"
  log "Log: ${LOG}"
  [[ "${failures}" -eq 0 ]]
}

main "$@"
