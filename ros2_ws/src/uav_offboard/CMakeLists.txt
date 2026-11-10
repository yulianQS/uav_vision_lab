cmake_minimum_required(VERSION 3.8)
project(uav_offboard)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(px4_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(tf2_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(uav_interfaces REQUIRED)


if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

add_executable(offboard_takeoff src/takeoff.cpp)
ament_target_dependencies(offboard_takeoff rclcpp px4_msgs)
install(TARGETS offboard_takeoff DESTINATION lib/${PROJECT_NAME})

add_executable(move_drone src/move.cpp)
install(TARGETS
  move_drone
  DESTINATION lib/${PROJECT_NAME}
)
ament_target_dependencies(move_drone
  rclcpp
  px4_msgs
)

add_executable(keyboard_test src/keyboard_test.cpp)
install(TARGETS
  keyboard_test
  DESTINATION lib/${PROJECT_NAME}
)
ament_target_dependencies(keyboard_test
  rclcpp
  geometry_msgs
)

add_executable(velocity_listener src/velocity_listener.cpp src/velocity_transform.cpp)
target_include_directories(velocity_listener PRIVATE include)
install(TARGETS
  velocity_listener
  DESTINATION lib/${PROJECT_NAME}
)
ament_target_dependencies(velocity_listener
  rclcpp
  geometry_msgs
  px4_msgs
  std_msgs
  uav_interfaces
)

add_executable(tag_reader src/tag_reader.cpp)
ament_target_dependencies(tag_reader
  rclcpp
  tf2_msgs
)
install(TARGETS
  tag_reader
  DESTINATION lib/${PROJECT_NAME}
)

add_executable(velocity_monitor src/velocity_monitor.cpp)
ament_target_dependencies(velocity_monitor 
rclcpp 
uav_interfaces)
install(TARGETS velocity_monitor DESTINATION lib/${PROJECT_NAME})

ament_package()
