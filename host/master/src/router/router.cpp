#include <cassert>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <thread>
#include <chrono>
#include "router.h"

const char* proc_name="router";

int main(){
    LOG_INFO(proc_name,"begin");
    agv::SignalHandler sig(proc_name);
    try {
        sig.init();
    } catch (const std::exception& e) {
        LOG_FATAL(proc_name,"%s",e.what());
        return 1;
    }

    agv::MqReceiver<agv::RouteExertMsg> mq;
    try {
        mq.init(agv::kMqRouteExert, agv::kRouteExertMaxMsg, agv::kRouteExertMsgSize);
    } catch (const std::exception& e) {
        LOG_ERROR(proc_name,"fail to create mq:%s",e.what());
        return 1;
    }
    agv::router router_instance;
    if(!router_instance.init()){
        LOG_ERROR(proc_name,"router init failed");
        return 1;
    }

    //注册退出清理序列
    agv::SecureExit exit_seq(proc_name);
    exit_seq.add_cleanup("finish",[&]{LOG_INFO(proc_name,"finish unlinking mq");});
    exit_seq.add_cleanup("unlink-mq",[&]{});
    exit_seq.add_cleanup("unlink-mq",[&]{router_instance.do_cleanup();});

    //组建 poll 监听数组
    constexpr int FD_SIG = 0;
    constexpr int FD_MQ = 1;

    struct pollfd fds[2];
    fds[FD_SIG].fd     = sig.get_fd();
    fds[FD_SIG].events = POLLIN;
    fds[FD_MQ].fd     = mq.get_fd();
    fds[FD_MQ].events = POLLIN;

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
        if (fds[FD_MQ].revents & POLLIN) {
            agv::RouteExertMsg msg_recv={};
            unsigned proi;
            mq.receive(msg_recv,proi);
            LOG_INFO(proc_name,"recieved RouteExertMsg car_id=%u arrived=%u last=%u",
                msg_recv.car_id, msg_recv.arrived_node, msg_recv.last_node);
            router_instance.handle_task(msg_recv);
            continue;
        }     
    }
    LOG_INFO(proc_name,"shutdown-requested");
    exit_seq.run(200);
    LOG_INFO(proc_name,"shutdown-finished");
}