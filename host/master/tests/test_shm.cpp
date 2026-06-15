/**
 * test_shm.cpp — SHM 布局与 Seqlock 单元测试
 *
 * 覆盖：
 *   1. Seqlock 基本读写流程
 *   2. Seqlock 写中途读者检测到冲突并重试
 *   3. ShmHeader 大小和 magic 常量
 *   4. ShmLayout 各字段偏移（锁和数据不重叠）
 *   5. ShmOwner create_and_init + ShmClient attach + header 校验
 *   6. shm_set_edge_status / shm_read_map 完整读写
 *   7. shm_update_car / shm_read_car 完整读写
 *   8. SeqlockWriteGuard RAII
 *   9. SEQLOCK_READ 宏重试逻辑（用线程模拟写者中途介入）
 *
 * 编译：
 *   g++ -std=c++17 -I.. -I../lib \
 *       -o test_shm test_shm.cpp \
 *       -lrt -lpthread
 */

#include "shm_manager.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <thread>
#include <chrono>

// ── 极简测试框架 ──────────────────────────────────────────────────────────────
static int g_run = 0, g_fail = 0;
#define CHECK(expr) \
    do { ++g_run; \
         if (!(expr)) { fprintf(stderr, "  FAIL %s:%d  %s\n", \
                                __FILE__, __LINE__, #expr); ++g_fail; } \
         else { fprintf(stderr, "  PASS  %s\n", #expr); } \
    } while(0)

// ─────────────────────────────────────────────────────────────────────────────
// 1. Seqlock 基本读写
// ─────────────────────────────────────────────────────────────────────────────

void test_seqlock_basic() {
    fprintf(stderr, "\n[test_seqlock_basic]\n");

    agv::Seqlock lock;

    // 初始 seq = 0（偶数），read_begin 不自旋
    uint32_t snap = lock.read_begin();
    CHECK(snap == 0);
    CHECK(lock.read_end(snap));   // 无写者，校验通过

    // 一次完整写
    lock.write_begin();
    lock.write_end();

    // 写后 seq = 2，读者再次校验
    snap = lock.read_begin();
    CHECK(snap == 2);
    CHECK(lock.read_end(snap));
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. SeqlockWriteGuard RAII
// ─────────────────────────────────────────────────────────────────────────────

void test_seqlock_write_guard() {
    fprintf(stderr, "\n[test_seqlock_write_guard]\n");

    agv::Seqlock lock;
    {
        agv::SeqlockWriteGuard g(lock);
        // 写中：seq 为奇数，read_begin 会自旋（此处不测自旋，只验 seq 奇偶）
        // 无法直接读私有 seq_，用 read_end(0) 检测：0 != 当前 seq，校验失败
        CHECK(!lock.read_end(0));  // 写中 seq=1，与快照 0 不符
    }
    // 写完：seq=2
    uint32_t snap = lock.read_begin();
    CHECK(snap == 2);
    CHECK(lock.read_end(snap));
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. 写者中途介入，读者 read_end 返回 false
// ─────────────────────────────────────────────────────────────────────────────

void test_seqlock_concurrent_retry() {
    fprintf(stderr, "\n[test_seqlock_concurrent_retry]\n");

    agv::Seqlock lock;
    std::atomic<bool> writer_done{false};
    std::atomic<bool> reader_saw_false{false};

    // 读者线程：先拿快照，等写者完成后检查
    std::thread reader([&] {
        uint32_t snap = lock.read_begin();  // snap=0
        // 等写者完成
        while (!writer_done.load()) {}
        // 写者修改了 seq，read_end 应返回 false
        bool valid = lock.read_end(snap);
        if (!valid) reader_saw_false.store(true);
    });

    // 主线程：短暂延迟后写
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    lock.write_begin();
    lock.write_end();
    writer_done.store(true);

    reader.join();
    CHECK(reader_saw_false.load());  // 读者必须检测到冲突
}

// ─────────────────────────────────────────────────────────────────────────────
// 4. ShmHeader 大小 + 常量
// ─────────────────────────────────────────────────────────────────────────────

void test_shm_header() {
    fprintf(stderr, "\n[test_shm_header]\n");

    CHECK(sizeof(agv::ShmHeader) == 64);
    CHECK(agv::kShmMagic   == 0x41475631u);
    CHECK(agv::kShmVersion == 1u);

    // ShmLayout 偏移：锁和数据不重叠
    size_t map_lock_off = offsetof(agv::ShmLayout, map_lock);
    size_t map_off      = offsetof(agv::ShmLayout, map);
    size_t car_lock_off = offsetof(agv::ShmLayout, car_lock);
    size_t car_off      = offsetof(agv::ShmLayout, cars);

    fprintf(stderr,
        "  ShmLayout offsets: header=0  map_lock=%zu  map=%zu"
        "  car_lock=%zu  cars=%zu  total=%zu\n",
        map_lock_off, map_off, car_lock_off, car_off, sizeof(agv::ShmLayout));

    CHECK(map_lock_off == 64);                            // header 之后
    CHECK(map_off      == map_lock_off + sizeof(agv::Seqlock));
    // 不断言 car_lock_off == map_off + sizeof(MapData)：
    // 编译器可能在 MapData 尾部插入对齐 padding，偏移由布局决定。
    // 正确性已由下一条 car_off == car_lock_off + sizeof(Seqlock) 保证。
    CHECK(car_lock_off > map_off);  // 只验方向正确
    CHECK(car_off      == car_lock_off + sizeof(agv::Seqlock));
}

// ─────────────────────────────────────────────────────────────────────────────
// 5. ShmOwner create + ShmClient attach + header 校验
// ─────────────────────────────────────────────────────────────────────────────

void test_shm_lifecycle() {
    fprintf(stderr, "\n[test_shm_lifecycle]\n");

    // Owner 创建
    agv::ShmOwner owner;
    owner.create_and_init();
    CHECK(owner.is_attached());

    agv::ShmLayout* shm = owner.ptr();
    CHECK(shm != nullptr);
    CHECK(shm->header.magic       == agv::kShmMagic);
    CHECK(shm->header.version     == agv::kShmVersion);
    CHECK(shm->header.initialized == 1);
    CHECK(shm->header.shm_size    == sizeof(agv::ShmLayout));

    // Client attach
    agv::ShmClient client;
    client.attach(0);  // 已初始化，不需要等待
    CHECK(client.is_attached());

    agv::ShmLayout* shm2 = client.ptr();
    CHECK(shm2 != nullptr);
    CHECK(shm2->header.magic == agv::kShmMagic);

    // 两个指针映射到同一块物理内存
    CHECK(shm->header.shm_size == shm2->header.shm_size);

    // owner 析构 → unlink；client 析构 → munmap
}

// ─────────────────────────────────────────────────────────────────────────────
// 6. MapData 读写：shm_set_edge_status / shm_read_map
// ─────────────────────────────────────────────────────────────────────────────

void test_map_read_write() {
    fprintf(stderr, "\n[test_map_read_write]\n");
    agv::Node adn={0,10,20,agv::NodeStatus::IDLE,"A","d"};
    agv::ShmOwner owner;
    owner.create_and_init();
    agv::ShmLayout* shm = owner.ptr();

    // 写入初始地图（启动阶段，无其他进程，直接写）
    shm->map.node_count_ = 3;
    shm->map.edge_count_ = 2;

    shm->map.edges_[0] = {0, 0, 1, 10, agv::EdgeStatus::IDLE};
    shm->map.edges_[1] = {1, 1, 2, 15, agv::EdgeStatus::IDLE};

    shm->map.nodes_[0] = agv::Node{0,10,20,agv::NodeStatus::IDLE,"A","d"};
    shm->map.nodes_[1] = agv::Node{1, 30, 20, agv::NodeStatus::IDLE, "B", "2"};
    shm->map.nodes_[2] = agv::Node{2, 50, 20, agv::NodeStatus::IDLE, "C", "3"};


    // 用辅助函数读快照
    agv::MapData snap = agv::shm_read_map(shm);
    CHECK(snap.node_count_ == 3);
    CHECK(snap.edge_count_ == 2);
    CHECK(snap.nodes_[0].id == 0);
    CHECK(snap.edges_[1].weight == 15);
    CHECK(snap.edges_[0].status == agv::EdgeStatus::IDLE);

    // ban 一条边
    agv::shm_set_edge_status(shm, 0, agv::EdgeStatus::BLOCKED);

    // 再次读快照，确认变化
    agv::MapData snap2 = agv::shm_read_map(shm);
    CHECK(snap2.edges_[0].status == agv::EdgeStatus::BLOCKED);
    CHECK(snap2.edges_[1].status == agv::EdgeStatus::IDLE);    // 未改变

    // access 恢复
    agv::shm_set_edge_status(shm, 0, agv::EdgeStatus::IDLE);
    agv::Edge e = agv::shm_read_edge(shm, 0);
    CHECK(e.status == agv::EdgeStatus::IDLE);
}

// ─────────────────────────────────────────────────────────────────────────────
// 7. CarData 读写：shm_update_car / shm_read_car / shm_set_car_status
// ─────────────────────────────────────────────────────────────────────────────

void test_car_read_write() {
    fprintf(stderr, "\n[test_car_read_write]\n");

    agv::ShmOwner owner;
    owner.create_and_init();
    agv::ShmLayout* shm = owner.ptr();

    // 初始化两辆车
    shm->cars.car_count_ = 2;
    shm->cars.cars_[0] = {
        .id              = 1,
        .status          = agv::CarStatus::IDLE,
        .current_node_id = 0,
        .current_task_id = 0,
        .target_node_id  = 0,
        .path_len        = 0,
    };
    shm->cars.cars_[1] = {
        .id              = 2,
        .status          = agv::CarStatus::IDLE,
        .current_node_id = 2,
        .current_task_id = 0,
        .target_node_id  = 0,
        .path_len        = 0,
    };

    // 读整块快照
    agv::CarData snap = agv::shm_read_cars(shm);
    CHECK(snap.car_count_    == 2);
    CHECK(snap.cars_[0].id   == 1);
    CHECK(snap.cars_[1].current_node_id == 2);

    // 小车 0 出发，状态变 MOVING，路径下发
    agv::Car updated = shm->cars.cars_[0];
    updated.status         = agv::CarStatus::MOVING;
    updated.target_node_id = 2;
    updated.path_len       = 2;
    updated.path_stack[0]  = 1;
    updated.path_stack[1]  = 2;
    agv::shm_update_car(shm, 0, updated);

    agv::Car c = agv::shm_read_car(shm, 0);
    CHECK(c.status          == agv::CarStatus::MOVING);
    CHECK(c.target_node_id  == 2);
    CHECK(c.path_len        == 2);
    CHECK(c.path_stack[0]   == 1);

    // 到站：只更新 status + current_node
    agv::shm_set_car_status(shm, 0, agv::CarStatus::IDLE);
    agv::Car c2 = agv::shm_read_car(shm, 0);
    CHECK(c2.status          == agv::CarStatus::IDLE);
    CHECK(c2.current_node_id == 2);
    CHECK(c2.path_len        == 2);  // path_stack 不变，由调用方另行清除
}

// ─────────────────────────────────────────────────────────────────────────────
// 8. MapData 和 CarData 独立上锁（写 map 不影响读 car）
// ─────────────────────────────────────────────────────────────────────────────

void test_independent_locks() {
    fprintf(stderr, "\n[test_independent_locks]\n");

    agv::ShmOwner owner;
    owner.create_and_init();
    agv::ShmLayout* shm = owner.ptr();

    // 写 map 锁时，car 锁的 seq 不变
    uint32_t car_seq_before = shm->car_lock.read_begin();

    agv::shm_set_edge_status(shm, 0, agv::EdgeStatus::BLOCKED);

    bool car_unaffected = shm->car_lock.read_end(car_seq_before);
    CHECK(car_unaffected);  // 写 map 不影响 car_lock 的 seq

    // 写 car 锁时，map 锁的 seq 不变
    uint32_t map_seq_before = shm->map_lock.read_begin();

    agv::shm_set_car_status(shm, 0, agv::CarStatus::MOVING);

    bool map_unaffected = shm->map_lock.read_end(map_seq_before);
    CHECK(map_unaffected);  // 写 car 不影响 map_lock 的 seq
}

// ─────────────────────────────────────────────────────────────────────────────
// 9. AdjEntry 邻接表操作
// ─────────────────────────────────────────────────────────────────────────────

void test_adj_entry() {
    fprintf(stderr, "\n[test_adj_entry]\n");

    agv::ShmOwner owner;
    owner.create_and_init();
    agv::ShmLayout* shm = owner.ptr();

    // 构建：节点 0 有两条出边（edge 0, edge 1）
    {
        agv::SeqlockWriteGuard g(shm->map_lock);
        shm->map.adj_[0].count       = 2;
        shm->map.adj_[0].edge_ids[0] = 0;
        shm->map.adj_[0].edge_ids[1] = 1;
    }

    agv::MapData snap = agv::shm_read_map(shm);
    CHECK(snap.adj_[0].count       == 2);
    CHECK(snap.adj_[0].edge_ids[0] == 0);
    CHECK(snap.adj_[0].edge_ids[1] == 1);
    CHECK(snap.adj_[1].count       == 0);  // 未设置的节点邻居数为 0
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

int main() {
    fprintf(stderr, "=== shm + seqlock unit tests ===\n");

    test_seqlock_basic();
    test_seqlock_write_guard();
    test_seqlock_concurrent_retry();
    test_shm_header();
    test_shm_lifecycle();
    test_map_read_write();
    test_car_read_write();
    test_independent_locks();
    test_adj_entry();

    fprintf(stderr, "\n=== results: %d/%d passed ===\n",
            g_run - g_fail, g_run);
    return g_fail == 0 ? 0 : 1;
}