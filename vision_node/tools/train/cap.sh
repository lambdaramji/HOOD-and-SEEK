#!/usr/bin/env bash
# Training capture: one doll/empty setup = FRAMES frames, every frame saved, into ~/hoodseek-train/raw-1023.
#   ~/hoodseek-train/cap.sh <id> <kind doll|negative> <position> <in_roi true|false|none> <note>
# Waits 3 s first so the person who moved the doll can step out of view. No people in view while it runs.
set -euo pipefail
export PI5=${PI5:?"PI5를 지정하세요. 예: export PI5=root@<Pi5 IP>"} OUT=$HOME/hoodseek-train/raw-1023 FRAMES=${FRAMES:-10} DUMP_EVERY=1
sleep 3
exec "${HOODSEEK_REPO:-$HOME/HOOD-SEEK}/vision_node/tools/measure.sh" "$1" "$2" "$3" - "$4" "$5"
