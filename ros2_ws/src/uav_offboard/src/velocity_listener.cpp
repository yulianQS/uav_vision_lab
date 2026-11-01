#include <memory>
#include <chrono>
#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>

#include <cmath>
#include <px4_msgs/msg/vehicle_attitude.hpp>


class VelocityListener : public rclcpp::Node
{
public:
    VelocityListener()
        : Node("velocity_listener")
    {
        // 创建订阅者，订阅 /keyboard/cmd_vel 话题。
        subscription_ =
            this->create_subscription<geometry_msgs::msg::Twist>(
                "/keyboard/cmd_vel",
                10,
                [this](geometry_msgs::msg::Twist::SharedPtr message) {
                    // 收到一条消息时，执行这个函数。
                    this->velocity_callback(message);
                }
            );


        auto attitude_qos = rclcpp::QoS(rclcpp::KeepLast(10)).best_effort();

        attitude_subscription_ =
            this->create_subscription<px4_msgs::msg::VehicleAttitude>(
                "/fmu/out/vehicle_attitude",
                attitude_qos,
                [this](px4_msgs::msg::VehicleAttitude::SharedPtr message) {
                    this->attitude_callback(message);
                }
            );
        watchdog_timer_ = this->create_wall_timer(std::chrono::milliseconds(100), [this]() { this->check_timeout(); });
        RCLCPP_INFO(this->get_logger(),"速度监听节点已启动，等待键盘消息。");
    }

private:
    void velocity_callback(const geometry_msgs::msg::Twist::SharedPtr message)
    {
        last_message_time_ = std::chrono::steady_clock::now();
        has_received_message_ = true;
        timeout_reported_ = false;

        target_vx_ = message->linear.x;
        target_vy_ = message->linear.y;
        target_vz_ = message->linear.z;

        const auto now = std::chrono::steady_clock::now();

        if (!has_attitude_ || now - last_attitude_time_ > std::chrono::milliseconds(500)) {
            RCLCPP_WARN(this->get_logger(), "姿态尚未收到或已经过期，暂不计算 NED 速度。");
            return;
        }

        print_ned_velocity(yaw_);

        RCLCPP_INFO(
            this->get_logger(),
            "收到速度：前=%.2f，右=%.2f，下=%.2f m/s",
            message->linear.x,
            message->linear.y,
            message->linear.z


        );
    }

    void check_timeout()
    {
        if (!has_received_message_ || timeout_reported_) {
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = now - last_message_time_;

        if (elapsed > std::chrono::milliseconds(1500)) {
            target_vx_ = 0.0;
            target_vy_ = 0.0;
            target_vz_ = 0.0;

            RCLCPP_WARN(this->get_logger(), "速度消息已超时，内部目标速度已归零。");
            RCLCPP_INFO(this->get_logger(), "当前目标：前=%.2f，右=%.2f，下=%.2f m/s", target_vx_, target_vy_, target_vz_);

            timeout_reported_ = true;
        }   
    }

    void print_ned_velocity(double yaw)
    {
        const double north = target_vx_ * std::cos(yaw) - target_vy_ * std::sin(yaw);
        const double east = target_vx_ * std::sin(yaw) + target_vy_ * std::cos(yaw);
        const double down = target_vz_;

        RCLCPP_INFO(this->get_logger(), "NED 速度：北=%.2f，东=%.2f，下=%.2f m/s", north, east, down);
    }

    void attitude_callback(const px4_msgs::msg::VehicleAttitude::SharedPtr message)
    {
        const double w = message->q[0];
        const double x = message->q[1];
        const double y = message->q[2];
        const double z = message->q[3];

        const double norm = std::sqrt(w * w + x * x + y * y + z * z);

        if (!std::isfinite(norm) || norm < 0.9 || norm > 1.1) {
            has_attitude_ = false;
            RCLCPP_WARN(this->get_logger(), "收到无效的姿态四元数。");
            return;
        }

        const double qw = w / norm;
        const double qx = x / norm;
        const double qy = y / norm;
        const double qz = z / norm;

        const double numerator = 2.0 * (qw * qz + qx * qy);
        const double denominator = 1.0 - 2.0 * (qy * qy + qz * qz);

        yaw_ = std::atan2(numerator, denominator);
        last_attitude_time_ = std::chrono::steady_clock::now();
        has_attitude_ = true;

        RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 2000, "姿态四元数：w=%.3f, x=%.3f, y=%.3f, z=%.3f", w, x, y, z);
        RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 2000, "偏航角 yaw：%.3f 弧度", yaw_);
  
    }

    // 把订阅者保存为类的成员。
    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr subscription_;
    rclcpp::Subscription<px4_msgs::msg::VehicleAttitude>::SharedPtr attitude_subscription_;

    rclcpp::TimerBase::SharedPtr watchdog_timer_;
    std::chrono::steady_clock::time_point last_message_time_;
    bool has_received_message_ = false;
    bool timeout_reported_ = false;

    double target_vx_ = 0.0;
    double target_vy_ = 0.0;
    double target_vz_ = 0.0;

    double yaw_ = 0.0;
    bool has_attitude_ = false;
    std::chrono::steady_clock::time_point last_attitude_time_;

};


int main(int argc, char *argv[])
{
    // 初始化 ROS 2。
    rclcpp::init(argc, argv);

    // 创建节点对象。
    auto node = std::make_shared<VelocityListener>();

    // 持续等待消息，并在消息到达时执行回调。
    rclcpp::spin(node);

    // 关闭 ROS 2。
    rclcpp::shutdown();

    return 0;
}