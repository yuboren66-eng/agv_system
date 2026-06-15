#pragma once

/**
 * seqlock.h — Seqlock（顺序锁）+ ProcMutex（进程共享写者互斥锁）实现
 *
 * 适用场景：读多写少、数据可重读（写者偶尔写，读者不介意重试）。
 * 你的 MapData / CarData 都满足这个特征。
 *
 * 原理：
 *   - 写者：先将 seq 加 1（变为奇数，表示"写中"），
 *           写完数据后再加 1（变为偶数，表示"写完"）。
 *   - 读者：记录读前 seq（必须为偶数才开始读），
 *           读完后再次检查 seq，若与读前相同则读取有效，
 *           否则说明期间有写者介入，重试。
 *
 * 局限：
 *   - 不适合含指针的数据（跨进程指针无效）。
 *   - 写者频繁时读者会持续重试，但 AGV 场景写操作稀少，不是问题。
 *   - 单写者保证由上层 ProcMutex 提供，见 SeqlockMWWriteGuard。
 *
 * SHM 使用注意：
 *   Seqlock 本身是 POD，sizeof 固定，可以直接嵌入 SHM 结构体。
 *   atomic 成员在 SHM 中要求所有进程用相同的内存模型，
 *   Linux x86/ARM 上均满足。
 */

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <pthread.h>

namespace agv {

class Seqlock {
public:
    Seqlock() = default;

    // ── 写者接口 ─────────────────────────────────────────────────────────────

    /// 写前调用：seq 奇数化，内存屏障防止写操作被提前
    void write_begin() {
        uint32_t s = seq_.load(std::memory_order_relaxed);
        seq_.store(s + 1, std::memory_order_release);
    }

    void write_end() {
        uint32_t s = seq_.load(std::memory_order_relaxed);
        seq_.store(s + 1, std::memory_order_release);
    }

    // ── 读者接口 
    uint32_t read_begin() const {
        uint32_t s;
        do {
            s = seq_.load(std::memory_order_acquire);
        } while (s & 1u);
        return s;
    }

    /// 读后调用：返回 true 表示读取期间无写者介入，数据有效
    bool read_end(uint32_t snapshot) const {
        std::atomic_thread_fence(std::memory_order_acquire);
        return seq_.load(std::memory_order_relaxed) == snapshot;
    }

private:
    // 对齐到 cacheline，避免 false sharing（SHM 中多个 Seqlock 相邻时）
    alignas(64) std::atomic<uint32_t> seq_ {0};
    // 填充到 64 字节，防止相邻字段与 seq_ 共享 cacheline
    char _pad[60];
};

static_assert(sizeof(Seqlock) == 64, "Seqlock must be exactly one cacheline");

// ── 进程共享互斥锁（用于写者互斥） ──────────────────────────────────────────

/**
 * ProcMutex — 住在 SHM 中的进程共享 pthread 互斥锁
 *
 * 必须由 ShmOwner 在 create_and_init() 中显式调用 init()，
 * 不能用 placement new 或 memset 初始化（pthread_mutex_t 有内部状态）。
 *
 * 启用了 PTHREAD_MUTEX_ROBUST：持锁进程崩溃后，下一个写者
 * 调用 lock() 会收到 EOWNERDEAD 并自动恢复，避免永久死锁。
 */
struct alignas(64) ProcMutex {
    pthread_mutex_t mtx;
    char _pad[64 - sizeof(pthread_mutex_t)]; // 补齐到一个 cacheline

    /// 在 SHM 初始化阶段由 owner 调用，设置 PTHREAD_PROCESS_SHARED
    void init() {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
        pthread_mutex_init(&mtx, &attr);
        pthread_mutexattr_destroy(&attr);
    }

    void destroy() { pthread_mutex_destroy(&mtx); }

    void lock() {
        int r = pthread_mutex_lock(&mtx);
        if (r == EOWNERDEAD) {
            // 上一个持锁进程异常退出，标记数据已恢复一致
            pthread_mutex_consistent(&mtx);
        }
    }

    void unlock() { pthread_mutex_unlock(&mtx); }
};
static_assert(sizeof(ProcMutex) == 64, "ProcMutex must be exactly one cacheline");

// ── RAII 写者守卫 ─────────────────────────────────────────────────────────────

/**
 * SeqlockWriteGuard — 单写者场景（已废弃，请用 SeqlockMWWriteGuard）
 *
 * @deprecated 直接使用此守卫不提供多写者互斥，仅保留用于兼容。
 *             新代码一律使用 SeqlockMWWriteGuard。
 */
class [[deprecated("Use SeqlockMWWriteGuard for multi-writer safety")]] SeqlockWriteGuard {
public:
    explicit SeqlockWriteGuard(Seqlock& lock) : lock_(lock) {
        lock_.write_begin();
    }
    ~SeqlockWriteGuard() {
        lock_.write_end();
    }
    SeqlockWriteGuard(const SeqlockWriteGuard&)            = delete;
    SeqlockWriteGuard& operator=(const SeqlockWriteGuard&) = delete;

private:
    Seqlock& lock_;
};

/**
 * SeqlockMWWriteGuard — 多写者安全的写守卫（推荐使用）
 *
 * 锁序：ProcMutex.lock() → Seqlock.write_begin() → 写数据
 *        → Seqlock.write_end() → ProcMutex.unlock()
 *
 * 读者从不碰 ProcMutex，读性能不受影响。
 *
 * 用法：
 *   {
 *       SeqlockMWWriteGuard g(shm->map_write_mutex, shm->map_lock);
 *       shm->map.edges_[i].status = EdgeStatus::BLOCKED;
 *   }  // 析构时自动 write_end + unlock
 */
class SeqlockMWWriteGuard {
public:
    SeqlockMWWriteGuard(ProcMutex& mtx, Seqlock& lock)
        : mtx_(mtx), lock_(lock) {
        mtx_.lock();         // ① 排他写者
        lock_.write_begin(); // ② seq 奇数化，通知读者「写中」
    }
    ~SeqlockMWWriteGuard() {
        lock_.write_end();   // ③ seq 偶数化，数据就绪
        mtx_.unlock();       // ④ 释放互斥锁
    }
    SeqlockMWWriteGuard(const SeqlockMWWriteGuard&)            = delete;
    SeqlockMWWriteGuard& operator=(const SeqlockMWWriteGuard&) = delete;

private:
    ProcMutex& mtx_;
    Seqlock&   lock_;
};

// ── 读者重试辅助宏 ────────────────────────────────────────────────────────────

/**
 * 用法：
 *   MapData snap;
 *   SEQLOCK_READ(shm->map_lock, {
 *       snap = shm->map;    // 读取整块数据的快照
 *   });
 *   // 之后使用 snap，不持有任何锁
 */
#define SEQLOCK_READ(lock, body)            \
    do {                                    \
        uint32_t _seq;                      \
        do {                                \
            _seq = (lock).read_begin();     \
            { body }                        \
        } while (!(lock).read_end(_seq));   \
    } while (0)

} // namespace agv