#include <unistd.h>
#include <mqueue.h>
#include <systemd/sd-journal.h>
#include <poll.h>
#include <sys/signalfd.h>
#include <signal.h>
#include <cstdio>
#include <cstring>
#include <atomic>
#include "agv/log_msg.h"

static std::atomic<bool> g_shutdown{false};

int main() {
    // 1. 阻塞信号，改用 signalfd 同步处理
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, nullptr);
    int sfd = signalfd(-1, &mask, SFD_NONBLOCK);

    // 2. 打开 mq_log，以阻塞读模式（不加 O_NONBLOCK）
    struct mq_attr attr{};
    attr.mq_maxmsg  = MQ_LOG_MAXMSG;
    attr.mq_msgsize = MQ_LOG_MSGSIZE;
    mqd_t mq = mq_open(MQ_LOG_NAME, O_RDONLY | O_CREAT, 0644, &attr);
    if (mq == (mqd_t)-1) {
        perror("mq_open failed");
        return 1;
    }

    // 3. poll 主循环：同时等待 signalfd 和 MQ
    struct pollfd fds[2];
    fds[0].fd     = sfd;
    fds[0].events = POLLIN;
    fds[1].fd     = mq;      // mqd_t 在 Linux 上本质是 fd，可直接 poll
    fds[1].events = POLLIN;

    char buf[MQ_LOG_MSGSIZE];

    while (!g_shutdown) {
        int ret = poll(fds, 2, -1);  // 无超时，完全阻塞等待
        if (ret < 0) break;

        if (fds[0].revents & POLLIN) {
            struct signalfd_siginfo si;
            read(sfd, &si, sizeof(si));
            g_shutdown = true;
            continue;
        }

        if (fds[1].revents & POLLIN) {
            unsigned int prio;
            ssize_t n = mq_receive(mq, buf, sizeof(buf), &prio);
            if (n <= 0) continue;

            auto* msg = reinterpret_cast<LogMsg*>(buf);

            // 将 LogLevel 映射到 syslog 优先级
            int syslog_prio = static_cast<int>(msg->level);

            // 写入 journald，附带结构化字段，可用 journalctl 过滤
            sd_journal_send(
                "MESSAGE=%s",         msg->text,
                "AGV_SOURCE=%s",      msg->source,
                "PRIORITY=%d",        syslog_prio,
                "SYSLOG_IDENTIFIER=agv_log",
                nullptr
            );
        }
    }

    mq_close(mq);
    close(sfd);
    // 注意：log_daemon 不删除 MQ，由 model_manager（最后退出）负责 mq_unlink
    return 0;
}