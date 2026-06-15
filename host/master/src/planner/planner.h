#pragma once
#include <atomic>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <limits>
#include <cmath>
#include <algorithm>
//请确保log_daemon正常运行
#include "shm_manager.h"
#include "mq_wrapper.h"
#include "logger.h"
#include "signal_handler.h"
#include "secure_exit.h"

#include <string>//test only

extern const char* proc_name;

namespace agv{

    struct AStarNode{
        uint16_t id;
        float f_cost;
        bool operator>(const AStarNode &other) const
        {
            return f_cost > other.f_cost;
        }
    };

    class planner{
        public:
        bool init(){
            try {
                LOG_INFO(proc_name,"init");
                _shm.attach(500);
                _mq_send_redis.init(kMqTaskDispatch);
                _mq_send.init(kMqMqttPublish);
                _mq_route.init(kMqRouteExert);
            } catch (const std::exception& e) {
                LOG_ERROR(proc_name,"fail to create mq:%s",e.what());
                return false;
            }
            return true;
        }
        void do_cleanup(){
            LOG_INFO(proc_name,"do_cleanup");
        }

        bool handle_task(const TaskDispatchMsg& msg){
            LOG_INFO(proc_name,"handle_task car_id=%u target_node=%u imm=%u",
                    msg.car_id, msg.target_node, msg.immediate);
            auto map_data=shm_read_map(_shm.ptr());
            auto car_data=shm_read_cars(_shm.ptr());
            if(msg.car_id>car_data.car_count_||msg.car_id==0){
                LOG_ERROR(proc_name,"invaild car no.");
                return false;
            }
            auto current_car=car_data.cars_[msg.car_id-1];
            if(current_car.status==agv::CarStatus::FAULT||current_car.status==agv::CarStatus::OFFLINE)
            {
                LOG_ERROR(proc_name,"unavailable car");
                return false;
            }
            uint16_t target_node=msg.target_node,start_node=current_car.current_node_id;

            //对于特殊情况（重新规划/取消）-调整目标节点
            if(msg.action==agv::TaskAction::kReplan)target_node=current_car.target_node_id;
            if(msg.action==agv::TaskAction::kCancel)target_node=current_car.last_start_node_id;
            //对于bef_threshold为真的处理，0.判断掉头到新路线是否可行 1.调整出发的节点信息 2.调整当前节点信息为处理后的节点状态
            if(msg.immediate==ImmeStra::kUturnNoCross){start_node=current_car.last_node_id;}
            if(msg.immediate==ImmeStra::kUturn){
                //判断last_node_id引出的路线中有没有和当前所在路线反向的路径
                auto last_node_id=current_car.last_node_id;
                auto current_node_id=current_car.current_node_id;
                uint16_t rev_node=0xFFFF;
                for(int i=0;i<map_data.adj_[last_node_id-1].count;++i){
                    //遍历其他连接的路径
                    auto edge_id=map_data.adj_[last_node_id-1].edge_ids[i];
                    if(edge_id>=map_data.edge_count_)continue;
                    auto edge=map_data.edges_[edge_id-1];
                    auto node_s=map_data.nodes_[edge.to_node-1];

                    LOG_INFO(proc_name,"rev_judging,from node:%u, the edge: %u, to node:%u",
                            last_node_id,edge_id,edge.to_node);
                    
                    //确认对应的点不是当前点，并且线路没封
                    if(node_s.id==current_node_id||
                        edge.status==agv::EdgeStatus::BLOCKED||
                        edge.status!=agv::EdgeStatus::IDLE)continue;
                    //判断是否为反向点
                    if(jud_rev(map_data.nodes_[last_node_id-1].x,map_data.nodes_[last_node_id-1].y,
                        map_data.nodes_[current_node_id-1].x,map_data.nodes_[current_node_id-1].y,
                        node_s.x,node_s.y)){
                        rev_node=node_s.id;
                        LOG_INFO(proc_name,"rev-node-found,%u",rev_node);
                        //break;
                    }
                }
                if(rev_node==0xFFFF){
                    LOG_ERROR(proc_name,"failed to find a rev-node for the car%u will remain at node",msg.car_id);
                    //TODO:更新小车状态
                    return false;
                }else start_node=rev_node;
            }
            LOG_INFO(proc_name,"arr-start:%d,end:%d",start_node,target_node);
            auto path=find_path(map_data,start_node,target_node);
            LOG_INFO(proc_name,"finish plan, routeLenth:%u",path.size());
            //LOG_INFO(proc_name,"begin to transmit");
            //for (auto m : path) {
            //    LOG_INFO(proc_name,"cur:%d",m);
           // }
            //LOG_INFO(proc_name,"done");
            if(path.empty()||path.size()>AGV_MAX_PATHLEN){
                LOG_ERROR(proc_name,"fail to schedule | reason:A-no path available;B-path too long;C-pramgram err");
                switch(msg.action){
                    case agv::TaskAction::kAssign:
                        LOG_ERROR(proc_name,"failed to assign, reject the schedule for car %u",msg.car_id);
                        return false;
                    case agv::TaskAction::kCancel:
                        LOG_ERROR(proc_name,"failed to cancel, the car%u will remain at node",msg.car_id);
                        _mq_send.send(MqttPublishMsg::make_action(msg.car_id, ActionCmd::kPause),kPrioHigh);
                        //TODO: 更新线路状态
                        return false;
                    case agv::TaskAction::kReplan:
                        LOG_ERROR(proc_name,"failed to replan, prepare to cancel task for car %u",msg.car_id);
                        _mq_send_redis.send(TaskDispatchMsg::cancel(msg.car_id,msg.immediate),kPrioHigh);
                        return false;
                }
                //下面这一段我也不知道哪来的
                //if(car_data.cars_[msg.car_id-1].current_node_id!=car_data.cars_[msg.car_id-1].current_node_id){
                //}
                return false;
            }
            //更新路径信息and小车信息
            if(msg.immediate==ImmeStra::kUturnNoCross)current_car.last_node_id=current_car.current_node_id;
            current_car.current_node_id=start_node;
            current_car.target_node_id=target_node;
            current_car.path_len=0;
            current_car.status=CarStatus::MOVING;
            for(auto i:path){
                current_car.path_stack[current_car.path_len]=i;
                current_car.path_len++;
            }
            shm_update_car(_shm.ptr(),msg.car_id-1,current_car);

            //根据immediate即时发布对应信息
            _mq_send.send(MqttPublishMsg::make_cnt(msg.car_id,path.size()),kPrioHigh);
            switch(msg.immediate){
                case agv::ImmeStra::kUturn:{
                    _mq_send.send(MqttPublishMsg::make_action(msg.car_id, ActionCmd::kUturn),kPrioHigh);
                    break;
                }
                case agv::ImmeStra::kUturnNoCross:{
                    _mq_send.send(MqttPublishMsg::make_action(msg.car_id, ActionCmd::kUturn),kPrioHigh);
                    break;
                }
                case agv::ImmeStra::kImme:{
                    _mq_send.send(MqttPublishMsg::make_action(msg.car_id, ActionCmd::kProcess),kPrioHigh);
                    break;
                }
            }
            LOG_INFO(proc_name,"find_path result size=%zu",path.size());
	    return true;
        }
        private:
        //(x1,y1)为起点坐标，(x2,y2)为当前点坐标，(x3,y3)为目标点坐标
        bool jud_rev(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2,uint16_t x3,uint16_t y3){
            int cal_a=(int(x2)-int(x1))*(int(y3)-int(y1));
            int cal_b=(int(y2)-int(y1))*(int(x3)-int(x1));
            if(cal_a==cal_b){
                //区分直行和掉头
                return (int(x3)-int(x2))*(int(x2)-int(x1))+(int(y3)-int(y2))*(int(y2)-int(y1))<=0;
            }
            return false;
        }
        std::vector<uint16_t> find_path(agv::MapData map_data, uint16_t start_node, uint16_t target_node) {
            std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>> open_set;

            int n = map_data.node_count_;
            const float INF = std::numeric_limits<float>::max();
            constexpr float SOFT_OBSTACLE_PENALTY = 100.0f;

            std::vector<float> g_cost(n, INF);
            std::vector<int> came_from(n, -1);
            std::vector<int> came_edge(n, -1);
            std::vector<bool> closed(n, false);

            if (start_node >= AGV_MAX_NODES || target_node >= AGV_MAX_NODES)
                return {};
            if (start_node >= n || target_node >= n)
                return {};
            g_cost[start_node] = 0.0f;
            open_set.push({start_node, heuristic(map_data.nodes_[start_node-1], map_data.nodes_[target_node-1])});

            while (!open_set.empty()) {
                AStarNode current_node = open_set.top();
                open_set.pop();

                uint16_t current = current_node.id;

                if (closed[current])
                    continue;
                closed[current] = true;

                if (current == target_node) {
                    std::vector<uint16_t> path_edges;
                    int cur = target_node;
                    while (came_from[cur] != -1) {
                        path_edges.push_back(static_cast<uint16_t>(came_edge[cur]));
                        cur = came_from[cur];
                    }
                    std::reverse(path_edges.begin(), path_edges.end());
                    return path_edges;
                }
                const AdjEntry& adj = map_data.adj_[current-1];
                for (int i = 0; i < adj.count; ++i) {
                    uint16_t edge_id = adj.edge_ids[i];
                    if (edge_id >= AGV_MAX_EDGES || edge_id >= map_data.edge_count_)
                        continue;

                    const Edge& edge = map_data.edges_[edge_id-1];
                    uint16_t next_node = (edge.from_node == current) ? edge.to_node : edge.from_node;
                    if (next_node >= AGV_MAX_NODES || next_node >= n)
                        continue;
                    const Node& next_ptr = map_data.nodes_[next_node-1];
                    EdgeStatus e_status = edge.status;
                    NodeStatus n_status = next_ptr.status;
                    if (e_status == EdgeStatus::BLOCKED ||
                        e_status == EdgeStatus::FAULT_REPAIR ||
                        n_status == NodeStatus::FAULT)
                        continue;
                    float penalty = 0.0f;
                    if (e_status == EdgeStatus::OCCUPIED ||
                        e_status == EdgeStatus::FAULT_TEMP ||
                        n_status == NodeStatus::OCCUPIED)
                        penalty = SOFT_OBSTACLE_PENALTY;

                    float new_g = g_cost[current] + static_cast<float>(edge.weight) + penalty;

                    if (new_g < g_cost[next_node]) {
                        g_cost[next_node] = new_g;
                        came_from[next_node] = static_cast<int>(current);
                        came_edge[next_node] = static_cast<int>(edge_id);

                        float f = new_g + heuristic(next_ptr, map_data.nodes_[target_node]);
                        open_set.push({next_node, f});
                    }
                }
            }
            return {};
        }
        static float heuristic(const Node& a, const Node& b) {
            float dx = static_cast<float>(a.x) - static_cast<float>(b.x);
            float dy = static_cast<float>(a.y) - static_cast<float>(b.y);
            return std::sqrt(dx * dx + dy * dy);
        }
        agv::ShmClient _shm;
        agv::MqSender<TaskDispatchMsg> _mq_send_redis;//发送调度消息，用于重排
        agv::MqSender<MqttPublishMsg> _mq_send;//发送mqtt消息，用于即时指令
        agv::MqSender<RouteExertMsg> _mq_route;//发送路径启动消息，用于即时指令
    };
}
