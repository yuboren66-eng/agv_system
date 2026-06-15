#pragma once
//注明：本部分的规划与编写均是ai完成，本人只是在其中调试&学习
#include <atomic>
#include <csignal>
#include <cstring>
#include <stdexcept>
#include <string>
#include <poll.h>
#include <sys/signalfd.h>
#include <unistd.h>

namespace agv {

/**
 * 信号处理回调类型。
 * SIGUSR1 触发时调用，用于 dump 调试状态，参数为进程名。
 */
using Usr1Callback = void (*)(const char* proc_name);

class SignalHandler {
public:
    /**
     * 构造时不做任何系统调用，允许作为全局/成员变量。
     *
     * @param proc_name  进程名，用于日志标注（如 "planner"）
     * @param usr1_cb    SIGUSR1 回调，传 nullptr 则忽略
     */
    explicit SignalHandler(const char* proc_name = "agv_proc",
                           Usr1Callback usr1_cb  = nullptr)
        : proc_name_(proc_name), usr1_cb_(usr1_cb) {}

    ~SignalHandler() {
        if (sfd_ >= 0) {
            ::close(sfd_);
        }
    }

    // 禁止拷贝，fd 不可共享
    SignalHandler(const SignalHandler&)            = delete;
    SignalHandler& operator=(const SignalHandler&) = delete;

    /**
     * 初始化：阻塞信号 + 创建 signalfd。
     * 必须在 poll 循环之前调用，建议在 main() 最开始调用。
     *
     * @throws std::runtime_error  系统调用失败时
     */
    void init() {
        // 构造要屏蔽的信号集
        sigemptyset(&mask_);
        sigaddset(&mask_, SIGTERM);
        sigaddset(&mask_, SIGINT);
        sigaddset(&mask_, SIGUSR1);

        // 阻塞上述信号：不让内核异步递送给进程
        if (::sigprocmask(SIG_BLOCK, &mask_, nullptr) < 0) {
            throw std::runtime_error(
                std::string("sigprocmask failed: ") + ::strerror(errno));
        }

        // 创建 signalfd：信号到达时 fd 变为可读
        sfd_ = ::signalfd(-1, &mask_, SFD_NONBLOCK | SFD_CLOEXEC);
        if (sfd_ < 0) {
            throw std::runtime_error(
                std::string("signalfd failed: ") + ::strerror(errno));
        }
    }

    /**
     * 返回 signalfd 的文件描述符，加入 poll/epoll 监听。
     * 必须在 init() 之后调用。
     */
    int get_fd() const { return sfd_; }

    /**
     * 当 poll 检测到 signalfd 可读时调用此函数。
     * 内部读取所有待处理信号并分发：
     *   - SIGTERM / SIGINT  → 设置 g_shutdown_ = true
     *   - SIGUSR1           → 调用 usr1_cb_（如已注册）
     *
     * 调用后通过 shutdown_requested() 检查是否该退出。
     */
    void handle_read() {
        struct signalfd_siginfo si;

        // 循环读取，直到 fd 中没有信号（SFD_NONBLOCK 保证不阻塞）
        while (true) {
            ssize_t n = ::read(sfd_, &si, sizeof(si));
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break; // 已读完
                break; // 其他错误，跳出
            }
            if (n != static_cast<ssize_t>(sizeof(si))) break;

            dispatch(si.ssi_signo);
        }
    }

    /**
     * 查询是否已收到 SIGTERM/SIGINT，应在主循环条件中检查。
     * 使用 memory_order_acquire 保证可见性。
     */
    bool shutdown_requested() const {
        return g_shutdown_.load(std::memory_order_acquire);
    }

    /**
     * 返回进程名（用于日志）。
     */
    const char* proc_name() const { return proc_name_; }

private:
    void dispatch(uint32_t signo) {
        switch (signo) {
        case SIGTERM:
        case SIGINT:
            g_shutdown_.store(true, std::memory_order_release);
            break;
        case SIGUSR1:
            if (usr1_cb_) {
                usr1_cb_(proc_name_);
            }
            break;
        default:
            break;
        }
    }

    const char*    proc_name_;
    Usr1Callback   usr1_cb_;
    int            sfd_      {-1};
    sigset_t       mask_     {};
    std::atomic<bool> g_shutdown_ {false};
};

} // namespace agv




/**
 * signal_handler.h — 统一信号处理框架
 *
 * 设计原则：
 *   - 进程启动时用 sigprocmask 阻塞 SIGTERM/SIGINT/SIGUSR1，
 *     不让内核异步递送，彻底消除信号处理函数的异步安全问题。
 *   - 用 signalfd 把信号变成一个普通可读 fd，
 *     统一纳入进程的 poll() 主循环，与 MQ fd、timerfd 平等处理。
 *   - 对外只暴露三个接口：init / get_fd / handle_read。
 *
 * 用法（每个进程 main.cpp 开头）：
 *
 *   #include "signal_handler.h"
 *
 *   int main() {
 *       agv::SignalHandler sig;
 *       sig.init();           // 阻塞信号 + 创建 signalfd
 *
 *       struct pollfd fds[2];
 *       fds[0].fd     = sig.get_fd();
 *       fds[0].events = POLLIN;
 *       // fds[1] = mq_fd / timerfd / ...
 *
 *       while (!sig.shutdown_requested()) {
 *           int ret = poll(fds, 2, -1);
 *           if (ret < 0) { if (errno == EINTR) continue; break; }
 *
 *           if (fds[0].revents & POLLIN) {
 *               sig.handle_read();   // 内部设置 g_shutdown，并处理 SIGUSR1
 *           }
 *           // 处理其他 fd ...
 *       }
 *
 *       // 退出序列 ...
 *       return 0;
 *   }
 */