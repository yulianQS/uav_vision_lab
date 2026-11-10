#include <memory>
#include <rclcpp/rclcpp.hpp>

#include <uav_interfaces/msg/velocity_status.hpp>
#include <chrono>



class VelocityMonitor : public rclcpp::Node
{
public:
    VelocityMonitor() : Node("velocity_monitor")
    {
        status_subscription_ = this->create_subscription<uav_interfaces::msg::VelocityStatus>(
            "/practice/velocity_status",
            10,
            [this](uav_interfaces::msg::VelocityStatus::SharedPtr message) {
                if (state_ == MonitorState::WAITING) {
                    RCLCPP_INFO(this->get_logger(), "已收到第一条速度状态消息。");
                } else if (state_ == MonitorState::TIMED_OUT) {
                    RCLCPP_INFO(this->get_logger(), "已恢复接收速度状态消息。");
                }
                last_message_time_ = std::chrono::steady_clock::now();
                state_ = MonitorState::RECEIVING;


                RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
                    "速度 NED：北=%.2f，东=%.2f，下=%.2f m/s；输入新鲜：%s",
                    message->velocity.x, message->velocity.y, message->velocity.z,
                    message->input_fresh ? "是" : "否");
            });


        watchdog_timer_ = this->create_wall_timer(
            std::chrono::milliseconds(100),
            [this]() {
                if (state_ == MonitorState::WAITING) {
                    const auto waiting_time = std::chrono::steady_clock::now() - started_at_;

                    if (waiting_time > std::chrono::seconds(3) && !waiting_reported_) {
                        RCLCPP_WARN(this->get_logger(), "启动后尚未收到速度状态消息。");
                        waiting_reported_ = true;
                    }
                    return;
                }

                if (state_ == MonitorState::TIMED_OUT) {
                    return;
                }

                const auto elapsed = std::chrono::steady_clock::now() - last_message_time_;

                if (elapsed > std::chrono::milliseconds(1500)) {
                    RCLCPP_WARN(this->get_logger(), "超过 1.5 秒没有收到速度状态消息。");
                    state_ = MonitorState::TIMED_OUT;
                }
            });

        RCLCPP_INFO(this->get_logger(), "速度监控节点已启动。");
    }
private:
    rclcpp::Subscription<uav_interfaces::msg::VelocityStatus>::SharedPtr status_subscription_;   
    
    rclcpp::TimerBase::SharedPtr watchdog_timer_;

    std::chrono::steady_clock::time_point last_message_time_{};

    std::chrono::steady_clock::time_point started_at_ = std::chrono::steady_clock::now();
    bool waiting_reported_ = false;

    enum class MonitorState { WAITING, RECEIVING, TIMED_OUT };
    MonitorState state_ = MonitorState::WAITING;
};

int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);

    auto node = std::make_shared<VelocityMonitor>();
    rclcpp::spin(node);

    rclcpp::shutdown();
    return 0;
}