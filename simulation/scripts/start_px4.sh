#!/usr/bin/env bash
set -euo pipefail
SIM_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
PX4_DIR="${PX4_DIR:-$(dirname -- "$SIM_DIR")/PX4-Autopilot}"
cd "$PX4_DIR"
export PX4_GZ_STANDALONE=1
# A copied build may contain CMake paths for a different checkout. Do not
# silently rebuild that checkout; reuse the local binary in standalone mode.
CACHE="$PX4_DIR/build/px4_sitl_default/CMakeCache.txt"
if [[ -f "$CACHE" ]] && ! grep -Fxq "CMAKE_HOME_DIRECTORY:INTERNAL=$PX4_DIR" "$CACHE"; then
  ROOTFS="$PX4_DIR/build/px4_sitl_default/rootfs"
  BIN="$PX4_DIR/build/px4_sitl_default/bin/px4"
  if [[ ! -x "$BIN" || ! -d "$ROOTFS/etc" ]]; then
    echo 'PX4 构建缓存属于其他目录，且本地可执行文件或 rootfs 不完整。请在当前目录重新构建 PX4。' >&2
    exit 1
  fi
  echo 'PX4 构建缓存来自其他目录，使用当前目录中的现有 SITL 可执行文件。' >&2
  export PX4_GZ_MODELS="$PX4_DIR/Tools/simulation/gz/models"
  export PX4_SYS_AUTOSTART=4010
  export PX4_SIM_MODEL=gz_x500_mono_cam
  cd "$ROOTFS"
  exec "$BIN"
fi
exec make px4_sitl gz_x500_mono_cam
