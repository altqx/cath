#!/usr/bin/env bash
# Disable Depth of Field + Blur (sticky soft camera / blurry cutscenes on Proton).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INI="${CATH_APPSETTINGS:-${HOME}/.local/share/Steam/steamapps/compatdata/893180/pfx/drive_c/users/steamuser/AppData/Roaming/TheEccentricApe/Catherine/AppSettings.ini}"
BACKUP_DIR="${ROOT}/artifacts/config-backup"

if [[ ! -f "${INI}" ]]; then
  echo "ERROR: AppSettings.ini not found: ${INI}" >&2
  echo "Launch the game once so it creates settings, then re-run." >&2
  exit 1
fi

mkdir -p "${BACKUP_DIR}"
cp -a -- "${INI}" "${BACKUP_DIR}/AppSettings.ini.$(date +%Y%m%d-%H%M%S)"

sed -i 's/^VIDEO_DOF=.*/VIDEO_DOF=0/' "${INI}"
sed -i 's/^VIDEO_BLUR=.*/VIDEO_BLUR=0/' "${INI}"

echo "Updated ${INI}"
rg -n 'VIDEO_DOF|VIDEO_BLUR' "${INI}" || true
echo
echo "Quit the game fully and relaunch. In Options → Graphics you can also set:"
echo "  Depth of Field = Off"
echo "  Blur = Off"
echo
echo "Note: some in-engine shots use intentional focus pulls; with DoF off those stay sharp."
echo "If the whole image is soft (not just focus), match in-game resolution to any"
echo "gamescope / compositor scale you use so the frame isn’t upscaled."
