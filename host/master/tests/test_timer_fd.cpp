/**
 * test_timer_fd.cpp -- TimerFd 单元测试
 *
 * 覆盖：
 *   1. 基本生命周期（init / close / 析构）
 *   2. 一次性定时器精度测试
 *   3. 周期性定时器计数测试
 *   4. stop 后不再触发
 *   5. 未初始化时异常
 *
 * 编译：
 *   g++ -std=c++17 -I../lib -o test_timer_fd test_timer_fd.cpp -lrt
 */

#include "timer_fd.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <poll.h>
#include <chrono>

// -- 极简测试框架 ----------------------------------------------------------------
static int g_run = 0, g_fail = 0;
#define CHECK(expr) \
    do { ++g_run; \
         if (!(expr)) { fprintf(stderr, "  FAIL %s:%d  %s\n", \
                                __FILE__, __LINE__, #expr); ++g_fail; } \
         else { fprintf(stderr, "  PASS  %s\n", #expr); } \
    } while(0)

// -- 辅助：获取当前时间戳（毫秒）-------------------------------------------------
static int64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// -- 1. 基本生命周期 -------------------------------------------------------------
void test_lifecycle() {
    fprintf(stderr, "\n[test_lifecycle]\n");
    {
        agv::TimerFd timer;
        CHECK(timer.get_fd() == -1);
        CHECK(!timer.is_running());
        timer.init();
        CHECK(timer.get_fd() >= 0);
    }
    // 析构后无泄漏（valgrind / asan 可验证）
    CHECK(true); // 走到这里即通过
}

// -- 2. 一次性定时器 -------------------------------------------------------------
void test_one_shot() {
    fprintf(stderr, "\n[test_one_shot]\n");

    agv::TimerFd timer;
    timer.init();
    timer.start_one_shot(100'000'000); // 100ms

    struct pollfd pfd{};
    pfd.fd     = timer.get_fd();
    pfd.events = POLLIN;

    int64_t t0 = now_ms();
    int ret = ::poll(&pfd, 1, 1000); // 最多等 1s
    CHECK(ret == 1);
    CHECK(pfd.revents & POLLIN);

    uint64_t exp = timer.handle_read();
    CHECK(exp == 1);
    CHECK(!timer.is_running()); // 一次性定时器到期后自动停止

    int64_t elapsed = now_ms() - t0;
    CHECK(elapsed >= 80 && elapsed <= 200); // 允许 20ms 容差
}

// -- 3. 周期性定时器 -------------------------------------------------------------
void test_periodic() {
    fprintf(stderr, "\n[test_periodic]\n");

    agv::TimerFd timer;
    timer.init();
    timer.start_periodic(0, 50'000'000); // 立即开始，每 50ms

    struct pollfd pfd{};
    pfd.fd     = timer.get_fd();
    pfd.events = POLLIN;

    uint64_t total_exp = 0;
    int64_t t0 = now_ms();

    for (int i = 0; i < 5; ++i) {
        int ret = ::poll(&pfd, 1, 1000);
        CHECK(ret == 1);
        uint64_t exp = timer.handle_read();
        CHECK(exp >= 1);
        total_exp += exp;
    }

    int64_t elapsed = now_ms() - t0;
    CHECK(total_exp >= 5);
    CHECK(elapsed >= 200 && elapsed <= 400); // 5 * 50ms = 250ms，允许容差

    timer.stop();
    CHECK(!timer.is_running());
}

// -- 4. stop 后不再触发 ----------------------------------------------------------
void test_stop() {
    fprintf(stderr, "\n[test_stop]\n");

    agv::TimerFd timer;
    timer.init();
    timer.start_periodic(0, 50'000'000);

    struct pollfd pfd{};
    pfd.fd     = timer.get_fd();
    pfd.events = POLLIN;

    // 先等一次触发
    int ret = ::poll(&pfd, 1, 1000);
    CHECK(ret == 1);
    timer.handle_read();

    // 停止
    timer.stop();
    CHECK(!timer.is_running());

    // 再 poll 150ms，不应触发
    ret = ::poll(&pfd, 1, 150);
    CHECK(ret == 0); // 超时返回 0
}

// -- 5. 未初始化时异常 -----------------------------------------------------------
void test_not_initialized() {
    fprintf(stderr, "\n[test_not_initialized]\n");

    agv::TimerFd timer;
    bool threw = false;
    try {
        timer.start_one_shot(100'000'000);
    } catch (const std::runtime_error& e) {
        threw = true;
    }
    CHECK(threw);
}

// -- main -----------------------------------------------------------------------
int main() {
    fprintf(stderr, "=== TimerFd unit tests ===\n");

    test_lifecycle();
    test_one_shot();
    test_periodic();
    test_stop();
    test_not_initialized();

    fprintf(stderr, "\n=== results: %d/%d passed ===\n",
            g_run - g_fail, g_run);
    return g_fail == 0 ? 0 : 1;
}