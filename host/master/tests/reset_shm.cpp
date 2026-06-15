/**
 * reset_shm.cpp — 重置共享内存到初始状态
 *
 * 行为和 model_manager 的 init_map() + init_car() 完全一致。
 * 1. 销毁现有 SHM（shm_unlink）
 * 2. 重新创建并初始化（ShmOwner::create_and_init）
 * 3. 写入地图数据（节点、边、邻接表、双向边）
 * 4. 写入小车数据
 *
 * 编译（在 build 目录下）：
 *   cmake .. && make reset_shm
 *
 * 运行（建议先停止 model_manager）：
 *   sudo ./reset_shm
 */

#include "shm_manager.h"

#include <cstdio>
#include <cstring>
#include <vector>

// ── 枚举转字符串（仅用于打印日志） ─────────────────────────────────────────

static void log(const char* msg) {
    fprintf(stdout, "[reset_shm] %s\n", msg);
}

// ── 初始化地图：与 model_manager.cpp 的 init_map() 完全一致 ──────────────

static void init_map(agv::ShmLayout* shm) {
    shm->map.node_count_ = 15;
    shm->map.edge_count_ = 18 * 2;   // 18 条双向边 → 36 条单向边
    shm->bipaths.bipath_count_ = 0;

    // 注意：offset_=18，idA 从 1 开始，idB = idA + 18
    std::vector<agv::bipath_pair> edges{
        agv::bipath_pair::create(1, 4,  1, 18, 10, agv::EdgeStatus::IDLE, "L1"),
        agv::bipath_pair::create(2, 5,  2, 18, 10, agv::EdgeStatus::IDLE, "L2"),
        agv::bipath_pair::create(3, 6,  3, 18, 10, agv::EdgeStatus::IDLE, "L3"),

        agv::bipath_pair::create(4, 5,  4, 18, 10, agv::EdgeStatus::IDLE, "L4"),
        agv::bipath_pair::create(5, 6,  5, 18, 10, agv::EdgeStatus::IDLE, "L5"),

        agv::bipath_pair::create(4, 7,  6, 18, 10, agv::EdgeStatus::IDLE, "L6"),
        agv::bipath_pair::create(5, 8,  7, 18, 10, agv::EdgeStatus::IDLE, "L7"),
        agv::bipath_pair::create(6, 9,  8, 18, 10, agv::EdgeStatus::IDLE, "L8"),

        agv::bipath_pair::create(7, 8,  9, 18, 10, agv::EdgeStatus::IDLE, "L9"),
        agv::bipath_pair::create(8, 9,  10,18, 10, agv::EdgeStatus::IDLE, "L10"),

        agv::bipath_pair::create(7, 10, 11,18, 10, agv::EdgeStatus::IDLE, "L11"),
        agv::bipath_pair::create(8, 11, 12,18, 10, agv::EdgeStatus::IDLE, "L12"),
        agv::bipath_pair::create(9, 12, 13,18, 10, agv::EdgeStatus::IDLE, "L13"),

        agv::bipath_pair::create(10,11, 14,18, 10, agv::EdgeStatus::IDLE, "L14"),
        agv::bipath_pair::create(11,12, 15,18, 10, agv::EdgeStatus::IDLE, "L15"),

        agv::bipath_pair::create(10,13, 16,18, 10, agv::EdgeStatus::IDLE, "L16"),
        agv::bipath_pair::create(11,14, 17,18, 10, agv::EdgeStatus::IDLE, "L17"),
        agv::bipath_pair::create(12,15, 18,18, 10, agv::EdgeStatus::IDLE, "L18")
    };

    for (auto& e : edges) {
        shm->map.edges_[e.idA - 1] = e.generate_edgeA();
        shm->map.edges_[e.idB - 1] = e.generate_edgeB();
        shm->bipaths.paths_[e.idA - 1] = e;  // 父类赋值
        shm->bipaths.bipath_count_++;
    }

    // 初始化邻接表（只向 from→to 方向添加出边，与 model_manager 一致）
    for (uint16_t i = 0; i < shm->map.edge_count_; i++) {
        const auto& e = shm->map.edges_[i];
        uint16_t from_idx = e.from_node - 1;  // node id → 0-based 索引

        auto& adj_from = shm->map.adj_[from_idx];
        if (adj_from.count < AGV_MAX_NEIGHBORS) {
            adj_from.edge_ids[adj_from.count++] = e.id;
        }
    }

    // 节点列表
    shm->map.nodes_[0]  = agv::Node{1,  70,  70,  agv::NodeStatus::IDLE, "N1",  ""};
    shm->map.nodes_[1]  = agv::Node{2,  70,  190, agv::NodeStatus::IDLE, "N2",  ""};
    shm->map.nodes_[2]  = agv::Node{3,  70,  310, agv::NodeStatus::IDLE, "N3",  ""};

    shm->map.nodes_[3]  = agv::Node{4,  260, 70,  agv::NodeStatus::IDLE, "N4",  ""};
    shm->map.nodes_[4]  = agv::Node{5,  260, 190, agv::NodeStatus::IDLE, "N5",  ""};
    shm->map.nodes_[5]  = agv::Node{6,  260, 310, agv::NodeStatus::IDLE, "N6",  ""};

    shm->map.nodes_[6]  = agv::Node{7,  450, 70,  agv::NodeStatus::IDLE, "N7",  ""};
    shm->map.nodes_[7]  = agv::Node{8,  450, 190, agv::NodeStatus::IDLE, "N8",  ""};
    shm->map.nodes_[8]  = agv::Node{9,  450, 310, agv::NodeStatus::IDLE, "N9",  ""};

    shm->map.nodes_[9]  = agv::Node{10, 640, 70,  agv::NodeStatus::IDLE, "N10", ""};
    shm->map.nodes_[10] = agv::Node{11, 640, 190, agv::NodeStatus::IDLE, "N11", ""};
    shm->map.nodes_[11] = agv::Node{12, 640, 310, agv::NodeStatus::IDLE, "N12", ""};

    shm->map.nodes_[12] = agv::Node{13, 830, 70,  agv::NodeStatus::IDLE, "N13", ""};
    shm->map.nodes_[13] = agv::Node{14, 830, 190, agv::NodeStatus::IDLE, "N14", ""};
    shm->map.nodes_[14] = agv::Node{15, 830, 310, agv::NodeStatus::IDLE, "N15", ""};
}

// ── 初始化小车：与 model_manager.cpp 的 init_car() 完全一致 ──────────────

static void init_car(agv::ShmLayout* shm) {
    shm->cars.car_count_ = 2;

    // car id 从 1 开始
    shm->cars.cars_[0] = {
        .id                 = 1,
        .status             = agv::CarStatus::IDLE,
        .current_node_id    = 4,
        .current_task_id    = 0,
        .target_node_id     = 0,
        .last_node_id       = 1,
        .last_start_node_id = 1,
        .path_len           = 0,
    };
    shm->cars.cars_[1] = {
        .id                 = 2,
        .status             = agv::CarStatus::IDLE,
        .current_node_id    = 5,
        .current_task_id    = 0,
        .target_node_id     = 0,
        .last_node_id       = 2,
        .last_start_node_id = 2,
        .path_len           = 0,
    };
}

// ── 主程序 ──────────────────────────────────────────────────────────────────

int main() {
    log("开始重置共享内存...");

    // 用 ShmOwner 销毁并重建共享内存
    // create_and_init() 内部会先 shm_unlink，再重新创建、mmap、初始化 header
    agv::ShmOwner owner;
    try {
        owner.create_and_init();
    } catch (const std::exception& e) {
        fprintf(stderr, "[reset_shm] 创建共享内存失败: %s\n", e.what());
        return 1;
    }

    agv::ShmLayout* shm = owner.ptr();
    log("共享内存创建成功，开始写入初始数据...");

    // 用 SeqlockMWWriteGuard 保护 MapData 写入
    {
        agv::SeqlockMWWriteGuard g(shm->map_write_mutex, shm->map_lock);
        init_map(shm);
    }

    // 用 SeqlockMWWriteGuard 保护 CarData 写入
    {
        agv::SeqlockMWWriteGuard g(shm->car_write_mutex, shm->car_lock);
        init_car(shm);
    }

    log("重置完成！");
    fprintf(stdout,
        "[reset_shm] 地图: %u 节点, %u 条边  | 小车: %u 辆  | 双向边: %u 对\n",
        shm->map.node_count_, shm->map.edge_count_,
        shm->cars.car_count_, shm->bipaths.bipath_count_);

    log("请重新启动 model_manager 以接管共享内存。");

    return 0;
}
