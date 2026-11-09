#include "uav_offboard/velocity_transform.hpp"

#include <cmath>

namespace uav_offboard
{
    // 假设飞机近似水平，仅使用偏航角将机体水平速度旋转到 NED 坐标系。
    NedVelocity body_to_ned(double forward, double right, double down, double yaw)
    {
        NedVelocity result;
        result.north = forward * std::cos(yaw) - right * std::sin(yaw);
        result.east = forward * std::sin(yaw) + right * std::cos(yaw);
        result.down = down;
        return result;
    }
}
