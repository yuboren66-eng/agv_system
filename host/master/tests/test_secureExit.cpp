#include "signal_handler.h"
#include "secure_exit.h"

#include <cerrno>
#include <cstring>
#include <cstdio>

#include <poll.h>

#define PROC_NAME "testing"
// ── 可选：SIGUSR1 时的调试状态 dump ─────────────────────────────────────
static void on_sigusr1(const char* proc_name) {
    fprintf(stderr, "[%s] SIGUSR1: dump state here\n", proc_name);
}

// ── 主函数模板 ────────────────────────────────────────────────────────────
int main() {

    // ════════════════════════════════════════════════════
    // Step 1: 最先初始化信号处理（必须在任何线程创建之前）
    // ════════════════════════════════════════════════════
    agv::SignalHandler sig("testing", on_sigusr1);
    try {
        sig.init();
    } catch (const std::exception& e) {
        fprintf(stderr, "[%s] FATAL: %s\n", PROC_NAME, e.what());
        return 1;
    }

    // ════════════════════════════════════════════════════
    // Step 2: 初始化业务资源
    //   - 打开 SHM
    //   - 打开 POSIX MQ
    //   - 创建 timerfd（若需要）
    //   - 连接 MQTT（若需要）
    // ════════════════════════════════════════════════════

    // TODO: int shm_fd = shm_open(...);
    // TODO: mqd_t mq  = mq_open("/mq_task_dispatch", O_RDONLY | O_NONBLOCK, ...);
    // TODO: int tfd   = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

    // ════════════════════════════════════════════════════
    // Step 3: 注册退出清理序列（后注册先执行）
    // ════════════════════════════════════════════════════
    agv::SecureExit exit_seq("testing");

    // TODO: exit_seq.add_cleanup("mq_close", [&]{ mq_close(mq); });
    // TODO: exit_seq.add_cleanup("shm_munmap", [&]{ munmap(shm_ptr, shm_size); shm_close(shm_fd); });
    // TODO: exit_seq.add_cleanup("timerfd_close", [&]{ close(tfd); });
    exit_seq.add_cleanup("hello-clean1",[&]{fprintf(stderr,"doing-cleaning");});

    // ════════════════════════════════════════════════════
    // Step 4: 组建 poll 监听数组
    // ════════════════════════════════════════════════════

    // 示例：监听 signalfd + 1 个 MQ fd
    constexpr int FD_SIG = 0;
    constexpr int FD_MQ  = 1;
    // constexpr int FD_TMR = 2;  // 若有 timerfd

    struct pollfd fds[2];
    fds[FD_SIG].fd     = sig.get_fd();
    fds[FD_SIG].events = POLLIN;
    fds[FD_MQ].fd      = -1;   // TODO: 替换为实际 MQ fd，-1 表示暂时禁用
    fds[FD_MQ].events  = POLLIN;

    fprintf(stderr, "[%s] INFO: started, entering poll loop\n", "testing");

    // ════════════════════════════════════════════════════
    // Step 5: poll 主循环
    // ════════════════════════════════════════════════════
    while (!sig.shutdown_requested()) {
        int nfds = sizeof(fds) / sizeof(fds[0]);
        int ret  = ::poll(fds, nfds, -1);  // 无限等待，全事件驱动

        if (ret < 0) {
            if (errno == EINTR) continue;   // 被打断，重试
            fprintf(stderr, "[%s] ERROR: poll: %s\n", PROC_NAME, strerror(errno));
            break;
        }

        // ── 处理信号事件（优先级最高）────────────────────
        if (fds[FD_SIG].revents & POLLIN) {
            sig.handle_read();
            // handle_read() 内部设置 g_shutdown_；
            // 下次循环 while 条件为 false，自然退出。
            continue;
        }

        // ── 处理 MQ 事件 ─────────────────────────────────
        if (fds[FD_MQ].revents & POLLIN) {
            // TODO: 读取消息，执行业务逻辑
            // struct TaskMsg msg;
            // ssize_t n = mq_receive(mq, (char*)&msg, sizeof(msg), nullptr);
            // if (n > 0) handle_task(msg);
        }

        // ── 处理 timerfd 事件 ────────────────────────────
        // if (fds[FD_TMR].revents & POLLIN) {
        //     uint64_t exp;
        //     read(tfd, &exp, sizeof(exp));  // 必须读走，否则持续触发
        //     on_timer();
        // }
    }

    // ════════════════════════════════════════════════════
    // Step 6: 优雅退出
    //   - 此处可先停止接收新任务（如关闭 MQ 读端）
    //   - exit_seq.run() 内部逆序执行所有清理回调，
    //     最后 sd_notify("STOPPING=1") + exit(0)
    // ════════════════════════════════════════════════════
    fprintf(stderr, "[%s] INFO: shutdown_requested, exiting\n", "testing");

    // TODO: 若有"当前任务"需要等待完成：
    //   wait_for_current_task_done();

    exit_seq.run(200);  // 等待最多 200ms，然后清理退出
    fprintf(stderr, "[%s] INFOB: shutdown_requested, exiting\n", "testing");
}