#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
AGENT="$ROOT_DIR/.local/bin/MicroXRCEAgent"
if [[ ! -x "$AGENT" ]]; then
  echo "未找到本项目的 MicroXRCEAgent：$AGENT；请先按 dependencies/环境依赖.md 构建。" >&2
  exit 1
fi
export LD_LIBRARY_PATH="$ROOT_DIR/.local/lib:$ROOT_DIR/.local/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$AGENT" udp4 -p 8888 "$@"
