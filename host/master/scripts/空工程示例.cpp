#include <cassert>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <thread>
#include <chrono>

//请确保log_daemon正常运行
#include "shm_manager.h"
#include "mq_wrapper.h"
#include "logger.h"
#include "signal_handler.h"
#include "secure_exit.h"


const char* proc_name="template-program";

int main(){
    LOG_INFO(proc_name,"begin");
    agv::SignalHandler sig(proc_name);
    try {
        sig.init();
    } catch (const std::exception& e) {
        LOG_FATAL(proc_name,"%s",e.what());
        return 1;
    }

    try {
        
    } catch (const std::exception& e) {
        LOG_ERROR(proc_name,"fail to create mq:%s",e.what());
        return 1;
    }
    agv::ShmOwner owner_shm;


    //注册退出清理序列
    agv::SecureExit exit_seq(proc_name);
    exit_seq.add_cleanup("finish",[&]{LOG_INFO(proc_name,"finish unlinking mq");});
    exit_seq.add_cleanup("unlink-mq",[&]{});

    //组建 poll 监听数组
    constexpr int FD_SIG = 0;

    struct pollfd fds[2];
    fds[FD_SIG].fd     = sig.get_fd();
    fds[FD_SIG].events = POLLIN;

    LOG_INFO(proc_name,"enter-poll-loop");

    //poll 主循环
    while (!sig.shutdown_requested()) {
        int nfds = sizeof(fds) / sizeof(fds[0]);
        int ret  = ::poll(fds, nfds, -1);  // 无限等待，全事件驱动

        if (ret < 0) {
            if (errno == EINTR) continue;   // 被打断，重试
            LOG_ERROR(proc_name,"poll:%s",strerror(errno));
            break;
        }

        if (fds[FD_SIG].revents & POLLIN) {
            sig.handle_read();
            continue;
        }

    }
    LOG_INFO(proc_name,"shutdown-requested");
    exit_seq.run(200);
    LOG_INFO(proc_name,"shutdown-finished");
}