#!/usr/bin/env bash
set -euo pipefail
SIM_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
PX4_DIR="${PX4_DIR:-$(dirname -- "$SIM_DIR")/PX4-Autopilot}"
cd "$PX4_DIR"
export PX4_GZ_STANDALONE=1
exec make px4_sitl gz_x500_mono_cam
