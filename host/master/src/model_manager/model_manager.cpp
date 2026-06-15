#include <cassert>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include <sys/eventfd.h>
#include <unistd.h>
//请确保log_daemon正常运行
#include "shm_manager.h"
#include "mq_wrapper.h"
#include "logger.h"
#include "signal_handler.h"
#include "secure_exit.h"


const char* proc_name="model-mng";

static std::atomic<bool> g_reset_requested{false};

static void on_sigusr1(const char* proc_name) {
    LOG_INFO(proc_name, "SIGUSR1 received, request reset");
    g_reset_requested.store(true, std::memory_order_release);
}

//初始状态 TODO
void init_map(agv::ShmLayout* shm_ptr){
    shm_ptr->map.node_count_ = 15;
    shm_ptr->map.edge_count_ = 18*2;
    shm_ptr->bipaths.bipath_count_=0;
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
    for(auto & e:edges){
        shm_ptr->map.edges_[e.idA-1] = e.generate_edgeA();
        shm_ptr->map.edges_[e.idB-1] = e.generate_edgeB();
        shm_ptr->bipaths.paths_[e.idA-1]=e;//这里会使用父类吗？？
        shm_ptr->bipaths.bipath_count_++;
        LOG_INFO(proc_name,"write edge in idx:%d-id:%d and its bipath idx:%d-id:%d"
            ,e.idA-1,shm_ptr->map.edges_[e.idA-1].id,e.idB-1,shm_ptr->map.edges_[e.idB-1].id);
    }

    // 初始化邻接表：遍历所有边，将边索引加入两端节点的邻接表
    for (uint16_t i = 0; i < shm_ptr->map.edge_count_; i++) {
        const auto& e = shm_ptr->map.edges_[i];
        uint16_t from_idx = e.from_node - 1;  // node id → 0-based 索引
        uint16_t to_idx   = e.to_node - 1;

        auto& adj_from = shm_ptr->map.adj_[from_idx];
        if (adj_from.count < AGV_MAX_NEIGHBORS) {
            adj_from.edge_ids[adj_from.count++] = e.id;
        }

        //auto& adj_to = shm_ptr->map.adj_[to_idx];
        //if (adj_to.count < AGV_MAX_NEIGHBORS) {
        //    adj_to.edge_ids[adj_to.count++] = e.id;
        //}
    }
    shm_ptr->map.nodes_[0]  = agv::Node{1,  70,  70,  agv::NodeStatus::IDLE, "N1",  ""};
    shm_ptr->map.nodes_[1]  = agv::Node{2,  70,  190, agv::NodeStatus::IDLE, "N2",  ""};
    shm_ptr->map.nodes_[2]  = agv::Node{3,  70,  310, agv::NodeStatus::IDLE, "N3",  ""};

    shm_ptr->map.nodes_[3]  = agv::Node{4,  260, 70,  agv::NodeStatus::IDLE, "N4",  ""};
    shm_ptr->map.nodes_[4]  = agv::Node{5,  260, 190, agv::NodeStatus::IDLE, "N5",  ""};
    shm_ptr->map.nodes_[5]  = agv::Node{6,  260, 310, agv::NodeStatus::IDLE, "N6",  ""};

    shm_ptr->map.nodes_[6]  = agv::Node{7,  450, 70,  agv::NodeStatus::IDLE, "N7",  ""};
    shm_ptr->map.nodes_[7]  = agv::Node{8,  450, 190, agv::NodeStatus::IDLE, "N8",  ""};
    shm_ptr->map.nodes_[8]  = agv::Node{9,  450, 310, agv::NodeStatus::IDLE, "N9",  ""};

    shm_ptr->map.nodes_[9]  = agv::Node{10, 640, 70,  agv::NodeStatus::IDLE, "N10", ""};
    shm_ptr->map.nodes_[10] = agv::Node{11, 640, 190, agv::NodeStatus::IDLE, "N11", ""};
    shm_ptr->map.nodes_[11] = agv::Node{12, 640, 310, agv::NodeStatus::IDLE, "N12", ""};

    shm_ptr->map.nodes_[12] = agv::Node{13, 830, 70,  agv::NodeStatus::IDLE, "N13", ""};
    shm_ptr->map.nodes_[13] = agv::Node{14, 830, 190, agv::NodeStatus::IDLE, "N14", ""};
    shm_ptr->map.nodes_[14] = agv::Node{15, 830, 310, agv::NodeStatus::IDLE, "N15", ""};
}

// ── 地图重置函数（原地重写，不删除/重建 SHM）──────────────────────────────────
/**
 * reset_shm_map() — 在主循环检测到 eventfd 可读后调用。
 *
 * 保护策略：
 *   - map / adj 用 SeqlockMWWriteGuard 包裹整段写操作，
 *     读者（planner 等）会感知到 seq 变化并重试，不会读到中间态。
 *   - bipaths 由 owner 维护且其他进程只读，
 *     用同一个 map_write_mutex + map_lock 串联写即可（bipaths 无独立锁）。
 *   - cars 不在此处重置（reset_shm_map 只负责地图部分）。
 */
void reset_shm_map(agv::ShmLayout* shm_ptr) {
    LOG_INFO(proc_name, "reset_shm_map: begin");

    {
        agv::SeqlockMWWriteGuard guard(shm_ptr->map_write_mutex, shm_ptr->map_lock);
	init_map(shm_ptr);	
    }  // guard 析构：write_end + mutex unlock

    LOG_INFO(proc_name, "reset_shm_map: done");
}

void init_car(agv::ShmLayout* shm_ptr){
    shm_ptr->cars.car_count_ = 2;
    //car id从1开始
    shm_ptr->cars.cars_[0] = {
        .id              = 1,
        .status          = agv::CarStatus::IDLE,
        .current_node_id = 4,
        .current_task_id = 0,
        .target_node_id  = 0,
	.last_node_id	 = 1,
        .last_start_node_id=1,
        .path_len        = 0,
    };
    shm_ptr->cars.cars_[1] = {
        .id              = 2,
        .status          = agv::CarStatus::IDLE,
        .current_node_id = 5,
        .current_task_id = 0,
        .target_node_id  = 0,
	.last_node_id	 = 2,
        .last_start_node_id=2,
        .path_len        = 0,
    };
}


int main(){
    LOG_INFO(proc_name,"begin");
    agv::SignalHandler sig(proc_name, on_sigusr1);
    try {
        sig.init();
    } catch (const std::exception& e) {
        LOG_FATAL(proc_name,"%s",e.what());
        return 1;
    }

    //初始化业务资源
    agv::MqOwner owner_mq;
    try {
        owner_mq.create_all();
    } catch (const std::exception& e) {
        LOG_ERROR(proc_name,"fail to create mq:%s",e.what());
        return 1;
    }
    agv::ShmOwner owner_shm;
    try{
        owner_shm.create_and_init();
        agv::ShmLayout* shm_ptr = owner_shm.ptr();
        init_map(shm_ptr);
        init_car(shm_ptr);
        //SUG检查逻辑
    }catch (const std::exception& e) {
        LOG_ERROR(proc_name,"fail to create shm:%s",e.what());
        return 1;
    }


    //注册退出清理序列
    agv::SecureExit exit_seq(proc_name);
    exit_seq.add_cleanup("finish",[&]{LOG_INFO(proc_name,"finish unlinking mq");});
    exit_seq.add_cleanup("unlink-mq",[&]{owner_mq.unlink_all();});

    //组建 poll 监听数组
    constexpr int FD_SIG   = 0;

    struct pollfd fds[1];
    fds[FD_SIG].fd     = sig.get_fd();
    fds[FD_SIG].events = POLLIN;


    LOG_INFO(proc_name,"enter-poll-loop");

    //poll 主循环
    while (!sig.shutdown_requested()) {
        int nfds = sizeof(fds) / sizeof(fds[0]);
        int ret  = ::poll(fds, nfds, -1);  // 无限等待，全事件驱动

        if (ret < 0) {
            if (errno == EINTR) continue;   // 被打断，重试
            LOG_ERROR(proc_name,"poll:%s",strerror(errno));
            break;
        }

        if (fds[FD_SIG].revents & POLLIN) {
            sig.handle_read();
        if (g_reset_requested.exchange(false, std::memory_order_acq_rel)) {
            reset_shm_map(owner_shm.ptr());
        }
            continue;
        }

    }
    ::close(reset_efd);
    g_reset_efd = -1;
    LOG_INFO(proc_name,"shutdown-requested");
    exit_seq.run(200);
    LOG_INFO(proc_name,"shutdown-finished");
}
