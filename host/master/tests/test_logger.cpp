#include "agv/log_msg.h"
#include "logger.h"
mqd_t mq = agv_log_init();
int main(){
    LOG_DEBUG("MainModule", "This is a debug message with value: %d", 100);
    mq_close(mq);
    return 0;
}