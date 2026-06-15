#pragma once
#include <cstdint>
constexpr const char* MQ_LOG_NAME="/mq_log";
constexpr int MQ_LOG_MAXMSG=64;
constexpr int MQ_LOG_MSGSIZE=256;

enum class LogLevel:uint8_t{
    DEBUG=7,
    INFO=6,
    WARN=4,
    ERROR=3,
    FATAL=2,
};

inline const char* level_str(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default:              return "UNKNOWN";
    }
}

struct LogMsg{
    LogLevel level;
    char source[32];
    char text[220];
};
static_assert(sizeof(LogMsg)<=MQ_LOG_MSGSIZE,"LogMsg size exceeds MQ_LOG_MSGSIZE");

