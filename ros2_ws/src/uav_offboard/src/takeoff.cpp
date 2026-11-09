
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>

#include <rclcpp/rclcpp.hpp>
#include <px4_msgs/msg/offboard_control_mode.hpp>
#include <px4_msgs/msg/trajectory_setpoint.hpp>
#include <px4_msgs/msg/vehicle_command.hpp>
#include <px4_msgs/msg/vehicle_command_ack.hpp>
#include <px4_msgs/msg/vehicle_local_position.hpp>
#include <px4_msgs/msg/vehicle_status.hpp>

using namespace std::chrono_literals;

class OffboardTakeoff : public rclcpp::Node {
public:
  OffboardTakeoff() : Node("offboard_takeoff") {
    if (!declare_parameter<bool>("confirm_simulation", false)) {
      throw std::runtime_error("仅用于 SITL 仿真：须显式设置 confirm_simulation:=true");
    }
    height_ = declare_parameter<double>("height", 2.0);
    if (!std::isfinite(height_) || height_ < 0.5 || height_ > 5.0) {
      throw std::runtime_error("height 必须在 0.5 至 5.0 米之间");
    }
    auto qos = rclcpp::QoS(rclcpp::KeepLast(10)).best_effort().durability_volatile();
    position_sub_ = create_subscription<px4_msgs::msg::VehicleLocalPosition>(
      "/fmu/out/vehicle_local_position", qos, [this](px4_msgs::msg::VehicleLocalPosition::SharedPtr msg) {
        position_ = *msg; position_time_ = std::chrono::steady_clock::now(); have_position_ = true;
      });
    status_sub_ = create_subscription<px4_msgs::msg::VehicleStatus>(
      "/fmu/out/vehicle_status_v1", qos, [this](px4_msgs::msg::VehicleStatus::SharedPtr msg) {
        status_ = *msg; status_time_ = std::chrono::steady_clock::now(); have_status_ = true;
      });
    ack_sub_ = create_subscription<px4_msgs::msg::VehicleCommandAck>(
      "/fmu/out/vehicle_command_ack", qos, [this](px4_msgs::msg::VehicleCommandAck::SharedPtr msg) {
        using Ack = px4_msgs::msg::VehicleCommandAck;
        if ((msg->command == px4_msgs::msg::VehicleCommand::VEHICLE_CMD_COMPONENT_ARM_DISARM ||
             msg->command == px4_msgs::msg::VehicleCommand::VEHICLE_CMD_DO_SET_MODE) &&
            msg->result != Ack::VEHICLE_CMD_RESULT_ACCEPTED &&
            msg->result != Ack::VEHICLE_CMD_RESULT_IN_PROGRESS) {
          RCLCPP_WARN(get_logger(), "PX4 拒绝命令 %u，结果 %u；请检查 PX4 控制台的健康检查", msg->command, msg->result);
        }
      });
    mode_pub_ = create_publisher<px4_msgs::msg::OffboardControlMode>("/fmu/in/offboard_control_mode", 10);
    setpoint_pub_ = create_publisher<px4_msgs::msg::TrajectorySetpoint>("/fmu/in/trajectory_setpoint", 10);
    command_pub_ = create_publisher<px4_msgs::msg::VehicleCommand>("/fmu/in/vehicle_command", 10);
    timer_ = create_wall_timer(100ms, [this] { update(); });
    RCLCPP_INFO(get_logger(), "仿真起飞节点已启动，等待 PX4 状态与有效本地位置。");
  }
private:
  using Clock = std::chrono::steady_clock;
  using Status = px4_msgs::msg::VehicleStatus;
  using Command = px4_msgs::msg::VehicleCommand;
  // 起飞顺序：等待有效状态、预发送目标、切换模式、解锁、爬升。
  enum class Stage { WAIT, PRESTREAM, MODE, ARM, CLIMB, STOP };
  uint64_t timestamp() { return static_cast<uint64_t>(get_clock()->now().nanoseconds() / 1000); }
  void fail(const char *message) {
    RCLCPP_ERROR(get_logger(), "%s", message); stage_ = Stage::STOP; timer_->cancel();
  }
  // 持续发送 Offboard 心跳和位置目标；未使用的控制量设为 NaN。
  void publish(const std::array<float, 3> &point) {
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    px4_msgs::msg::OffboardControlMode mode{};
    mode.timestamp = timestamp(); mode.position = true; mode_pub_->publish(mode);
    px4_msgs::msg::TrajectorySetpoint sp{};
    sp.timestamp = timestamp(); sp.position = point;
    sp.velocity = {nan, nan, nan}; sp.acceleration = {nan, nan, nan}; sp.jerk = {nan, nan, nan};
    sp.yaw = nan; sp.yawspeed = nan; setpoint_pub_->publish(sp);
  }
  void command(uint32_t id, float p1, float p2) {
    Command cmd{}; cmd.timestamp = timestamp(); cmd.command = id; cmd.param1 = p1; cmd.param2 = p2;
    cmd.target_system = 1; cmd.target_component = 1; cmd.source_system = 1; cmd.source_component = 1;
    cmd.from_external = true; command_pub_->publish(cmd);
  }
  // 定时推进状态机；状态失效时停止心跳，由 PX4 的失联保护接管。
  void update() {
    const auto now = Clock::now();
    if (!have_position_ || !have_status_) return;
    if (now - position_time_ > 3s || now - status_time_ > 3s) {
      if (stage_ != Stage::WAIT) fail("PX4 状态或位置超时，停止心跳并交由 PX4 失控保护处理。");
      return;
    }
    if (!position_.xy_valid || !position_.z_valid || !std::isfinite(position_.x) ||
        !std::isfinite(position_.y) || !std::isfinite(position_.z)) {
      if (stage_ != Stage::WAIT) fail("本地位置失效，停止心跳并交由 PX4 失控保护处理。");
      return;
    }
    if (stage_ == Stage::WAIT) {
      if (status_.arming_state == Status::ARMING_STATE_ARMED) {
        fail("飞机已解锁，本节点仅支持从上锁状态起飞。"); return;
      }
      anchor_ = {position_.x, position_.y, position_.z};
      target_ = {position_.x, position_.y, position_.z - static_cast<float>(height_)};
      stage_ = Stage::PRESTREAM; stage_time_ = now;
      RCLCPP_INFO(get_logger(), "准备起飞 %.1f 米，预发送 Offboard 心跳。", height_);
    }
    if (stage_ == Stage::PRESTREAM) {
      publish(anchor_);
      if (now - stage_time_ >= 2s) {
        stage_ = Stage::MODE; stage_time_ = now; last_request_ = now - 1s;
        RCLCPP_INFO(get_logger(), "请求 Offboard 模式。");
      }
      return;
    }
    if (stage_ == Stage::MODE) {
      publish(anchor_);
      if (status_.nav_state == Status::NAVIGATION_STATE_OFFBOARD) {
        stage_ = Stage::ARM; stage_time_ = now; last_request_ = now - 1s;
        RCLCPP_INFO(get_logger(), "已进入 Offboard，等待健康检查后请求解锁。");
      } else if (now - stage_time_ > 15s) {
        fail("切换 Offboard 超时，请检查 PX4 控制台。");
      } else if (now - last_request_ >= 1s) {
        command(Command::VEHICLE_CMD_DO_SET_MODE, 1.f, 6.f); last_request_ = now;
      }
      return;
    }
    if (stage_ == Stage::ARM) {
      publish(anchor_);
      if (status_.nav_state != Status::NAVIGATION_STATE_OFFBOARD) {
        fail("解锁前退出 Offboard，停止控制。");
      } else if (status_.arming_state == Status::ARMING_STATE_ARMED) {
        stage_ = Stage::CLIMB;
        RCLCPP_INFO(get_logger(), "解锁成功，开始起飞并保持目标高度。");
      } else if (now - stage_time_ > 30s) {
        fail("等待解锁超时，请查看 PX4 健康检查。");
      } else if (status_.pre_flight_checks_pass && now - last_request_ >= 1s) {
        command(Command::VEHICLE_CMD_COMPONENT_ARM_DISARM, 1.f, 0.f); last_request_ = now;
      }
      return;
    }
    if (stage_ == Stage::CLIMB) {
      if (status_.arming_state != Status::ARMING_STATE_ARMED ||
          status_.nav_state != Status::NAVIGATION_STATE_OFFBOARD || status_.failsafe) {
        fail("飞机上锁、退出 Offboard 或进入失效保护，停止控制。"); return;
      }
      publish(target_);
      if (!reported_ && std::fabs(position_.z - target_[2]) < 0.3f) {
        reported_ = true; RCLCPP_INFO(get_logger(), "已到达目标高度附近，持续悬停。");
      }
    }
  }
  double height_{}; bool have_position_{false}, have_status_{false}, reported_{false};
  Stage stage_{Stage::WAIT}; std::array<float, 3> anchor_{}, target_{};
  Clock::time_point position_time_{}, status_time_{}, stage_time_{}, last_request_{};
  px4_msgs::msg::VehicleLocalPosition position_{}; Status status_{};
  rclcpp::Subscription<px4_msgs::msg::VehicleLocalPosition>::SharedPtr position_sub_;
  rclcpp::Subscription<Status>::SharedPtr status_sub_;
  rclcpp::Subscription<px4_msgs::msg::VehicleCommandAck>::SharedPtr ack_sub_;
  rclcpp::Publisher<px4_msgs::msg::OffboardControlMode>::SharedPtr mode_pub_;
  rclcpp::Publisher<px4_msgs::msg::TrajectorySetpoint>::SharedPtr setpoint_pub_;
  rclcpp::Publisher<Command>::SharedPtr command_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

// 程序入口：初始化 ROS 2，运行节点，退出时释放资源。
int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  try { rclcpp::spin(std::make_shared<OffboardTakeoff>()); }
  catch (const std::exception &e) { RCLCPP_ERROR(rclcpp::get_logger("offboard_takeoff"), "%s", e.what()); rclcpp::shutdown(); return 1; }
  rclcpp::shutdown(); return 0;
}
