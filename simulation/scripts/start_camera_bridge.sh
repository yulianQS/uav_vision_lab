#!/usr/bin/env bash
set -euo pipefail
# Source /opt/ros/humble/setup.bash before running.
ros2 pkg prefix ros_gz_bridge >/dev/null || {
  echo 'Missing ros_gz_bridge. For this Humble + Harmonic installation use ros-humble-ros-gzharmonic-bridge.' >&2
  exit 1
}
BASE='/world/tag_range/model/x500_mono_cam_0/link/camera_link/sensor/imager'
exec ros2 run ros_gz_bridge parameter_bridge \
  "$BASE/image@sensor_msgs/msg/Image[gz.msgs.Image" \
  "$BASE/camera_info@sensor_msgs/msg/CameraInfo[gz.msgs.CameraInfo" \
  '/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock' \
  --ros-args \
  -r "$BASE/image:=/camera/image_raw" \
  -r "$BASE/camera_info:=/camera/camera_info" \
  -p "qos_overrides./camera/image_raw.publisher.reliability:=reliable" \
  -p "qos_overrides./camera/camera_info.publisher.reliability:=reliable"
