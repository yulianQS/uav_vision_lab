# PX4 / ROS 2 前视单目 AprilTag 实验

推荐用本地 PX4 v1.16.0 的 `x500_mono_cam`，配合 Gazebo Harmonic 和 ROS 2 Humble。
前视相机直接复用 PX4 中的模型，场景和配置保存在本目录。

## 已准备内容

```text
simulation/
├── worlds/tag_range.sdf          # 平地 + 三块朝向起点的竖直 Tag 板
├── worlds/PX4-LICENSE            # 基础世界的上游许可证
├── assets/apriltag/              # 官方 Tag 图像、许可证、下载来源和 SHA256
├── config/apriltag.yaml          # 36h11，黑色外边框边长 0.8 m
├── config/server.config          # 仿真插件配置，关闭 GStreamer 视频流
└── scripts/
    ├── build_world.py            # 使用官方图像生成 SDF；仅重新生成时依赖 Pillow
    ├── start_world.sh
    ├── start_px4.sh
    ├── start_px4_offboard.sh   # 无地面站的 SITL 配置
    ├── start_camera_bridge.sh
    ├── start_detector.sh
    └── start_agent.sh
```

靶标的 Gazebo 世界坐标（米，Z 向上）：

| Tag ID | X | Y | Z（板中心） | 黑色外边框边长 |
| --- | --- | --- | --- | --- |
| 0 | 8 | 0 | 2 | 0.8 m |
| 1 | 12 | 4 | 2 | 0.8 m |
| 2 | 16 | -4 | 2 | 0.8 m |

白色留边在内的板宽为 1 m；检测器的 `size` 填 0.8 m，不能填 1 m。
板面朝 -X，飞机初始前方为 +X。先起飞到约 2 m 再观察；停在地面时部分靶标可能在视野外。
黑色格子直接用几何体表达，避免贴图 UV 方向与纹理滤波问题；白色留边完整保留。
这是用于学习的理想光照、无图像噪声场景，距离不是检测成功率保证。

## 上游环境依赖检查

以下为原项目环境的记录，并非本次机器的验证结果。
2026-10-30 检查结果：Ubuntu 22.04.5、ROS 2 Humble、PX4 v1.16.0、Gazebo Sim 8.15.0；
`apriltag_ros/apriltag_node`、`MicroXRCEAgent`、`ros_gz_bridge` 和 PX4 SITL 可执行文件已存在。

当前 Humble + Harmonic 组合的桥接包来自 Gazebo 软件源：

```bash
sudo apt update
sudo apt install ros-humble-ros-gzharmonic-bridge
```

Humble 默认的 `ros-humble-ros-gz-bridge` 通常对应 Fortress；
本实验要用与 Harmonic 匹配的桥接版本。

## 启动顺序（各开一个终端）

在项目根目录执行下面的命令。若需要无地面站自动起飞，请按下方「无 QGroundControl 的 Offboard 仿真起飞」启动 PX4，不要同时启动两个 PX4 实例。
启动前关闭其他 PX4/Gazebo 仿真，避免 PX4 连接到其他世界。

1. 启动场景：

   ```bash
   ./simulation/scripts/start_world.sh
   ```

若 Gazebo 相机渲染报 OpenGL 3.3 错误，
可在桌面会话中启动仅服务器模式并启用软件渲染：

   ```bash
   LIBGL_ALWAYS_SOFTWARE=1 ./simulation/scripts/start_world.sh -s
   ```

软件渲染帧率可能较低；桌面终端通常已设置 `DISPLAY`。

2. 启动 PX4 并生成前视相机飞机：

   ```bash
   ./simulation/scripts/start_px4.sh
   ```

3. 桥接相机图像、CameraInfo 和仿真时钟：

   ```bash
   source /opt/ros/humble/setup.bash
   ./simulation/scripts/start_camera_bridge.sh
   ```

4. 启动 AprilTag 检测：

   ```bash
   source /opt/ros/humble/setup.bash
   ./simulation/scripts/start_detector.sh
   ```

5. 后续需要通过 ROS 2 控制 PX4 时，再启动 DDS Agent：

   ```bash
   ./simulation/scripts/start_agent.sh
   ```

PX4 和世界使用 standalone 方式连接，无需往 PX4 的 worlds 目录复制文件。
脚本默认使用相邻的 `PX4-Autopilot`；可通过 `PX4_DIR` 指定其他 checkout。
场景默认使用本项目的 `config/server.config`，其中关闭了 PX4 的 GStreamer 相机流插件；
Gazebo 相机和 ROS 桥接仍可使用。此机器上的对照测试显示该插件启用时 Gazebo 内存持续增长。
需要其他插件配置时，可显式设置 `GZ_SIM_SERVER_CONFIG_PATH`。
如果现有 CMake 缓存属于其他路径，脚本会提示并使用当前目录已有的 SITL 二进制文件，
避免错误地重建旧路径。需要重新编译 PX4 改动时，请先在当前 checkout 重新配置构建。
检测脚本把理想、无畸变的 Gazebo 原图接到 `image_rect`；接真实相机时需要先校正图像。

## 检查图像和检测结果

```bash
source /opt/ros/humble/setup.bash
gz topic -l
ros2 topic list
ros2 topic hz /camera/image_raw
ros2 topic echo /camera/camera_info --once
ros2 topic echo /detections
ros2 topic echo /tf
```

相机 Gazebo 话题基路径默认是：
`/world/tag_range/model/x500_mono_cam_0/link/camera_link/sensor/imager`。
如果没有图像，先从 `gz topic -l` 确认实际名称，实例编号不是 0 时修改桥接脚本。
图像和 CameraInfo 必须有匹配时间戳；`/clock` 用于 ROS 仿真时间。
`apriltag_ros` 在 `/detections` 发布 ID 等检测信息，通过 `/tf` 发布相机到 Tag 的相对位姿。

## 单目定位与动作实验

建议按以下顺序学习，每一步先确认可观测结果：

1. 使用下文的 Offboard 起飞节点升至约 2 m 并悬停，确认前视画面和 ID 0。
2. 检查 Tag 的相机相对位置，正前方距离约 8 m（会受到相机安装偏移影响）。
3. 用 ROS 2 Offboard 完成固定位置的起飞、悬停、降落，再接入视觉反馈。
4. 根据 Tag 的横向偏差控制航向，缓慢靠近并停在距板约 3 m 的位置。
5. 加入 ID 对应的动作，例如 ID 0 悬停、ID 1 转向、ID 2 返航；加入连续多帧确认和丢失目标后的悬停逻辑。

这里的“单目”指目标感知只用一台相机；默认 X500 仍使用仿真 GPS/IMU 等做飞行状态估计。
已知 Tag 实际尺寸 + 相机内参，可以从四个角点估计相机到 Tag 的米制相对位姿。
若要把它变为世界中的目标位置，还需要飞机位姿、相机安装外参和时间同步。
这不等同于无 GPS 的纯视觉导航；后者还需要 VIO/SLAM 或基于已知 Tag 地图的定位。

坐标处理时区分：相机光学坐标 X 向右、Y 向下、Z 向前；ROS 常用 ENU/FLU；PX4 常用 NED/FRD。
检查 Gazebo 图像 header 的 frame_id 是否确实表示 optical frame，再建立相机到机体的 TF，不能直接把检测平移量当 PX4 位置指令。
ROS 控制侧需要 `px4_msgs` 与 PX4 v1.16 消息定义兼容；优先核对 `release/1.16` 分支。
进入 Offboard 前持续发送控制心跳与 setpoint，之后持续保持。`uav_offboard` 的 `move_drone` 节点要求飞机已解锁并悬停；`offboard_takeoff` 是独立的仿真起飞节点。

## 远距离识别

当前官方相机：1280×960，水平视场角约 1.74 rad（约 100°），对应 fx 约 540 像素。
正面近似投影边长为 `pixels ≈ fx × tag_size / distance`。
0.8 m Tag 在 8 m、16 m 处分别约 54、27 像素宽，仅是几何估算。
斜视、遮挡、采样和运动都会影响识别与深度估计；需要通过实验确定有效范围。
可先用 0.8–1.0 m 大 Tag 跑通，再研究更小的 Tag、更高分辨率或更窄视场角。

## 来源与可参考项目

- PX4 官方模型：https://github.com/PX4/PX4-gazebo-models 。本地已包含相机和 X500，无需重复下载整套模型。
- PX4 v1.16 ROS 2 文档：https://docs.px4.io/v1.16/en/ros2/user_guide
- ROS / Gazebo 桥接：https://github.com/gazebosim/ros_gz ，其版本表包含 Humble + Harmonic。
- ROS 2 AprilTag 检测：https://github.com/christianrauch/apriltag_ros
- 官方 Tag 图像：https://github.com/AprilRobotics/apriltag-imgs 。已下载 36h11 ID 0–2，保存许可证和校验值。
- 完整任务参考：https://github.com/09priyamgupta/apriltag_precision_landing_px4 。主要为下视精准降落，可学习感知与控制连接方式，前视任务需要调整场景、外参和控制逻辑。

世界以本地 PX4 的 default.sdf 为基础生成，许可证见 `worlds/PX4-LICENSE`。
本地 PX4 commit：`6ea3539157ca358c70a515878b77077af7d4611d`；模型子模块 commit：`e05f4312d3f28aa621157610584a4870406cb6d3`。

## 上游验证范围

以下为原项目环境的历史验证记录。
2026-10-30 实际启动 Gazebo 场景、PX4 前视飞机、ROS 图像桥接和 AprilTag 节点；
已识别 ID 0、1、2，并通过 TF 得到 ID 0 前向距离约 7.85 m。Micro XRCE-DDS Agent 与 PX4 建立会话。
虚拟机中使用软件渲染，测试图像约 1 Hz，偶有图像和内参同步警告；
尚未测试起飞、Offboard 指令和视觉闭环动作。SDF 和 Shell 脚本基础检查通过。

## 本机复现记录

2026-10-31：Ubuntu 22.04、ROS 2 Humble、Gazebo Sim 8.15.0，RTX 4060（OpenGL 4.6）。
已编译 PX4 SITL，启动场景及前视飞机，收到 1280×960 图像，约 29.7 Hz；检测到 ID 0、1、2，
TF 中 ID 0 前向距离约 7.93 m。DDS Agent 与 PX4 建立会话，ROS 读取到 `/fmu/out/vehicle_local_position` 的有效位置数据。
`px4_msgs` 与 `uav_offboard` 均已在本机编译。
后续已补充无地面站 Offboard 起飞验证，见下文；尚未验证视觉闭环。

2026-10-31 内存排查：在 3 GiB 上限的无 GUI 对照测试中，原 GStreamer 插件启用时
进程组内存从约 0.3 GiB 持续增长至 2.8 GiB；禁用后约 0.5 GiB 保持稳定。
加入 ROS 相机桥接及 AprilTag 检测后约 0.7 GiB，检测到 ID 0、1、2。
带 GUI 的场景和 PX4 短时测试中约 0.84 GiB，内存保持平稳。
此项测试时间较短，长期稳定性仍需观察；默认不提供 GStreamer 视频流。

## 无 QGroundControl 的 Offboard 仿真起飞

**仅用于本项目 PX4 SITL 仿真，不用于真实飞机。** 在项目根目录分别开启终端：

1. 启动场景：`./simulation/scripts/start_world.sh`。
2. 启动 PX4：`./simulation/scripts/start_px4_offboard.sh`。等待启动完成；不要再启动第二个 PX4。
3. 启动 DDS Agent：`./simulation/scripts/start_agent.sh`。
4. 另开终端编译并启动起飞节点：

   ```bash
   # 在项目根目录执行
   source /opt/ros/humble/setup.bash
   cd ros2_ws
   colcon build --packages-up-to uav_offboard
   source install/setup.bash
   ros2 run uav_offboard offboard_takeoff --ros-args -p confirm_simulation:=true -p height:=2.0
   ```

节点等待有效的本地位置和状态，先发送 2 秒 Offboard 心跳，再请求模式切换，等待健康检查通过后解锁，上升至相对初始位置 2 米并持续悬停。`height` 范围为 0.5–5 米。节点必须持续运行；状态超时或退出 Offboard 时会停止发送控制信号，PX4 会按照其 Offboard 失联配置接管。该节点不会自动降落。

需要结束飞行时，在 PX4 的 `pxh>` 提示符输入 `commander land`，观察飞机落地、上锁后再停止各终端。降落时飞机退出 Offboard，起飞节点会提示已停止控制，这是模式切换后的预期行为。手动停止起飞节点会中断 Offboard 心跳并触发 PX4 失联保护，不应将其作为正常降落方式。

仿真专用启动脚本会将 SITL 的 `NAV_DLL_ACT` 设为 0（关闭地面站失联动作），并将 `COM_RC_IN_MODE` 设为 4（忽略摇杆输入）；这两个设置会保存在该 SITL 构建目录的参数中。其他健康检查和 Offboard 信号丢失保护仍然适用。不要在真实飞机使用这些参数或本节点。

验证记录：2026-10-31 在 Gazebo Harmonic / PX4 v1.16 SITL 中未连接 QGroundControl，通过 DDS Agent 自动进入 Offboard、解锁并起飞；读取本地 NED 坐标 `z ≈ -1.97 m`，节点报告到达 2 m 目标附近。随后发送 `commander land`，确认 PX4 上锁。视觉反馈闭环和自动降落尚未验证。
