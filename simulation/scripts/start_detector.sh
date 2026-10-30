#!/usr/bin/env bash
set -euo pipefail
SIM_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
exec ros2 run apriltag_ros apriltag_node --ros-args \
  -r image_rect:=/camera/image_raw \
  -r camera_info:=/camera/camera_info \
  --params-file "$SIM_DIR/config/apriltag.yaml"
