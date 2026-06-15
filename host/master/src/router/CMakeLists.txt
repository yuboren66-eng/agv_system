add_executable(router router.cpp)
target_link_libraries(router PRIVATE agv_logger agv_ipc systemd rt)
