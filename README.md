# 无人机 ROS 2 单目视觉仿真实验

基于 PX4、ROS 2 和 Gazebo，学习前视单目相机识别 AprilTag、估计目标相对位姿，并逐步实现视觉引导动作。

## 目录

- `PX4-Autopilot/`：官方 PX4 子模块，固定在本实验使用的提交版本。
- `ros2_ws/src/`：后续 ROS 2 感知和控制节点。
- `simulation/`：AprilTag 场景、检测配置和启动脚本。

## 获取完整工程

```bash
git clone --recurse-submodules <本仓库地址>
```

如果已克隆主仓库：

```bash
git submodule update --init --recursive
```

完整安装与运行步骤见 [仿真实验说明](simulation/README.md)。目前已完成场景文件和脚本的基础检查，实时检测与闭环动作尚未验证。

## 提交约定

每次提交使用中文标题和中文说明，交代修改内容、目的和验证结果。
