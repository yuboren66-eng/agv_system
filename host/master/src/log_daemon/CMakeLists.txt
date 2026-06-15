add_executable(log_daemon main.cpp)
target_link_libraries(log_daemon PRIVATE agv_logger systemd rt)