add_subdirectory(mq)

add_executable(test_logger test_logger.cpp)
target_link_libraries(test_logger PRIVATE agv_logger systemd rt)

add_executable(inject_log inject_log.cpp)
target_link_libraries(inject_log PRIVATE agv_logger systemd rt)

add_executable(test_secureExit test_secureExit.cpp)
target_link_libraries(test_secureExit PRIVATE agv_ipc systemd rt)

add_executable(test_shm test_shm.cpp)
target_link_libraries(test_shm PRIVATE agv_ipc -lpthread systemd rt)

add_executable(test_shm_read_print test_shm_read_print.cpp)
target_link_libraries(test_shm_read_print PRIVATE agv_ipc -lpthread systemd rt)

add_executable(test_tmp test_tmp.cpp)
target_link_libraries(test_tmp PRIVATE agv_logger agv_ipc -lpthread systemd rt)

add_executable(test_mq_mqtt test_mq_mqtt.cpp)
target_link_libraries(test_mq_mqtt PRIVATE agv_logger agv_ipc -lpthread systemd rt)

add_executable(test_mq_inject test_mq_inject.cpp)
target_link_libraries(test_mq_inject PRIVATE agv_logger agv_ipc -lpthread systemd rt)

add_executable(test_timer_fd test_timer_fd.cpp)
target_link_libraries(test_timer_fd PRIVATE agv_timer rt)

#add_executable(test_1_http test_1_http.cpp)
#target_link_libraries(test_1_http PRIVATE agv_timer rt -lfcgi)
