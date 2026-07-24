#!/usr/bin/env bash
# Optional private reference only — never link into cath binaries.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="${ROOT}/vendor/gamebryo-ref"
if [[ -d "${DEST}/.git" ]]; then
  echo "Already present: ${DEST}"
  exit 0
fi
mkdir -p "${ROOT}/vendor"
git clone --depth 1 https://github.com/sigmaco/gamebryo-v3.2.0.661.git "${DEST}"
echo "Cloned reference SDK to ${DEST}"
echo "Study headers/docs only. Do not link Lib/ or DLL/ into cath."
