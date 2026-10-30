#include <chrono>
#include <memory>

#include <rclcpp/rclcpp.hpp>
#include <tf2_msgs/msg/tf_message.hpp>

using std::placeholders::_1;

class TagReader : public rclcpp::Node
{
public:
    TagReader() : Node("tag_reader")
    {
        // 订阅 ROS 2 的 TF 话题。
        tf_sub_ = create_subscription<tf2_msgs::msg::TFMessage>(
            "/tf",
            rclcpp::QoS(50),
            std::bind(&TagReader::tf_callback, this, _1));

        RCLCPP_INFO(get_logger(), "正在等待 0 号 Tag 的位置...");
    }

private:
    void tf_callback(const tf2_msgs::msg::TFMessage::SharedPtr msg)
    {
        // 一条 TF 消息中可能包含多个坐标变换。
        for (const auto &transform : msg->transforms) {
            // 只处理 0 号 Tag。
            if (transform.child_frame_id != "tag36h11:0") {
                continue;
            }

            const auto &position = transform.transform.translation;

            // 最多每 2 秒打印一次，避免终端输出过快。
            RCLCPP_INFO_THROTTLE(
                get_logger(),
                *get_clock(),
                2000,
                "0号Tag | 相机坐标：x=%.2f m, y=%.2f m, z=%.2f m",
                position.x,
                position.y,
                position.z);
        }
    }

    rclcpp::Subscription<tf2_msgs::msg::TFMessage>::SharedPtr tf_sub_;
};

int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);

    auto node = std::make_shared<TagReader>();
    rclcpp::spin(node);

    rclcpp::shutdown();
    return 0;
}