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

    class router{
        public:
        bool init(){
            try {
                LOG_INFO(proc_name,"init");
                _shm.attach(500);
                _mq_send.init(kMqMqttPublish);
            } catch (const std::exception& e) {
                LOG_ERROR(proc_name,"fail to create mq:%s",e.what());
                return false;
            }
            return true;
        }
        void do_cleanup(){
            LOG_INFO(proc_name,"do_cleanup");
        }

        bool handle_task(RouteExertMsg& msg){
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
            
            if(current_car.current_node_id==current_car.target_node_id){
                current_car.status=agv::CarStatus::IDLE;
                current_car.path_len=0;
                current_car.last_start_node_id=current_car.current_node_id;
                current_car.current_task_id=0;
                shm_update_car(_shm.ptr(),msg.car_id-1,current_car);
                _mq_send.send(MqttPublishMsg::make_ori(msg.car_id,agv::OriCmd::kARRIVED));
                return true;
            }
            //默认情况下，arrived_node和last_node为0xFFFF，表示由router根据当前状态推断
            if(msg.last_node==0xFFFF) msg.last_node=current_car.last_node_id;
            if(msg.arrived_node==0xFFFF) msg.arrived_node=current_car.current_node_id;
            LOG_INFO(proc_name,"handled-last:%u, arrived:%u",msg.last_node,msg.arrived_node);
            int next_path_idx=current_car.path_len-1;
            uint16_t next_id;
            for(;;next_path_idx--){
                if(next_path_idx<0){
                    //没有找到相关路径，出错。
                    LOG_ERROR(proc_name,"fail to find the next path for car%u",msg.car_id);
                    return false;
                }
                LOG_INFO(proc_name,"scanning,pathNo:%d, pathIdx:%d, path-from:%u, path-to:%u",
                    next_path_idx,current_car.path_stack[next_path_idx],
                    map_data.edges_[current_car.path_stack[next_path_idx]-1].from_node,
                    map_data.edges_[current_car.path_stack[next_path_idx]-1].to_node);
                if(map_data.edges_[current_car.path_stack[next_path_idx]-1].from_node==msg.arrived_node){
                    next_id=map_data.edges_[current_car.path_stack[next_path_idx]-1].to_node;
                    break;
                }
            }

            MqttPublishMsg mqtt_msg;
#ifndef _AGV_USE_ANGLE_MODE
            {
                auto turn_cmd=generate_turn(map_data.nodes_[current_car.last_node_id-1].x,map_data.nodes_[current_car.last_node_id-1].y,
                    map_data.nodes_[current_car.current_node_id-1].x,map_data.nodes_[current_car.current_node_id-1].y,
                    map_data.nodes_[next_id-1].x,map_data.nodes_[next_id-1].y);
                mqtt_msg = MqttPublishMsg::make_ori(msg.car_id,turn_cmd);
            }
#else
            static_assert(false,"function developing");
#endif
            _mq_send.send(mqtt_msg,kPrioHigh);
            current_car.last_node_id=current_car.current_node_id;
            current_car.current_node_id=next_id;//map_data.edges_[current_car.path_stack[next_path_idx]].to_node;
            shm_update_car(_shm.ptr(),msg.car_id-1,current_car);
            return true;
        }
        private:
        //(x1,y1)为起点坐标，(x2,y2)为当前点坐标，(x3,y3)为目标点坐标
        agv::OriCmd generate_turn(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2,uint16_t x3,uint16_t y3){
            int cal_a=(int(x2)-int(x1))*(int(y3)-int(y1));
            int cal_b=(int(y2)-int(y1))*(int(x3)-int(x1));
            if(cal_a==cal_b){
                //区分直行和掉头
                int cal_c=(int(x3)-int(x2))*(int(x2)-int(x1))+(int(y3)-int(y2))*(int(y2)-int(y1));
                if(cal_c>0)return agv::OriCmd::kStraight;
                else return agv::OriCmd::kUTurn;
            }
            else if(cal_a>cal_b)return agv::OriCmd::kRight;
            else return agv::OriCmd::kLeft;
        }
        agv::ShmClient _shm;
        agv::MqSender<MqttPublishMsg> _mq_send;//发送mqtt消息，用于即时指令
    };
}
