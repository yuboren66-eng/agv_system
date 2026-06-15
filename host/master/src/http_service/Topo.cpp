#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#include "shm_manager.h"
#include "mq_wrapper.h"
#include "logger.h"
#include <fcgi_stdio.h>
#include "fcgi_utils.h"
using namespace agv::http;
const char* proc_name = "topo_api";

// ── 在 SHM 中按 edge.id 查找数组下标 ────────────────────────────────────────

static int find_edge_idx(const agv::MapData& map, uint16_t edge_id) {
    for (uint16_t i = 0; i < map.edge_count_; ++i) {
        if (map.edges_[i].id == edge_id) return i;
    }
    return -1;
}

static int find_node_idx(const agv::MapData& map, uint16_t node_id) {
    for (uint16_t i = 0; i < map.node_count_; ++i) {
        if (map.nodes_[i].id == node_id) return i;
    }
    return -1;
}

// ── 触发所有在途小车重规划 ───────────────────────────────────────────────────

static void trigger_replan_all(agv::ShmLayout* shm,
                                agv::MqSender<agv::TaskDispatchMsg>& mq,
                                bool mq_ok) {
    if (!mq_ok) return;
    agv::CarData snap = agv::shm_read_cars(shm);
    for (uint16_t i = 0; i < snap.car_count_; ++i) {
        auto s = static_cast<uint8_t>(snap.cars_[i].status);
        if (s == 1||s==4 /*MOVING or WAITING*/) {
            auto msg = agv::TaskDispatchMsg::replan(snap.cars_[i].id);
            mq.send(msg, agv::kPrioHigh);
        }
    }
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
    // SHM attach
    agv::ShmClient shm_client;
    bool shm_ok = false;
    try {
        shm_client.attach(1000);
        shm_ok = true;
    } catch (const std::exception& e) {
        dprintf(2, "[status] SHM not available: %s\n", e.what());
        //LOG_ERROR(proc_name, "SHM not available: %s", e.what());
    }

    // MQ 发送端
    agv::MqSender<agv::TaskDispatchMsg> mq;
    bool mq_ok = false;
    try {
        mq.init(agv::kMqTaskDispatch, agv::kTaskDispatchMsgSize);
        mq_ok = true;
    } catch (const std::exception& e) {
        dprintf(2, "[status] MQ not available: %s\n", e.what());
        //LOG_ERROR(proc_name, "MQ not available: %s", e.what());
    }

    while (FCGI_Accept() >= 0) {
        if (handle_preflight()) continue;

        const char* uri    = getenv("REQUEST_URI");
        const char* method = getenv("REQUEST_METHOD");

        if (!method || strcmp(method, "POST") != 0) {
            reply_err(405, "只支持 POST 方法");
            continue;
        }

        std::string body = read_body();
        if (body.empty()) {
            reply_err(400, "请求体为空");
            continue;
        }

        // 解析公共字段
        std::vector<std::string> edges = json_str_array(body, "edges");
        std::vector<std::string> nodes = json_str_array(body, "nodes");

        // ── /api/ban ─────────────────────────────────────────────
        if (uri && strstr(uri, "/api/ban")) {
            if (shm_ok) {
                agv::ShmLayout* shm = shm_client.ptr();
                agv::MapData snap   = agv::shm_read_map(shm);
                agv::bipathData bipath_snap = agv::shm_read_bipaths(shm);

                for (auto& es : edges) {
                    uint16_t eid = edge_id_from_str(es);
                    int idx = find_edge_idx(snap, eid);
                    if (idx >= 0)

                        agv::shm_set_edge_status(shm, idx, agv::EdgeStatus::BLOCKED);
                    //同时封禁该边的反向边（如果存在）
                    if(eid<=0||eid>bipath_snap.bipath_count_)
		            {
                        LOG_WARN(proc_name,"invalid edge id %u",eid);
                        continue;//边 ID 无效或不是双向边
                    }
                    auto it=bipath_snap.paths_[eid-1];
                    uint16_t other_eid;
                    it.get_other_path(eid, &other_eid);
                    idx = find_edge_idx(snap,other_eid);
                    LOG_INFO(proc_name,"finding inv edge:%d",idx);
                    if(idx==-1){
                        LOG_WARN(proc_name,"other edge of %u not found",eid);
                        continue;//反向边不存在
                    }
                    LOG_INFO(proc_name,"blocked the inv edge of %d",idx);
                    if(idx>=0)agv::shm_set_edge_status(shm, idx, agv::EdgeStatus::BLOCKED);
                }
                for (auto& ns : nodes) {
                    uint16_t nid = node_id_from_str(ns);
                    int idx = find_node_idx(snap, nid);
                    if (idx >= 0) {
                        agv::SeqlockMWWriteGuard g(shm->map_write_mutex, shm->map_lock);
                        shm->map.nodes_[idx].status = agv::NodeStatus::FAULT;
                    }
                }
                trigger_replan_all(shm, mq, mq_ok);
            }

            dprintf(2, "[topo_api] ban edges=%zu nodes=%zu\n", edges.size(), nodes.size());

            // 构建 data JSON
            char data[1024];
            size_t dp = 0;
            dp += snprintf(data+dp, sizeof(data)-dp, "{\"edges\":[");
            for (size_t i = 0; i < edges.size(); ++i)
                dp += snprintf(data+dp, sizeof(data)-dp,
                               "%s\"%s\"", i?",":"", edges[i].c_str());
            dp += snprintf(data+dp, sizeof(data)-dp, "],\"nodes\":[");
            for (size_t i = 0; i < nodes.size(); ++i)
                dp += snprintf(data+dp, sizeof(data)-dp,
                               "%s\"%s\"", i?",":"", nodes[i].c_str());
            snprintf(data+dp, sizeof(data)-dp, "]}");
            reply_ok("禁止通行已生效", data);
        }

        // ── /api/access ──────────────────────────────────────────
        else if (uri && strstr(uri, "/api/access")) {
            if (shm_ok) {
                agv::ShmLayout* shm = shm_client.ptr();
                agv::MapData snap   = agv::shm_read_map(shm);
		agv::bipathData bipath_snap = agv::shm_read_bipaths(shm);
                for (auto& es : edges) {
                    uint16_t eid = edge_id_from_str(es);
                    int idx = find_edge_idx(snap, eid);
                    if (idx >= 0)
                        agv::shm_set_edge_status(shm, idx, agv::EdgeStatus::IDLE);

                    //同时封禁该边的反向边（如果存在）
                    if(eid<=0||eid>bipath_snap.bipath_count_){
                        //LOG_WARN(proc_name,"invalid edge id %u",eid);
                        continue;//边 ID 无效或不是双向边
                    }
                    auto it=bipath_snap.paths_[eid-1];
                    uint16_t other_eid;
                    it.get_other_path(eid, &other_eid);
                    idx = find_edge_idx(snap,other_eid);
                    if(idx==-1){
                        //LOG_WARN(proc_name,"other edge of %u not found",eid);
                        continue;//反向边不存在
                    }
                    if(idx>=0)agv::shm_set_edge_status(shm, idx, agv::EdgeStatus::IDLE);
                }
                for (auto& ns : nodes) {
                    uint16_t nid = node_id_from_str(ns);
                    int idx = find_node_idx(snap, nid);
                    if (idx >= 0) {
                        agv::SeqlockMWWriteGuard g(shm->map_write_mutex, shm->map_lock);
                        shm->map.nodes_[idx].status = agv::NodeStatus::IDLE;
                    }
                }
                trigger_replan_all(shm, mq, mq_ok);
            }

            dprintf(2, "[topo_api] access edges=%zu nodes=%zu\n", edges.size(), nodes.size());

            char data[1024];
            size_t dp = 0;
            dp += snprintf(data+dp, sizeof(data)-dp, "{\"edges\":[");
            for (size_t i = 0; i < edges.size(); ++i)
                dp += snprintf(data+dp, sizeof(data)-dp,
                               "%s\"%s\"", i?",":"", edges[i].c_str());
            dp += snprintf(data+dp, sizeof(data)-dp, "],\"nodes\":[");
            for (size_t i = 0; i < nodes.size(); ++i)
                dp += snprintf(data+dp, sizeof(data)-dp,
                               "%s\"%s\"", i?",":"", nodes[i].c_str());
            snprintf(data+dp, sizeof(data)-dp, "]}");
            reply_ok("已恢复通行", data);
        }

        // ── /api/repair ──────────────────────────────────────────
        else if (uri && strstr(uri, "/api/repair")) {
            if (shm_ok) {
                agv::ShmLayout* shm = shm_client.ptr();
                agv::MapData snap   = agv::shm_read_map(shm);

                for (auto& es : edges) {
                    uint16_t eid = edge_id_from_str(es);
                    int idx = find_edge_idx(snap, eid);
                    if (idx >= 0)
                        agv::shm_set_edge_status(shm, idx,
                                                  agv::EdgeStatus::FAULT_REPAIR);
                }
                trigger_replan_all(shm, mq, mq_ok);
            }

            dprintf(2, "[topo_api] repair edges=%zu\n", edges.size());

            char data[512];
            size_t dp = 0;
            dp += snprintf(data+dp, sizeof(data)-dp, "{\"edges\":[");
            for (size_t i = 0; i < edges.size(); ++i)
                dp += snprintf(data+dp, sizeof(data)-dp,
                               "%s\"%s\"", i?",":"", edges[i].c_str());
            snprintf(data+dp, sizeof(data)-dp, "]}");
            reply_ok("修复任务已成功下发", data);
        }

        else {
            reply_err(404, "接口不存在");
        }
    }
    return 0;
}
