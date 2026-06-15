#pragma once

/**
 * shm_manager.h — 共享内存生命周期管理
 *
 * 两种角色，对应两种使用方式：
 *
 * ① Owner（model_manager 进程）
 *      ShmOwner owner;
 *      owner.create_and_init();      // 创建 SHM + 写入初始数据
 *      ShmLayout* shm = owner.ptr();
 *      // ... 运行 ...
 *      // 析构时自动 munmap + shm_unlink
 *
 * ② Client（planner / mqtt_subscriber / http_api 等所有其他进程）
 *      ShmClient client;
 *      client.attach();              // attach 已存在的 SHM，并校验 header
 *      ShmLayout* shm = client.ptr();
 *      // ... 运行 ...
 *      // 析构时自动 munmap（不 unlink）
 *
 * 读写数据示例（见文件末尾）
 */

#include "shm_layout.h"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace agv {

    // ─────────────────────────────────────────────────────────────────────────────
    // 内部基类：持有映射指针，处理 munmap
    // ─────────────────────────────────────────────────────────────────────────────

    class ShmBase {
    public:
        ShmBase()  = default;
        ~ShmBase() { unmap(); }

        ShmBase(const ShmBase&)            = delete;
        ShmBase& operator=(const ShmBase&) = delete;

        ShmLayout* ptr()       { return shm_; }
        const ShmLayout* ptr() const { return shm_; }

        bool is_attached() const { return shm_ != nullptr; }

    protected:
        ShmLayout* shm_  {nullptr};
        size_t     size_ {sizeof(ShmLayout)};

        void unmap() {
            if (shm_) {
                ::munmap(shm_, size_);
                shm_ = nullptr;
            }
        }

        static std::string errmsg(const char* ctx) {
            return std::string(ctx) + ": " + ::strerror(errno);
        }
    };

    // ─────────────────────────────────────────────────────────────────────────────
    // ShmOwner — 由 model_manager 进程持有，负责创建和销毁
    // ─────────────────────────────────────────────────────────────────────────────

    class ShmOwner : public ShmBase {
    public:
        /**
         * 创建 SHM 并完成初始化：
         *   1. shm_open(O_CREAT)
         *   2. ftruncate 设定大小
         *   3. mmap
         *   4. 写入 ShmHeader（magic / version / size）
         *   5. 用 placement new 初始化两个 Seqlock（seq_ = 0）
         *   6. 清零 MapData / CarData
         *
         * 若 SHM 已存在（上次异常退出残留），先 unlink 再重建，
         * 保证 header 和数据结构总是干净的。
         *
         * @throws std::runtime_error
         */
        void create_and_init() {
            // 先清理残留
            ::shm_unlink(kShmName);

            int fd = ::shm_open(kShmName, O_CREAT | O_RDWR, 0666);
            if (fd < 0) throw std::runtime_error(errmsg("shm_open(create)"));

            if (::ftruncate(fd, static_cast<off_t>(size_)) < 0) {
                ::close(fd);
                throw std::runtime_error(errmsg("ftruncate"));
            }

            void* p = ::mmap(nullptr, size_,
                            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            ::close(fd);   // mmap 后 fd 可以关闭，映射仍然有效

            if (p == MAP_FAILED)
                throw std::runtime_error(errmsg("mmap(owner)"));

            shm_ = static_cast<ShmLayout*>(p);

            // 用 placement new 初始化含 atomic 的字段（不能用 memset）
            // 数据区用 value-initialization 清零
            // 注意：ProcMutex 不参与 placement new，由 init() 单独初始化
            new (&shm_->map_lock) Seqlock();
            shm_->map_write_mutex.init();      // PTHREAD_PROCESS_SHARED + ROBUST
            new (&shm_->map)      MapData{};

            new (&shm_->car_lock) Seqlock();
            shm_->car_write_mutex.init();      // PTHREAD_PROCESS_SHARED + ROBUST
            new (&shm_->cars)     CarData{};
            new (&shm_->bipaths)   bipathData{};

            // 写 header（最后写，initialized 标志位最后置 1）
            shm_->header.magic       = kShmMagic;
            shm_->header.version     = kShmVersion;
            shm_->header.shm_size    = static_cast<uint32_t>(size_);
            shm_->header.initialized = 1;

            fprintf(stderr,
                "[shm_owner] created %s  size=%zu  "
                "map_offset=%zu  car_offset=%zu  bipath_offset=%zu\n",
                kShmName, size_,
                offsetof(ShmLayout, map),
                offsetof(ShmLayout, cars),
                offsetof(ShmLayout, bipaths));
        }

        ~ShmOwner() {
            if (is_attached()) {
                shm_->map_write_mutex.destroy();
                shm_->car_write_mutex.destroy();
                ::shm_unlink(kShmName);
                fprintf(stderr, "[shm_owner] unlinked %s\n", kShmName);
            }
            // 基类析构自动 munmap
        }
    };

    // ─────────────────────────────────────────────────────────────────────────────
    // ShmClient — 由所有其他进程持有，attach 已存在的 SHM
    // ─────────────────────────────────────────────────────────────────────────────

    class ShmClient : public ShmBase {
    public:
        /**
         * attach 到已存在的 SHM，并校验 header。
         * 若 model_manager 尚未完成初始化，可选重试。
         *
         * @param retry_ms  等待 initialized 标志的最长毫秒数（0 = 不等待）
         * @throws std::runtime_error  SHM 不存在、magic/version 不匹配等
         */
        void attach(int retry_ms = 500) {
            int fd = ::shm_open(kShmName, O_RDWR, 0);
            if (fd < 0) throw std::runtime_error(errmsg("shm_open(attach)"));

            void* p = ::mmap(nullptr, size_,
                            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            ::close(fd);

            if (p == MAP_FAILED)
                throw std::runtime_error(errmsg("mmap(client)"));

            shm_ = static_cast<ShmLayout*>(p);

            validate_header(retry_ms);

            fprintf(stderr, "[shm_client] attached %s\n", kShmName);
        }

    private:
        void validate_header(int retry_ms) {
            // 等待 owner 完成初始化
            int waited = 0;
            while (!shm_->header.initialized && waited < retry_ms) {
                ::usleep(10 * 1000);  // 10ms
                waited += 10;
            }

            if (!shm_->header.initialized)
                throw std::runtime_error("shm: owner not initialized yet");

            if (shm_->header.magic != kShmMagic)
                throw std::runtime_error("shm: magic mismatch");

            if (shm_->header.version != kShmVersion)
                throw std::runtime_error("shm: version mismatch");

            if (shm_->header.shm_size != static_cast<uint32_t>(size_)) {
                throw std::runtime_error(
                    std::string("shm: size mismatch, expected ") +
                    std::to_string(size_) + " got " +
                    std::to_string(shm_->header.shm_size));
            }
        }
    };

    // ─────────────────────────────────────────────────────────────────────────────
    // 读写辅助函数（推荐通过这些函数操作，不要直接访问 shm->map）
    // ─────────────────────────────────────────────────────────────────────────────

    // ── MapData 读：拿快照 ────────────────────────────────────────────────────────

    /**
     * 读取整块 MapData 快照，调用方持有副本，不持有任何锁。
     *
     * 用法：
     *   MapData snap = shm_read_map(shm);
     *   // 用 snap 做路径规划，不再访问 shm->map
     */
    inline MapData shm_read_map(ShmLayout* shm) {
        MapData snap;
        SEQLOCK_READ(shm->map_lock, {
            snap = shm->map;
        });
        return snap;
    }

    /**
     * 只读取单条 Edge（ban/access 场景下的快速查询）。
     * edge_idx 为 edges_ 数组下标，非 edge.id。
     */
    inline Edge shm_read_edge(ShmLayout* shm, uint16_t edge_idx) {
        Edge snap;
        SEQLOCK_READ(shm->map_lock, {
            snap = shm->map.edges_[edge_idx];
        });
        return snap;
    }

    // ── MapData 写：修改边状态（ban/access 接口调用） ─────────────────────────────

    /**
     * 修改单条边的状态，由 path_updater 进程调用。
     *
     * 用法：
     *   shm_set_edge_status(shm, edge_idx, EdgeStatus::BLOCKED);
     */
    inline void shm_set_edge_status(ShmLayout* shm,
                                    uint16_t   edge_idx,
                                    EdgeStatus status,
                                    const char* label = nullptr) {
        if(edge_idx<0)return;
        SeqlockMWWriteGuard g(shm->map_write_mutex, shm->map_lock);
        shm->map.edges_[edge_idx].status = status;
        if (label) {
            strncpy(shm->map.edges_[edge_idx].label, label, sizeof(shm->map.edges_[edge_idx].label) - 1);
            shm->map.edges_[edge_idx].label[sizeof(shm->map.edges_[edge_idx].label) - 1] = '\0';
        }
    }

    // ── CarData 读：拿快照 ────────────────────────────────────────────────────────

    /**
     * 读取整块 CarData 快照。
     *
     * 用法：
     *   CarData snap = shm_read_cars(shm);
     */
    inline CarData shm_read_cars(ShmLayout* shm) {
        CarData snap;
        SEQLOCK_READ(shm->car_lock, {
            snap = shm->cars;
        });
        return snap;
    }

    /**
     * 只读取单辆车状态。
     * car_idx 为 cars_ 数组下标，非 car.id。
     */
    inline Car shm_read_car(ShmLayout* shm, uint8_t car_idx) {
        Car snap;
        SEQLOCK_READ(shm->car_lock, {
            snap = shm->cars.cars_[car_idx];
        });
        return snap;
    }

    // ── CarData 写：更新小车状态（mqtt_subscriber 调用） ─────────────────────────

    /**
     * 更新单辆车的完整状态，由 mqtt_subscriber 进程调用。
     *
     * 用法：
     *   Car updated = ...;
     *   shm_update_car(shm, car_idx, updated);
     */
    inline void shm_update_car(ShmLayout* shm, uint8_t car_idx, const Car& car) {
        SeqlockMWWriteGuard g(shm->car_write_mutex, shm->car_lock);
        shm->cars.cars_[car_idx] = car;
    }

    /**
     * 只更新小车的状态字段（最常用的最小写操作）。
     */
    inline void shm_set_car_status(ShmLayout* shm,
                                    uint8_t    car_idx,
                                    CarStatus  status) {
        SeqlockMWWriteGuard g(shm->car_write_mutex, shm->car_lock);
        shm->cars.cars_[car_idx].status          = status;
    }


    inline bipathData shm_read_bipaths(ShmLayout* shm) {
        bipathData snap;
        snap = shm->bipaths;//不需要读写保护，因为双向边数据由 owner 初始化后只读不改
        return snap;
    }



} // namespace agv
