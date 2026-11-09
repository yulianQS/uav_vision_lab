# 无人机 ROS 2 单目视觉仿真实验

基于 PX4、ROS 2 和 Gazebo，学习前视单目相机识别 AprilTag、估计目标相对位姿，并逐步实现视觉引导动作。

## 目录

- `PX4-Autopilot/`：官方 PX4 子模块，固定在本实验使用的提交版本。
- `ros2_ws/src/`：PX4 消息定义，以及后续 ROS 2 感知和控制节点。
- `dependencies/`：依赖清单和 Micro XRCE-DDS Agent 源码子模块。
- `simulation/`：AprilTag 场景、检测配置和启动脚本。

## 获取完整工程

```bash
git clone --recurse-submodules <本仓库地址>
```

如果已克隆主仓库：

```bash
git submodule update --init --recursive
```

依赖安装与恢复步骤见 [开发环境与依赖](dependencies/环境依赖.md)，运行步骤见 [仿真实验说明](simulation/README.md)。已验证相机图像、Tag 检测和无需地面站的 SITL Offboard 自动起飞；视觉闭环动作尚未验证。

## ROS 2 节点代码结构

`ros2_ws/src/uav_offboard/src/` 中每个节点的程序入口和实现保存在对应的 `.cpp` 文件中：
`keyboard_test.cpp` 处理键盘输入，`velocity_listener.cpp` 监听并发布练习速度，
`tag_reader.cpp` 读取标签位置，`takeoff.cpp` 和 `move.cpp` 分别处理仿真起飞与移动。
可复用的速度坐标转换放在 `velocity_transform.hpp` 和 `velocity_transform.cpp` 中。
各节点沿用原有的 `ros2 run` 可执行程序名称。

## 提交约定

每次提交使用中文标题和中文说明，交代修改内容、目的和验证结果。
