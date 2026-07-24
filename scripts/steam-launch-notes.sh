#!/usr/bin/env bash
# Print recommended Steam launch settings for Catherine Classic.
set -euo pipefail

APP_ID=893180
GAME_DIR="${HOME}/.local/share/Steam/steamapps/common/CatherineClassic"
COMPAT="${HOME}/.local/share/Steam/steamapps/compatdata/${APP_ID}"
TOOLS="${HOME}/.local/share/Steam/compatibilitytools.d"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cat <<EOF
Catherine Classic — Steam launch checklist
==========================================

App ID:          ${APP_ID}
Game directory:  ${GAME_DIR}
Proton prefix:   ${COMPAT}

1) Steam → Catherine Classic → Properties → Compatibility
   - Force a specific Steam Play compatibility tool
   - Preferred (pick one current build):
       • Proton Experimental
       • Latest GE-Proton / Proton-GE
       • Proton-CachyOS Latest
       • DW-Proton Latest
   - Fallback only if New Game softlocks on current Proton:
       • GE-Proton8-32

   Tools present under ${TOOLS}:
EOF

if [[ -d "${TOOLS}" ]]; then
  find "${TOOLS}" -mindepth 1 -maxdepth 1 -printf '       • %f\n' 2>/dev/null | sort || true
fi

cat <<EOF

2) Launch options
   - Preferred: (empty)
   - Optional:  gamemoderun %command%
   - Do NOT use: PROTON_USE_WINED3D=1  (cutscene A/V desync)

3) After first launch with your chosen Proton (prefix created):
   ${ROOT}/scripts/setup-prefix.sh
   # or from repo root: ./scripts/setup-prefix.sh

   Re-run setup-prefix.sh after switching major Proton versions if video breaks again
   (same app id prefix is reused when possible).

4) On current Proton, re-encode movies (required to pass Shakespeare quote):
   ./scripts/reencode-movies.sh --only movie2
   ./scripts/reencode-movies.sh

5) Verify:
   - New Game past Shakespeare quote into Golden Playhouse FMV
   - First anime cutscene (video + voice)
   - Logo / title movies

EOF
