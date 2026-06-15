#pragma once

#include "config/config.h"
#include "seqlock.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace agv{
    static constexpr char kShmName[] = "/agv_shm";
    /// 魔数，用于校验 SHM 是否由本程序初始化
    static constexpr uint32_t kShmMagic   = 0x41475631u;  // "AGV1"
    static constexpr uint32_t kShmVersion = 2u;

    enum class CarStatus : uint8_t {
        IDLE    = 0,
        MOVING  = 1,
        FAULT   = 2,
        OFFLINE = 3,
        WAIT    = 4,
    };

    enum class NodeStatus : uint8_t {
        IDLE     = 0,
        OCCUPIED = 1,
        FAULT    = 2,
    };

    enum class EdgeStatus : uint8_t {
        IDLE         = 0,
        OCCUPIED     = 1,
        BLOCKED      = 2,   // ban 接口设置
        FAULT_TEMP   = 3,
        FAULT_REPAIR = 4,
    };



    struct Car{
        uint8_t id;
        CarStatus status;
        uint8_t current_node_id;
        uint8_t current_task_id;
        uint8_t target_node_id;
        uint8_t last_node_id;
        uint8_t last_start_node_id;
        uint8_t path_len;
        uint16_t path_stack[AGV_MAX_PATHLEN];
    };
    struct Node {
        uint16_t   id;//请保证id和map中的索引严格一致
        uint16_t   x, y;
        NodeStatus status;
        char       name [AGC_MAX_NAME];
        char       label[AGC_MAX_LABEL];
        uint8_t    _pad[1];             // 补齐到偶数字节
    };
    struct Edge{
        uint16_t id;
        uint16_t from_node;
        uint16_t to_node;
        uint16_t weight;
        EdgeStatus status;
        char       label[AGC_MAX_LABEL];
    };

    /// 单个节点的邻接信息：存放从该节点出发的所有边 ID
    struct AdjEntry {
        uint8_t  count;                         ///< 有效邻居数
        uint8_t  _pad[1];
        uint16_t edge_ids[AGV_MAX_NEIGHBORS];   ///< 出边 ID 列表（通过 edge_id 查 Edge）
    };
    struct MapData {
        Node     nodes_[AGV_MAX_NODES];
        Edge     edges_[AGV_MAX_EDGES];
        AdjEntry adj_  [AGV_MAX_NODES];     ///< adj_[node_id] = 该节点的出边列表
        uint16_t node_count_;
        uint16_t edge_count_;
    };
    //DO_NOT_USE;

    struct CarData {
        Car      cars_[AGV_MAX_CARS];
        uint16_t car_count_;
    };
    //DO_NOT_USE;


        //双向边相关操作
    struct bipath{
        uint16_t idA;
        uint16_t idB;
        int main_path(uint16_t path_id) const {
            if(path_id == idA) return 1;
            if(path_id == idB) return 0;
            return -1;
        }
        int get_other_path(uint16_t path_id,uint16_t* other_path_id) const {
            if(path_id == idA) {
                *other_path_id = idB;
                return 1;
            }
            if(path_id == idB) {
                *other_path_id = idA;
                return 0;
            }
            *other_path_id=0xFFFF;
            return -1;
        }
    };

    struct bipathData{
        bipath paths_[AGV_MAX_EDGES/2];
        uint16_t bipath_count_;
    };

    struct bipath_pair:public bipath{
        uint16_t from_node;
        uint16_t to_node;
        uint16_t weight;
        agv::EdgeStatus status;
        char label[AGC_MAX_LABEL];

        static bipath_pair create(uint16_t from, uint16_t to, uint16_t id_a, uint16_t offset_, uint16_t w, agv::EdgeStatus s,const char* l)
        {        
            bipath_pair p;
            p.idA = id_a;
            p.idB = id_a + offset_;
            p.from_node = from;
            p.to_node = to;
            p.weight = w;
            p.status = s;
            strncpy(p.label, l, sizeof(p.label) - 1);
            p.label[sizeof(p.label) - 1] = '\0';
            return p;
        }
        Edge generate_edgeA() const {
            Edge e;
            e.id = idA;
            e.from_node = from_node;
            e.to_node = to_node;
            e.weight = weight;
            e.status = status;
            strncpy(e.label, label, sizeof(e.label) - 1);
            e.label[sizeof(e.label) - 1] = '\0';
            return e;
        }
        Edge generate_edgeB() const {
            Edge e;
            e.id = idB;
            e.from_node = to_node;
            e.to_node = from_node;
            e.weight = weight;
            e.status = status;
            strncpy(e.label, label, sizeof(e.label) - 1);
            e.label[sizeof(e.label) - 1] = '\0';
            return e;
        }
    };

    struct ShmHeader {
        uint32_t magic;         ///< == kShmMagic，用于 attach 时校验
        uint32_t version;       ///< 版本号，进程不匹配时拒绝 attach
        uint32_t shm_size;      ///< sizeof(ShmLayout)，二次校验
        uint8_t  initialized;   ///< 1 = model_manager 已完成初始化
        uint8_t  _pad[51];      ///< 补齐到 64 字节
    };
    static_assert(sizeof(ShmHeader) == 64, "ShmHeader must be 64 bytes");

    /**
    * ShmLayout — SHM 的完整内存布局
    *
    * 内存排列：
    *   [ShmHeader         ]  64 B   header 和校验信息
    *   [Seqlock map_lock  ]  64 B   MapData 的 seq 计数器（读者用）
    *   [ProcMutex         ]  64 B   MapData 的写者互斥锁（写者用）
    *   [MapData           ]  ~8 KB  地图数据
    *   [Seqlock car_lock  ]  64 B   CarData 的 seq 计数器（读者用）
    *   [ProcMutex         ]  64 B   CarData 的写者互斥锁（写者用）
    *   [CarData           ]  ~1 KB  小车状态
    *  [bipathData         ]  ~1 KB  双向边数据
    *
    * Seqlock 和 ProcMutex 分开排列而不是内嵌在数据结构里，
    * 是为了让它们不被写操作的 memcpy 覆盖。
    *
    * 注意：本版本将 kShmVersion 改为 2，旧进程 attach 时将拒绝连接。
    */
    struct ShmLayout {
        ShmHeader   header;              // offset 0

        Seqlock     map_lock;            // offset 64       — MapData 的 seq 计数器
        ProcMutex   map_write_mutex;     // offset 128      — MapData 的写者互斥锁（新增）
        MapData     map;                 // offset 192

        Seqlock     car_lock;            // 紧随 map 之后  — CarData 的 seq 计数器
        ProcMutex   car_write_mutex;     //                — CarData 的写者互斥锁（新增）
        CarData     cars;                // 紧随 car_write_mutex

        bipathData bipaths;             // 紧随 cars 之后  — 双向边数据
    };
}
