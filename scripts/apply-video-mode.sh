#!/usr/bin/env bash
# Switch Catherine Classic prefix between video modes for different Proton paths.
set -euo pipefail

APP_ID=893180
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG_DIR="${ROOT}/artifacts/logs"
STAMP="$(date +%Y%m%d-%H%M%S)"
MODE="${1:-}"

usage() {
  cat <<EOF
Usage: $(basename "$0") <h264|mf-wmv>

  h264    Current Proton (CachyOS/Experimental/GE): builtin MF + winegstreamer,
          for H.264+AAC files named .wmv (after reencode-movies.sh).
          Fixes crash when mf_catherine DLLs meet re-encoded videos.

  mf-wmv  Stock MPEG-4/WMA .wmv + mf_catherine MF DLLs + winegstreamer disabled.
          Best with GE-Proton8-32 after setup-prefix.sh (no H.264 re-encode).
EOF
}

if [[ -z "${MODE}" || "${MODE}" == "-h" || "${MODE}" == "--help" ]]; then
  usage
  exit 0
fi

mkdir -p "${LOG_DIR}"
LOG="${LOG_DIR}/apply-video-mode-${STAMP}.log"
exec > >(tee -a "${LOG}") 2>&1

need_cmd() { command -v "$1" >/dev/null 2>&1; }
if need_cmd protontricks; then
  PT=(protontricks --no-bwrap)
elif need_cmd flatpak && flatpak info com.github.Matoking.protontricks >/dev/null 2>&1; then
  PT=(flatpak run com.github.Matoking.protontricks --no-bwrap)
else
  echo "ERROR: protontricks required" >&2
  exit 1
fi

reg() {
  local name="$1" value="$2"
  echo "DllOverrides ${name}=${value}"
  "${PT[@]}" -c \
    "wine reg add \"HKCU\\Software\\Wine\\DllOverrides\" /v ${name} /t REG_SZ /d \"${value}\" /f" \
    "${APP_ID}" >/dev/null
}

case "${MODE}" in
  h264)
    echo "=== mode: h264 (current Proton + re-encoded movies) ==="
    # Do not force Win7 mf_catherine MF — it crashes on MP4/H.264 content named .wmv
    for dll in mfplat.dll mf.dll mfplay.dll mfreadwrite.dll mferror.dll evr.dll dxva2.dll colorcnv.dll; do
      reg "*${dll}" "builtin"
      reg "${dll%.dll}" "builtin"
    done
    reg "winegstreamer" "builtin"
    printf 'mode=h264\nwinegstreamer=builtin\nmf*=builtin\n%s\n' "$(date -Iseconds)" \
      > "${ROOT}/artifacts/dll-overrides-applied.txt"
    echo "OK. Use Proton-CachyOS / Experimental / latest GE with re-encoded .wmv files."
    ;;
  mf-wmv)
    echo "=== mode: mf-wmv (stock WMV + mf_catherine MF) ==="
    for dll in colorcnv.dll mf.dll mferror.dll mfplat.dll mfplay.dll mfreadwrite.dll evr.dll dxva2.dll; do
      # Wine DllOverrides value for Windows DLLs shipped with the prefix
      reg "*${dll}" "native"
      reg "${dll%.dll}" "native"
    done
    reg "winegstreamer" ""
    printf 'mode=mf-wmv\nwinegstreamer=\nmf*=mf_catherine\n%s\n' "$(date -Iseconds)" \
      > "${ROOT}/artifacts/dll-overrides-applied.txt"
    echo "OK. Prefer GE-Proton8-32 and stock (or restored) MPEG-4/WMA .wmv files."
    echo "If you re-encoded, restore first: ./scripts/restore-movies.sh"
    ;;
  *)
    usage
    exit 1
    ;;
esac

echo "Log: ${LOG}"
