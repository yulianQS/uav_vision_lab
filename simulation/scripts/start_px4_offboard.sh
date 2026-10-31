#!/usr/bin/env bash
set -euo pipefail
# SITL only: do not use these parameters on an aircraft.
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "$SCRIPT_DIR/../.." && pwd)"
PX4_DIR="${PX4_DIR:-$ROOT_DIR/PX4-Autopilot}"
if pgrep -x px4 >/dev/null; then
  echo '已有 PX4 实例运行；请先关闭该实例，再使用仿真专用脚本启动。' >&2
  exit 1
fi
export PX4_PARAM_COM_RC_IN_MODE=4
# rcS applies the x500 airframe's NAV_DLL_ACT default after environment
# overrides. Set it only after commander is running so that 0 takes effect.
launcher_pid=$$
(
  for ((i=0; i<120; i++)); do
    kill -0 "$launcher_pid" 2>/dev/null || exit 0
    if (cd "$PX4_DIR/build/px4_sitl_default/rootfs" && \
        ../bin/px4-commander status >/dev/null 2>&1); then
      if (cd "$PX4_DIR/build/px4_sitl_default/rootfs" && \
          ../bin/px4-param set NAV_DLL_ACT 0); then
        echo 'SITL：已关闭地面站失联动作；Offboard 失联保护仍然生效。' >&2
      else
        echo 'SITL：设置 NAV_DLL_ACT 失败，请检查 PX4 控制台。' >&2
      fi
      exit 0
    fi
    sleep 1
  done
  echo 'SITL：等待 PX4 commander 超时，未设置 NAV_DLL_ACT。' >&2
) &
echo 'SITL 专用：忽略手动摇杆输入，并在启动后关闭地面站失联动作。' >&2
exec "$SCRIPT_DIR/start_px4.sh" "$@"
