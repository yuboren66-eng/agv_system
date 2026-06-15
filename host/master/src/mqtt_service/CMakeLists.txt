cmake_minimum_required(VERSION 3.10)
project(mqtt_service)

add_executable(mqtt_publisher mqtt_publisher.cpp)
target_include_directories(mqtt_publisher PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/lib
    ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(mqtt_publisher PRIVATE agv_logger agv_ipc mosquitto rt pthread)

add_executable(mqtt_subscriber mqtt_subscriber.cpp)
target_include_directories(mqtt_subscriber PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/lib
    ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(mqtt_subscriber PRIVATE agv_logger agv_ipc mosquitto rt pthread)
