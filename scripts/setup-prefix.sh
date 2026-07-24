#!/usr/bin/env bash
# Apply Media Foundation + video tweaks so Catherine Classic (893180) plays
# cutscenes on current Proton builds (Experimental / GE / CachyOS / DW), not
# only GE-Proton8-32.
set -euo pipefail

APP_ID=893180
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERB="${ROOT}/vendor/mf_catherine.verb"
LOG_DIR="${ROOT}/artifacts/logs"
STAMP="$(date +%Y%m%d-%H%M%S)"
LOG="${LOG_DIR}/setup-prefix-${STAMP}.log"
COMPATDATA="${STEAM_COMPATDATA:-${HOME}/.local/share/Steam/steamapps/compatdata/${APP_ID}}"
SKIP_LAV=0

usage() {
  cat <<EOF
Usage: $(basename "$0") [--skip-lav]

Installs mf_catherine.verb into the Proton prefix for app ${APP_ID}, then
applies newer-Proton video helpers (lavfilters + disable winegstreamer).

Works with: Proton Experimental, latest GE-Proton, Proton-CachyOS, DW-Proton,
and GE-Proton8-32. Prefer a current Proton; use GE-Proton8-32 only as fallback.

  --skip-lav   Skip lavfilters (mf_catherine only)
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --skip-lav) SKIP_LAV=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage; exit 1 ;;
  esac
done

mkdir -p "${LOG_DIR}"

log() { printf '%s\n' "$*" | tee -a "${LOG}"; }

need_cmd() { command -v "$1" >/dev/null 2>&1; }

install_protontricks() {
  if need_cmd protontricks; then
    return 0
  fi
  log "protontricks not found; attempting install..."
  if need_cmd pacman; then
    sudo pacman -S --needed --noconfirm protontricks
  elif need_cmd flatpak; then
    flatpak install -y flathub com.github.Matoking.protontricks
  else
    log "ERROR: install protontricks manually, then re-run."
    exit 1
  fi
}

protontricks_cmd() {
  if need_cmd protontricks; then
    printf '%s' "protontricks"
  elif need_cmd flatpak && flatpak info com.github.Matoking.protontricks >/dev/null 2>&1; then
    printf '%s' "flatpak run com.github.Matoking.protontricks"
  else
    return 1
  fi
}

# Prefer builtin winegstreamer on current Proton so H.264/AAC (re-encoded
# .wmv files) and stock decode paths can work. Empty override breaks newer Proton.
apply_winegstreamer_builtin() {
  log "Setting winegstreamer DLL override to builtin (needed for current Proton / H.264)..."
  # shellcheck disable=SC2086
  ${PT} --no-bwrap -c \
    'wine reg add "HKCU\Software\Wine\DllOverrides" /v winegstreamer /t REG_SZ /d "builtin" /f' \
    "${APP_ID}"

  mkdir -p "${ROOT}/artifacts"
  {
    echo "winegstreamer=builtin"
    date -Iseconds
  } > "${ROOT}/artifacts/dll-overrides-applied.txt"
}

# Register MF decoder DLLs (verb may skip this on newer winetricks).
register_mf_codecs() {
  log "Registering MF codec DLLs (regsvr32)..."
  # shellcheck disable=SC2086
  ${PT} --no-bwrap -c \
    'wine regsvr32 /s colorcnv.dll mp4sdecd.dll wmadmod.dll evr.dll' \
    "${APP_ID}" || log "WARN: regsvr32 returned non-zero (often OK if already registered)"
}

main() {
  exec > >(tee -a "${LOG}") 2>&1
  log "=== setup-prefix ${STAMP} ==="
  log "ROOT=${ROOT}"
  log "APP_ID=${APP_ID}"
  log "VERB=${VERB}"
  log "COMPATDATA=${COMPATDATA}"
  log "SKIP_LAV=${SKIP_LAV}"

  if [[ ! -f "${VERB}" ]]; then
    log "ERROR: missing ${VERB}"
    exit 1
  fi

  if [[ ! -d "${COMPATDATA}/pfx" ]]; then
    log "ERROR: Proton prefix not found at ${COMPATDATA}/pfx"
    log "Launch Catherine Classic once under your chosen Proton, then re-run."
    exit 1
  fi

  install_protontricks
  PT="$(protontricks_cmd)" || {
    log "ERROR: protontricks unavailable after install attempt"
    exit 1
  }
  log "Using: ${PT}"

  log "Applying mf_catherine.verb (downloads MF DLLs; may take several minutes)..."
  # shellcheck disable=SC2086
  ${PT} --no-bwrap "${APP_ID}" --force "${VERB}"

  if [[ "${SKIP_LAV}" -eq 0 ]]; then
    log "Installing lavfilters (helps newer Proton video decode paths)..."
    # shellcheck disable=SC2086
    if ! ${PT} --no-bwrap "${APP_ID}" -q lavfilters; then
      log "WARN: lavfilters install failed; continuing (mf_catherine may still be enough)"
    fi
  fi

  register_mf_codecs
  # Default to current-Proton / H.264 path. Native MF + re-encoded H.264 crashes.
  # For GE-Proton8-32 + stock WMV: run apply-video-mode.sh mf-wmv after restore-movies.sh
  "${ROOT}/scripts/apply-video-mode.sh" h264

  log "Done."
  log ""
  log "Recommended: use latest Proton (Experimental / current GE / CachyOS / DW)."
  log "On current Proton, re-encode movies after setup (fixes Shakespeare softlock):"
  log "  ${ROOT}/scripts/reencode-movies.sh --only movie2   # quick: intro after quote"
  log "  ${ROOT}/scripts/reencode-movies.sh                 # full story FMVs"
  log "  ${ROOT}/scripts/apply-video-mode.sh h264           # required after re-encode"
  log "Fallback (stock WMV): restore-movies.sh && apply-video-mode.sh mf-wmv + GE-Proton8-32"
  "${ROOT}/scripts/steam-launch-notes.sh" || true
  log "Log: ${LOG}"
}

main "$@"
