#!/usr/bin/env bash
set -euo pipefail
SIM_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
PX4_DIR="${PX4_DIR:-$(dirname -- "$SIM_DIR")/PX4-Autopilot}"
export GZ_SIM_RESOURCE_PATH="$PX4_DIR/Tools/simulation/gz/models:$SIM_DIR/worlds${GZ_SIM_RESOURCE_PATH:+:$GZ_SIM_RESOURCE_PATH}"
export GZ_SIM_SYSTEM_PLUGIN_PATH="$PX4_DIR/build/px4_sitl_default/src/modules/simulation/gz_plugins${GZ_SIM_SYSTEM_PLUGIN_PATH:+:$GZ_SIM_SYSTEM_PLUGIN_PATH}"
export GZ_SIM_SERVER_CONFIG_PATH="$PX4_DIR/src/modules/simulation/gz_bridge/server.config"
exec gz sim -r "$SIM_DIR/worlds/tag_range.sdf" "$@"
