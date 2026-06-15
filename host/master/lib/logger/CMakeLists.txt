add_library(agv_logger STATIC logger.cpp)
target_include_directories(agv_logger PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(agv_logger PUBLIC rt)