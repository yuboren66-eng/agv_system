#include <mqueue.h>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <unistd.h>
#include "config/config.h"
#include "logger.h"

mqd_t agv_log_init(){
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = MQ_LOG_MAXMSG;
    attr.mq_msgsize = MQ_LOG_MSGSIZE;
    attr.mq_curmsgs = 0;

    mqd_t mq = mq_open(MQ_LOG_NAME, O_CREAT | O_WRONLY, 0644, &attr);
    if (mq == (mqd_t)-1) {
        write(2, "[logger] Failed to open message queue\n", 39);
        return (mqd_t)-1;
    }
    return mq;
}

static void _agv_log(mqd_t mq, LogLevel level, const char* source, const char* text){
    if (mq == (mqd_t)-1) {
        write(2, "[logger] Invalid message queue descriptor\n", 43);
        return;
    }
    LogMsg msg;
    msg.level = level;
    strncpy(msg.source, source, sizeof(msg.source) - 1);
    msg.source[sizeof(msg.source) - 1] = '\0';
    strncpy(msg.text, text, sizeof(msg.text) - 1);
    msg.text[sizeof(msg.text) - 1] = '\0';
    #ifdef _AGV_PRINT_DEBUG
        dprintf(1, "[%s](%s) %s\n", level_str(msg.level),msg.source, msg.text);
    #endif

    if (mq_send(mq, (const char*)&msg, sizeof(msg), 0) == -1) {
        write(2, "[logger] Failed to send log message\n", 37);
    }
}

void agv_logf(mqd_t mq, LogLevel level, const char* source, const char* fmt, ...){
    va_list args;
    va_start(args, fmt);
    char buf[MQ_LOG_MSGSIZE];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    _agv_log(mq, level, source, buf);
}
