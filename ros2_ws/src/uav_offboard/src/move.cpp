#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>

#include <rclcpp/rclcpp.hpp>

#include <px4_msgs/msg/offboard_control_mode.hpp>
#include <px4_msgs/msg/trajectory_setpoint.hpp>
#include <px4_msgs/msg/vehicle_command.hpp>
#include <px4_msgs/msg/vehicle_local_position.hpp>
#include <px4_msgs/msg/vehicle_status.hpp>

using namespace std::chrono_literals;

class MoveDrone : public rclcpp::Node
{
public:
    MoveDrone() : Node("move_drone")
    {
        // 移动距离，单位：米。
        // PX4 使用 NED 坐标：X 北，Y 东，Z 向下。
        north_ = declare_parameter<double>("north", 1.0);
        east_  = declare_parameter<double>("east", 0.0);
        up_    = declare_parameter<double>("up", 0.0);

        auto sensor_qos = rclcpp::QoS(rclcpp::KeepLast(5))
                              .best_effort()
                              .durability_volatile();

        position_sub_ =
            create_subscription<px4_msgs::msg::VehicleLocalPosition>(
                "/fmu/out/vehicle_local_position",
                sensor_qos,
                [this](const px4_msgs::msg::VehicleLocalPosition::SharedPtr msg) {
                    position_ = *msg;
                    have_position_ = true;
                    position_received_ = std::chrono::steady_clock::now();
                });

        status_sub_ =
            create_subscription<px4_msgs::msg::VehicleStatus>(
                "/fmu/out/vehicle_status_v1",
                sensor_qos,
                [this](const px4_msgs::msg::VehicleStatus::SharedPtr msg) {
                    status_ = *msg;
                    have_status_ = true;
                    status_received_ = std::chrono::steady_clock::now();
                });

        mode_pub_ = create_publisher<px4_msgs::msg::OffboardControlMode>(
            "/fmu/in/offboard_control_mode", 10);

        setpoint_pub_ = create_publisher<px4_msgs::msg::TrajectorySetpoint>(
            "/fmu/in/trajectory_setpoint", 10);

        command_pub_ = create_publisher<px4_msgs::msg::VehicleCommand>(
            "/fmu/in/vehicle_command", 10);

        timer_ = create_wall_timer(100ms, [this]() { update(); });

        RCLCPP_INFO(
            get_logger(),
            "等待 PX4 状态和位置。请确认 MicroXRCEAgent 已启动。");
    }

private:
    enum class Stage {
        WAITING,
        PRESTREAM,
        SWITCHING,
        MOVING,
        STOPPED
    };

    uint64_t timestamp_us()
    {
    return static_cast<uint64_t>(
        get_clock()->now().nanoseconds() / 1000);
    }

    void stop_with_error(const char *message)
    {
        RCLCPP_ERROR(get_logger(), "%s", message);
        stage_ = Stage::STOPPED;
        timer_->cancel();
    }

    void publish_setpoint(const std::array<float, 3> &position)
    {
        const float nan = std::numeric_limits<float>::quiet_NaN();

        // 持续告诉 PX4：使用位置控制。
        px4_msgs::msg::OffboardControlMode mode{};
        mode.timestamp = timestamp_us();
        mode.position = true;
        mode_pub_->publish(mode);

        // 发布目标位置。
        px4_msgs::msg::TrajectorySetpoint setpoint{};
        setpoint.timestamp = timestamp_us();
        setpoint.position = position;

        // 不指定速度、加速度和偏航角。
        setpoint.velocity = {nan, nan, nan};
        setpoint.acceleration = {nan, nan, nan};
        setpoint.jerk = {nan, nan, nan};
        setpoint.yaw = nan;
        setpoint.yawspeed = nan;

        setpoint_pub_->publish(setpoint);
    }

    void request_offboard()
    {
        px4_msgs::msg::VehicleCommand command{};
        command.timestamp = timestamp_us();

        command.command =
            px4_msgs::msg::VehicleCommand::VEHICLE_CMD_DO_SET_MODE;

        command.param1 = 1.0f;
        command.param2 = 6.0f;  // PX4 Offboard 模式

        command.target_system = 1;
        command.target_component = 1;
        command.source_system = 1;
        command.source_component = 1;
        command.from_external = true;

        command_pub_->publish(command);
    }

    void update()
    {
        using Status = px4_msgs::msg::VehicleStatus;
        const auto now = std::chrono::steady_clock::now();

        if (!have_position_ || !have_status_) {
            return;
        }

        if (now - position_received_ > 3s ||
            now - status_received_ > 3s) {
            stop_with_error("PX4 状态或位置数据超时，已停止控制。");
            return;
        }

        if (!position_.xy_valid || !position_.z_valid ||
            !std::isfinite(position_.x) ||
            !std::isfinite(position_.y) ||
            !std::isfinite(position_.z)) {
            if (stage_ != Stage::WAITING) {
                stop_with_error("本地位置失效，已停止控制。");
            }
            return;
        }

        if (stage_ == Stage::WAITING) {
            if (status_.arming_state != Status::ARMING_STATE_ARMED) {
                stop_with_error(
                    "飞机尚未解锁。请先起飞并悬停，再运行本节点。");
                return;
            }

            // 记录接管时的位置。
            anchor_ = {position_.x, position_.y, position_.z};

            // up 为正表示上升，而 NED 的 Z 正方向是向下。
            target_ = {
                position_.x + static_cast<float>(north_),
                position_.y + static_cast<float>(east_),
                position_.z - static_cast<float>(up_)
            };

            RCLCPP_INFO(
                get_logger(),
                "当前位置 NED: [%.2f, %.2f, %.2f]",
                anchor_[0], anchor_[1], anchor_[2]);

            RCLCPP_INFO(
                get_logger(),
                "目标位置 NED: [%.2f, %.2f, %.2f]",
                target_[0], target_[1], target_[2]);

            stage_ = Stage::PRESTREAM;
            stage_started_ = now;
        }

        if (status_.arming_state != Status::ARMING_STATE_ARMED) {
            stop_with_error("飞机已上锁，停止控制。");
            return;
        }

        if (stage_ == Stage::PRESTREAM) {
            // 先连续发送当前位置，避免刚切换模式时立即移动。
            publish_setpoint(anchor_);

            if (now - stage_started_ >= 2s) {
                stage_ = Stage::SWITCHING;
                stage_started_ = now;
                last_request_ = now - 1s;
                RCLCPP_INFO(get_logger(), "正在请求 Offboard 模式。");
            }

            return;
        }

        if (stage_ == Stage::SWITCHING) {
            publish_setpoint(anchor_);

            if (status_.nav_state == Status::NAVIGATION_STATE_OFFBOARD) {
                stage_ = Stage::MOVING;
                RCLCPP_INFO(get_logger(), "已进入 Offboard，开始移动。");
            } else if (now - stage_started_ > 10s) {
                stop_with_error(
                    "切换 Offboard 超时，请检查 PX4 控制台。");
            } else if (now - last_request_ >= 1s) {
                request_offboard();
                last_request_ = now;
            }

            return;
        }

        if (stage_ == Stage::MOVING) {
            if (status_.nav_state != Status::NAVIGATION_STATE_OFFBOARD) {
                stop_with_error("飞机已退出 Offboard，停止控制。");
                return;
            }

            publish_setpoint(target_);

            const double dx = position_.x - target_[0];
            const double dy = position_.y - target_[1];
            const double dz = position_.z - target_[2];
            const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);

            if (distance < 0.3 && !reported_arrival_) {
                RCLCPP_INFO(
                    get_logger(),
                    "已到达目标附近，持续保持目标位置。");
                reported_arrival_ = true;
            }
        }
    }

    double north_{0.0};
    double east_{0.0};
    double up_{0.0};

    bool have_position_{false};
    bool have_status_{false};
    bool reported_arrival_{false};

    Stage stage_{Stage::WAITING};

    std::array<float, 3> anchor_{};
    std::array<float, 3> target_{};

    std::chrono::steady_clock::time_point position_received_{};
    std::chrono::steady_clock::time_point status_received_{};
    std::chrono::steady_clock::time_point stage_started_{};
    std::chrono::steady_clock::time_point last_request_{};

    px4_msgs::msg::VehicleLocalPosition position_{};
    px4_msgs::msg::VehicleStatus status_{};

    rclcpp::Subscription<
        px4_msgs::msg::VehicleLocalPosition>::SharedPtr position_sub_;

    rclcpp::Subscription<
        px4_msgs::msg::VehicleStatus>::SharedPtr status_sub_;

    rclcpp::Publisher<
        px4_msgs::msg::OffboardControlMode>::SharedPtr mode_pub_;

    rclcpp::Publisher<
        px4_msgs::msg::TrajectorySetpoint>::SharedPtr setpoint_pub_;

    rclcpp::Publisher<
        px4_msgs::msg::VehicleCommand>::SharedPtr command_pub_;

    rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<MoveDrone>());
    rclcpp::shutdown();
    return 0;
}