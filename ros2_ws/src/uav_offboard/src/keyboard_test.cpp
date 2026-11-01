#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>

int main(int argc, char *argv[])
{
    // 初始化 ROS 2。
    rclcpp::init(argc, argv);

    // 创建一个名为 keyboard_test 的节点。
    auto node = rclcpp::Node::make_shared("keyboard_test");

    // 创建发布者，向 /keyboard/cmd_vel 发布 Twist 消息。
    auto publisher =
        node->create_publisher<geometry_msgs::msg::Twist>(
            "/keyboard/cmd_vel", 10);
    // 保存终端原本的设置，退出时恢复。
    termios old_settings{};

    if (tcgetattr(STDIN_FILENO, &old_settings) != 0) {
        std::cerr << "无法读取终端设置，请在终端中运行此程序。" << std::endl;
        return 1;
    }

    termios new_settings = old_settings;

    // 关闭规范输入：不用按回车，程序就能读到字符。
    new_settings.c_lflag &= static_cast<tcflag_t>(~ICANON);

    // 关闭终端自动回显，避免按键被重复显示。
    new_settings.c_lflag &= static_cast<tcflag_t>(~ECHO);

    if (tcsetattr(STDIN_FILENO, TCSANOW, &new_settings) != 0) {
        std::cerr << "无法设置终端模式。" << std::endl;
        return 1;
    }

    std::cout << "请按 w/a/s/d/q/e；按 x 退出。" << std::endl;

    char key = '\0';

    while (true) {
                // 创建一个集合，记录我们想检查的输入。
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);

        // 最多等待 0.5 秒。
        // 注意：select 可能修改这个变量，所以每次循环都重新创建。
        timeval timeout{};
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;

        const int result = select(
            STDIN_FILENO + 1,
            &read_fds,
            nullptr,
            nullptr,
            &timeout
        );

        if (result < 0) {
            std::cerr << "检查键盘输入失败。" << std::endl;
            break;
        }

        if (result == 0) {
            geometry_msgs::msg::Twist stop_message{};
            publisher->publish(stop_message);
            std::cout << "没有新按键，发布零速度。" << std::endl;
            continue;
        }

        // select 确认有输入后，再读取字符。
        const auto count = read(STDIN_FILENO, &key, 1);

                if (count <= 0) {
                    break;
                }

                if (key == 'x') {
                    std::cout << "退出按键测试。" << std::endl;
                    break;
                }

                // 练习时先使用较低的速度，单位：米/秒。
        const double speed = 0.3;

        // 机体坐标系 FRD：前、右、下为正。
        double vx = 0.0;
        double vy = 0.0;
        double vz = 0.0;

        switch (key) {
            case 'w':
                vx = speed;       // 向前
                break;

            case 's':
                vx = -speed;      // 向后
                break;

            case 'a':
                vy = -speed;      // 向左
                break;

            case 'd':
                vy = speed;       // 向右
                break;

            case 'q':
                vz = -speed;      // 上升
                break;

            case 'e':
                vz = speed;       // 下降
                break;

            default:
                std::cout << "未识别的按键: " << key << std::endl;
                continue;
        }

        geometry_msgs::msg::Twist message;

        message.linear.x = vx;  // 前
        message.linear.y = vy;  // 右
        message.linear.z = vz;  // 下

        publisher->publish(message);

        std::cout << "期望速度 FRD："
                << "前=" << vx << " m/s, "
                << "右=" << vy << " m/s, "
                << "下=" << vz << " m/s"
                << std::endl;
    }

    // 恢复终端设置。
    tcsetattr(STDIN_FILENO, TCSANOW, &old_settings);

    rclcpp::shutdown();

    return 0;
}