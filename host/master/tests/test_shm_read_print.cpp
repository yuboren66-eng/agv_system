/**
 * test_shm_read_print.cpp — 读取共享内存并打印到终端
 *
 * 参照 shm_layout.h 中 ShmLayout 的结构，以 ShmClient 方式 attach
 * 到已有的共享内存，将 MapData、CarData、bipathData 全部读取并打印。
 *
 * 编译（在 build 目录下）：
 *   cmake .. && make test_shm_read_print
 *
 * 或直接：
 *   g++ -std=c++17 -I.. -I../lib \
 *       -o test_shm_read_print test_shm_read_print.cpp \
 *       -lrt -lpthread
 */

#include "shm_manager.h"

#include <cstdio>
#include <cstring>

// ── 枚举转字符串辅助 ──────────────────────────────────────────────────────────

static const char* car_status_str(agv::CarStatus s) {
    using CS = agv::CarStatus;
    switch (s) {
        case CS::IDLE:    return "IDLE";
        case CS::MOVING:  return "MOVING";
        case CS::FAULT:   return "FAULT";
        case CS::OFFLINE: return "OFFLINE";
        case CS::WAIT:    return "WAIT";
        default:          return "UNKNOWN";
    }
}

static const char* node_status_str(agv::NodeStatus s) {
    using NS = agv::NodeStatus;
    switch (s) {
        case NS::IDLE:     return "IDLE";
        case NS::OCCUPIED: return "OCCUPIED";
        case NS::FAULT:    return "FAULT";
        default:           return "UNKNOWN";
    }
}

static const char* edge_status_str(agv::EdgeStatus s) {
    using ES = agv::EdgeStatus;
    switch (s) {
        case ES::IDLE:         return "IDLE";
        case ES::OCCUPIED:     return "OCCUPIED";
        case ES::BLOCKED:      return "BLOCKED";
        case ES::FAULT_TEMP:   return "FAULT_TEMP";
        case ES::FAULT_REPAIR: return "FAULT_REPAIR";
        default:               return "UNKNOWN";
    }
}

// ── 主程序 ───────────────────────────────────────────────────────────────────-

int main() {
    // 1. attach 到已有的共享内存
    agv::ShmClient client;
    try {
        client.attach(500);  // 最多等 500ms 等 owner 初始化
    } catch (const std::exception& e) {
        fprintf(stderr, "[ERROR] 无法 attach 共享内存: %s\n", e.what());
        fprintf(stderr, "请确认 model_manager 已在运行且 /dev/shm/%s 存在。\n",
                agv::kShmName + 1); // 去掉前导斜杠
        return 1;
    }

    const agv::ShmLayout* shm = client.ptr();

    // ── 打印 Header 信息 ───────────────────────────────────────────────────

    printf("\n========================================\n");
    printf(" 共享内存状态 (magic=0x%08x, version=%u)\n",
           shm->header.magic, shm->header.version);
    printf(" 大小: %u 字节, initialized=%d\n",
           shm->header.shm_size, shm->header.initialized);
    printf("========================================\n");

    // ── 打印 MapData ───────────────────────────────────────────────────────

    agv::MapData map = agv::shm_read_map(const_cast<agv::ShmLayout*>(shm));
    printf("\n── MapData ──  node_count=%u  edge_count=%u\n",
           map.node_count_, map.edge_count_);

    printf("\n  节点列表:\n");
    for (uint16_t i = 0; i < map.node_count_ && i < AGV_MAX_NODES; ++i) {
        const auto& n = map.nodes_[i];
        printf("    [%u] id=%-4u  (%-4u,%-4u)  status=%-10s  name='%s'  label='%s'\n",
               i, n.id, n.x, n.y,
               node_status_str(n.status), n.name, n.label);
    }

    printf("\n  边列表:\n");
    for (uint16_t i = 0; i < map.edge_count_ && i < AGV_MAX_EDGES; ++i) {
        const auto& e = map.edges_[i];
        printf("    [%u] id=%-4u  %u → %u  w=%-4u  status=%-12s  label='%s'\n",
               i, e.id, e.from_node, e.to_node, e.weight,
               edge_status_str(e.status), e.label);
    }

    printf("\n  邻接表:\n");
    for (uint16_t i = 0; i < map.node_count_ && i < AGV_MAX_NODES; ++i) {
        const auto& adj = map.adj_[i];
        if (adj.count == 0) continue;
        printf("    node %u -> ", i);
        for (uint8_t j = 0; j < adj.count; ++j) {
            printf("edge_id=%u ", adj.edge_ids[j]);
        }
        printf("\n");
    }

    // ── 打印 CarData ───────────────────────────────────────────────────────

    agv::CarData cars = agv::shm_read_cars(const_cast<agv::ShmLayout*>(shm));
    printf("\n── CarData ──  car_count=%u\n", cars.car_count_);

    for (uint16_t i = 0; i < cars.car_count_ && i < AGV_MAX_CARS; ++i) {
        const auto& c = cars.cars_[i];
        printf("    [%u] id=%u  status=%-8s  last=%u  node=%u  target=%u"
               "  task=%u  path_len=%u  path=[",
               i, c.id, car_status_str(c.status),
               c.last_node_id,c.current_node_id, c.target_node_id,
               c.current_task_id, c.path_len);
        for (uint8_t j = 0; j < c.path_len && j < AGV_MAX_PATHLEN; ++j) {
            printf("%s%u", (j > 0) ? "," : "", c.path_stack[j]);
        }
        printf("]\n");
    }

    // ── 打印 bipathData ────────────────────────────────────────────────────

    agv::bipathData bipaths = agv::shm_read_bipaths(const_cast<agv::ShmLayout*>(shm));
    printf("\n── bipathData ──  bipath_count=%u\n", bipaths.bipath_count_);

    for (uint16_t i = 0; i < bipaths.bipath_count_ && i < AGV_MAX_EDGES / 2; ++i) {
        const auto& bp = bipaths.paths_[i];
        printf("    [%u] idA=%u  idB=%u\n", i, bp.idA, bp.idB);
    }

    printf("\n========================================\n");
    printf(" 打印完成\n");
    printf("========================================\n");

    return 0;
}
