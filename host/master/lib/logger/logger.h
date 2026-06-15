#pragma once
#include <mqueue.h>
#include <cstdarg>
#include "agv/log_msg.h"

// Initialize/open the message queue. Returns (mqd_t)-1 on error.
mqd_t agv_log_init();

// Format text and send log message. Text will be truncated to fit LogMsg::text.
void agv_logf(mqd_t mq, LogLevel level, const char* source, const char* fmt, ...);

// Convenience macros for logging at different levels.
#define LOG_DEBUG(src, fmt, ...) agv_logf(agv_log_init(), LogLevel::DEBUG, src, fmt, ##__VA_ARGS__)
#define LOG_INFO(src, fmt, ...)  agv_logf(agv_log_init(), LogLevel::INFO, src, fmt, ##__VA_ARGS__)
#define LOG_WARN(src, fmt, ...)  agv_logf(agv_log_init(), LogLevel::WARN, src, fmt, ##__VA_ARGS__)
#define LOG_ERROR(src, fmt, ...) agv_logf(agv_log_init(), LogLevel::ERROR, src, fmt, ##__VA_ARGS__)
#define LOG_FATAL(src, fmt, ...) agv_logf(agv_log_init(), LogLevel::FATAL, src, fmt, ##__VA_ARGS__)