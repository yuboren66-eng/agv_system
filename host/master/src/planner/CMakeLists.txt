add_executable(planner planner.cpp)
target_link_libraries(planner PRIVATE agv_logger agv_ipc systemd rt)
