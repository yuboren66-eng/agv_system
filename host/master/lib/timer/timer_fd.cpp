#include "timer_fd.h"

namespace agv {

void TimerFd::init() {
    tfd_ = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (tfd_ < 0) {
        throw std::runtime_error(errmsg("timerfd_create"));
    }
}

void TimerFd::start_one_shot(uint64_t nanoseconds) {
    if (tfd_ < 0) {
        throw std::runtime_error("TimerFd::start_one_shot: not initialized");
    }

    struct itimerspec its{};
    its.it_value.tv_sec  = static_cast<time_t>(nanoseconds / 1'000'000'000);
    its.it_value.tv_nsec = static_cast<long>(nanoseconds % 1'000'000'000);

    if (::timerfd_settime(tfd_, 0, &its, nullptr) < 0) {
        throw std::runtime_error(errmsg("timerfd_settime(one_shot)"));
    }
    running_ = true;
}

void TimerFd::start_periodic(uint64_t initial_ns, uint64_t interval_ns) {
    if (tfd_ < 0) {
        throw std::runtime_error("TimerFd::start_periodic: not initialized");
    }

    struct itimerspec its{};
    its.it_value.tv_sec     = static_cast<time_t>(initial_ns / 1'000'000'000);
    its.it_value.tv_nsec    = static_cast<long>(initial_ns % 1'000'000'000);
    its.it_interval.tv_sec  = static_cast<time_t>(interval_ns / 1'000'000'000);
    its.it_interval.tv_nsec = static_cast<long>(interval_ns % 1'000'000'000);

    if (::timerfd_settime(tfd_, 0, &its, nullptr) < 0) {
        throw std::runtime_error(errmsg("timerfd_settime(periodic)"));
    }
    running_ = true;
}

void TimerFd::stop() {
    if (tfd_ < 0) {
        return;
    }

    struct itimerspec its{}; // 全部置 0 -> 停止定时器
    if (::timerfd_settime(tfd_, 0, &its, nullptr) < 0) {
        // 停止失败不是致命错误，不抛异常
    }
    running_ = false;
}

uint64_t TimerFd::handle_read() {
    if (tfd_ < 0) {
        return 0;
    }

    uint64_t expirations = 0;
    ssize_t n = ::read(tfd_, &expirations, sizeof(expirations));
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        // 其他错误，返回 0 避免主循环崩溃
        return 0;
    }
    return expirations;
}

} // namespace agv