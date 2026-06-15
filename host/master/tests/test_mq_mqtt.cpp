#include <cassert>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <thread>
#include <chrono>

//请确保log_daemon正常运行
#include "mq_wrapper.h"
#include "logger.h"


const char* proc_name="mqtt-mq-sender";

int main(int argc, char *argv[]) {
    agv::MqSender<agv::MqttPublishMsg>  tx;
    if(argc<1){
        return 1;
    }
    
    try {
        tx.init(agv::kMqMqttPublish,  agv::kMqttPublishMsgSize);
    } catch (const std::exception& e) {
        fprintf(stderr, "[child] FATAL: %s\n", e.what());
        ::exit(1);
    }
    auto op_name=argv[1];
    agv::MqttPublishMsg msg=agv::MqttPublishMsg::make_action(1,agv::ActionCmd::kPause);
    switch (op_name[0]) {
        case 'a':
            if(op_name[1]=='1')msg=agv::MqttPublishMsg::make_angle(1,12);
            if(op_name[1]=='2')msg=agv::MqttPublishMsg::make_angle(1,312);
            break;
        case 'o':
            if(op_name[1]=='1')msg=agv::MqttPublishMsg::make_ori(1,agv::OriCmd::kLeft);
            if(op_name[1]=='2')msg=agv::MqttPublishMsg::make_ori(1,agv::OriCmd::kRight);
            if(op_name[1]=='3')msg=agv::MqttPublishMsg::make_ori(1,agv::OriCmd::kStraight);
            if(op_name[1]=='4')msg=agv::MqttPublishMsg::make_ori(1,agv::OriCmd::kARRIVED);
            if(op_name[1]=='5')msg=agv::MqttPublishMsg::make_ori(1,agv::OriCmd::kUTurn);
            break;
        case 'q':
            if(op_name[1]=='1')msg=agv::MqttPublishMsg::make_query(1,agv::QueryCmd::kStatus);
            if(op_name[1]=='2')msg=agv::MqttPublishMsg::make_query(1,agv::QueryCmd::kLog);
            break;
        case 'c':
            if(op_name[1]=='1')msg=agv::MqttPublishMsg::make_action(1,agv::ActionCmd::kPause);
            if(op_name[1]=='2')msg=agv::MqttPublishMsg::make_action(1,agv::ActionCmd::kProcess);
            if(op_name[1]=='3')msg=agv::MqttPublishMsg::make_action(1,agv::ActionCmd::kReboot);
            if(op_name[1]=='4')msg=agv::MqttPublishMsg::make_action(1,agv::ActionCmd::kUturn);
            break;
    }
    if(op_name[2]=='e'||op_name[2]=='E')msg.set_event(true);
    if(op_name[2]=='E')msg.cmd_type=agv::MqttCmdType::TST_POS;
    LOG_INFO(proc_name,"send:%d",int(tx.send(msg,1)));
    return 0;
}