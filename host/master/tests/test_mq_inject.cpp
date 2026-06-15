#include <cstdio>
#include <cstring>
#include <cstdlib>

//请确保log_daemon和model_manager正常运行
#include "mq_wrapper.h"
#include "logger.h"

const char* proc_name = "mq-inject";

static void usage(const char* prog) {
    fprintf(stderr,
        "用法: %s <队列> <类型> [参数...]\n"
        "\n"
        "队列:\n"
        "  task       /mq_task_dispatch\n"
        "  route      /mq_route_exert\n"
        "  mqtt       /mq_mqtt_publish\n"
        "\n"
        "task 类型:\n"
        "  assign <car_id> <target_node> [imme]\n"
        "  cancel <car_id> [imme]\n"
        "  replan <car_id> [imme]\n"
        "     imme: 0=kNormal(默认)  1=kImme  2=kUturn\n"
        "\n"
        "route 类型:\n"
        "  apply <car_id> [arrived_node] [last_node]\n"
        "     默认 arrived_node=0xFFFF last_node=0xFFFF\n"
        "\n"
        "mqtt 类型: (请用 test_mq_mqtt 工具)\n"
        "  o1=左转 o2=右转 o3=直行 o4=到达\n"
        "  a1=角度12 a2=角度312\n"
        "  c1=暂停 c2=继续 c3=重启 c4=掉头\n"
        "  q1=查询状态 q2=查询日志\n"
        "\n",
        prog);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        usage(argv[0]);
        return 1;
    }

    const char* queue = argv[1];
    const char* type  = argv[2];

    // ── TaskDispatch ────────────────────────────────────────────────
    if (strcmp(queue, "task") == 0) {
        agv::MqSender<agv::TaskDispatchMsg> tx;
        try {
            tx.init(agv::kMqTaskDispatch, agv::kTaskDispatchMsgSize);
        } catch (const std::exception& e) {
            fprintf(stderr, "[mq-inject] FATAL: %s\n", e.what());
            return 1;
        }

        agv::ImmeStra imm = agv::ImmeStra::kNormal;
        if (argc >= 5) {
            int v = atoi(argv[4]);
            if (v == 1) imm = agv::ImmeStra::kImme;
            else if (v == 2) imm = agv::ImmeStra::kUturn;
        }

        agv::TaskDispatchMsg msg;
        if (strcmp(type, "assign") == 0 && argc >= 4) {
            uint8_t car   = (uint8_t)atoi(argv[3]);
            uint8_t node  = (argc >= 5) ? (uint8_t)atoi(argv[4]) : 0;
            // 如果 imme 是数字参数，需要识别：第4个参数是node还是imme
            // 简化处理：assign需要3个参数 car, node, [imme]
            // 所以 car=argv[3], node=argv[4], imme=argv[5]
            imm = agv::ImmeStra::kNormal;
            if (argc >= 6) {
                int v = atoi(argv[5]);
                if (v == 1) imm = agv::ImmeStra::kImme;
                else if (v == 2) imm = agv::ImmeStra::kUturn;
            }
            msg = agv::TaskDispatchMsg::assign(car, node, imm);
            LOG_INFO(proc_name, "task assign car=%u node=%u imm=%d", car, node, (int)imm);
        } else if (strcmp(type, "cancel") == 0 && argc >= 4) {
            msg = agv::TaskDispatchMsg::cancel((uint8_t)atoi(argv[3]), imm);
            LOG_INFO(proc_name, "task cancel car=%u", (uint8_t)atoi(argv[3]));
        } else if (strcmp(type, "replan") == 0 && argc >= 4) {
            msg = agv::TaskDispatchMsg::replan((uint8_t)atoi(argv[3]), imm);
            LOG_INFO(proc_name, "task replan car=%u", (uint8_t)atoi(argv[3]));
        } else {
            fprintf(stderr, "[mq-inject] 无效的 task 参数\n");
            return 1;
        }

        int ret = (int)tx.send(msg, agv::kPrioNormal);
        LOG_INFO(proc_name, "send result=%d", ret);
        return ret == 0 ? 0 : 1;
    }

    // ── RouteExert ──────────────────────────────────────────────────
    else if (strcmp(queue, "route") == 0) {
        agv::MqSender<agv::RouteExertMsg> tx;
        try {
            tx.init(agv::kMqRouteExert, agv::kRouteExertMsgSize);
        } catch (const std::exception& e) {
            fprintf(stderr, "[mq-inject] FATAL: %s\n", e.what());
            return 1;
        }

        if (strcmp(type, "apply") == 0 && argc >= 4) {
            uint8_t  car    = (uint8_t)atoi(argv[3]);
            uint16_t arr    = (argc >= 5) ? (uint16_t)atoi(argv[4]) : 0xFFFF;
            uint16_t last   = (argc >= 6) ? (uint16_t)atoi(argv[5]) : 0xFFFF;
            auto msg = agv::RouteExertMsg::apply(car, arr, last);
            LOG_INFO(proc_name, "route apply car=%u arrived=%u last=%u", car, arr, last);
            int ret = (int)tx.send(msg, agv::kPrioNormal);
            LOG_INFO(proc_name, "send result=%d", ret);
            return ret == 0 ? 0 : 1;
        } else {
            fprintf(stderr, "[mq-inject] 无效的 route 参数\n");
            return 1;
        }
    }

    // ── MqttPublish（复用 test_mq_mqtt 的功能）─────────────────────
    else if (strcmp(queue, "mqtt") == 0) {
        agv::MqSender<agv::MqttPublishMsg> tx;
        try {
            tx.init(agv::kMqMqttPublish, agv::kMqttPublishMsgSize);
        } catch (const std::exception& e) {
            fprintf(stderr, "[mq-inject] FATAL: %s\n", e.what());
            return 1;
        }

        agv::MqttPublishMsg msg = agv::MqttPublishMsg::make_action(1, agv::ActionCmd::kPause);

        switch (type[0]) {
            case 'a':  // angle
                if (type[1] == '1') msg = agv::MqttPublishMsg::make_angle(1, 12);
                if (type[1] == '2') msg = agv::MqttPublishMsg::make_angle(1, 312);
                break;
            case 'o':  // ori
                if (type[1] == '1') msg = agv::MqttPublishMsg::make_ori(1, agv::OriCmd::kLeft);
                if (type[1] == '2') msg = agv::MqttPublishMsg::make_ori(1, agv::OriCmd::kRight);
                if (type[1] == '3') msg = agv::MqttPublishMsg::make_ori(1, agv::OriCmd::kStraight);
                if (type[1] == '4') msg = agv::MqttPublishMsg::make_ori(1, agv::OriCmd::kARRIVED);
                break;
            case 'q':  // query
                if (type[1] == '1') msg = agv::MqttPublishMsg::make_query(1, agv::QueryCmd::kStatus);
                if (type[1] == '2') msg = agv::MqttPublishMsg::make_query(1, agv::QueryCmd::kLog);
                break;
            case 'c':  // control
                if (type[1] == '1') msg = agv::MqttPublishMsg::make_action(1, agv::ActionCmd::kPause);
                if (type[1] == '2') msg = agv::MqttPublishMsg::make_action(1, agv::ActionCmd::kProcess);
                if (type[1] == '3') msg = agv::MqttPublishMsg::make_action(1, agv::ActionCmd::kReboot);
                if (type[1] == '4') msg = agv::MqttPublishMsg::make_action(1, agv::ActionCmd::kUturn);
                break;
            default:
                fprintf(stderr, "[mq-inject] 无效的 mqtt 类型 '%s'\n", type);
                return 1;
        }

        int ret = (int)tx.send(msg, agv::kPrioNormal);
        LOG_INFO(proc_name, "send mqtt result=%d", ret);
        return ret == 0 ? 0 : 1;
    }

    else {
        fprintf(stderr, "[mq-inject] 未知队列 '%s'\n", queue);
        usage(argv[0]);
        return 1;
    }
}
