// scripts/inject_log.cpp
// 编译：g++ inject_log.cpp -o inject_log -lrt
#include <mqueue.h>
#include <cstring>
#include <cstdio>
#include "agv/log_msg.h"
#include "logger.h"

int main(int argc, char* argv[]) {
    mqd_t mq = mq_open(MQ_LOG_NAME, O_WRONLY | O_NONBLOCK);
    if (mq == (mqd_t)-1) { perror("mq_open"); return 1; }

    LogMsg msg{};
    msg.level = LogLevel::ERROR;
    strncpy(msg.source, "inject_test", sizeof(msg.source)-1);
    strncpy(msg.text,   argc > 1 ? argv[1] : "hello from inject", sizeof(msg.text)-1);

    mq_send(mq, reinterpret_cast<char*>(&msg), sizeof(msg), 6);
    mq_close(mq);
    LOG_DEBUG("inject_log", "Injected log message");
    return 0;
}