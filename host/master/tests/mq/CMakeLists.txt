add_executable(test_mq1 test_mq1.cpp)

add_executable(test_mq2 test_mq2.cpp)
target_link_libraries(test_mq2 PRIVATE agv_ipc systemd rt)