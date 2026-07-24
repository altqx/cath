#!/usr/bin/env bash
# Restore Catherine Classic WMVs from artifacts/movie-backup.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GAME_DIR="${CATH_GAME_DIR:-${HOME}/.local/share/Steam/steamapps/common/CatherineClassic}"
DATA="${GAME_DIR}/data"
BACKUP_ROOT="${ROOT}/artifacts/movie-backup"

if [[ ! -d "${BACKUP_ROOT}" ]]; then
  echo "ERROR: no backup at ${BACKUP_ROOT}" >&2
  exit 1
fi

restored=0
while IFS= read -r -d '' bak; do
  rel="${bak#"${BACKUP_ROOT}/"}"
  dest="${DATA}/${rel}"
  mkdir -p "$(dirname "${dest}")"
  cp -a -- "${bak}" "${dest}"
  echo "RESTORED: ${rel}"
  restored=$((restored + 1))
done < <(find "${BACKUP_ROOT}" -type f -iname '*.wmv' -print0)

echo "Restored ${restored} file(s)."
