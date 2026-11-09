//声明：有哪些转换函数和数据


#pragma once

namespace uav_offboard
{
 struct NedVelocity
 {
     double north;
     double east;
     double down;
 };

NedVelocity body_to_ned(double forward, double right, double down, double yaw); 

};

