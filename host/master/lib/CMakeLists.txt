add_library(agv_logger STATIC logger/logger.cpp)
target_include_directories(agv_logger PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/logger
)
target_link_libraries(agv_logger PUBLIC rt)

add_library(agv_ipc INTERFACE)
target_include_directories(agv_ipc INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/ipc
)
# ipc 有 .cpp 文件，编译成静态库
#add_library(agv_ipc STATIC
#    ipc/mq_wrapper.cpp
#    ipc/signalfd_helper.cpp
#    ipc/timerfd_helper.cpp
#)
#target_link_libraries(agv_ipc PUBLIC rt)  # rt 提供 mq_open

add_library(agv_timer STATIC timer/timer_fd.cpp)
target_include_directories(agv_timer PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/timer
)
target_link_libraries(agv_timer PUBLIC rt)