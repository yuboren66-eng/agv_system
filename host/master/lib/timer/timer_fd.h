#pragma once
//注明：本部分的规划与编写均是ai完成，本人只是在其中调试&学习
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <sys/timerfd.h>
#include <unistd.h>

namespace agv {

/**
 * Linux timerfd 封装。
 *
 * 设计原则：
 *   - 构造时不做系统调用，允许作为全局/成员变量。
 *   - init() 创建 timerfd（CLOCK_MONOTONIC，非阻塞 + 关闭执行）。
 *   - 通过 start_one_shot / start_periodic 设置定时器，
 *     到期时 fd 变为可读，可统一纳入 poll() 主循环。
 *   - handle_read() 读取到期次数，必须调用，否则 fd 持续触发。
 *   - 析构自动 close fd。
 */
class TimerFd {
public:
    /**
     * 构造时不做系统调用。
     */
    TimerFd() = default;
    ~TimerFd() { close(); }

    // 禁止拷贝，fd 不可共享
    TimerFd(const TimerFd&)            = delete;
    TimerFd& operator=(const TimerFd&) = delete;

    /**
     * 创建 timerfd。
     * 必须在 poll 循环之前调用。
     *
     * @throws std::runtime_error  系统调用失败时
     */
    void init();

    /**
     * 启动一次性定时器。
     *
     * @param nanoseconds  多少纳秒后到期
     * @throws std::runtime_error
     */
    void start_one_shot(uint64_t nanoseconds);

    /**
     * 启动周期性定时器。
     *
     * @param initial_ns   首次到期时间（纳秒）
     * @param interval_ns  之后每次间隔（纳秒）
     * @throws std::runtime_error
     */
    void start_periodic(uint64_t initial_ns, uint64_t interval_ns);

    /**
     * 停止定时器（但不关闭 fd，可再次启动）。
     */
    void stop();

    /**
     * 返回 timerfd 的文件描述符，加入 poll/epoll 监听。
     * 必须在 init() 之后调用。
     */
    int get_fd() const { return tfd_; }

    /**
     * 当 poll 检测到 timerfd 可读时调用此函数。
     * 内部读取到期次数并返回。
     *
     * @return 本次到期次数（>=1）。若 fd 不可读则返回 0。
     */
    uint64_t handle_read();

    /**
     * 查询定时器是否正在运行。
     */
    bool is_running() const { return running_; }

private:
    void close() {
        if (tfd_ >= 0) {
            ::close(tfd_);
            tfd_ = -1;
        }
        running_ = false;
    }

    static std::string errmsg(const char* ctx) {
        return std::string(ctx) + ": " + ::strerror(errno);
    }

    int      tfd_     = -1;
    bool     running_ = false;
};

} // namespace agv
