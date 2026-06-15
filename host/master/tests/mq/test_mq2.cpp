/**
 * experiment_mq.cpp — POSIX MQ 封装实验程序
 *
 * 演示三条队列的完整生命周期：
 *
 *   进程角色模拟（单进程内用 fork 分出子进程）：
 *
 *   父进程（owner + 生产者）
 *     1. MqOwner 创建三条队列
 *     2. 用 MqSender 向 mq_task_dispatch 发 3 条任务
 *     3. 用 MqSender 向 mq_mqtt_publish  发 2 条命令
 *     4. 等待子进程退出
 *     5. MqOwner 析构自动 unlink 所有队列
 *
 *   子进程（消费者，用 poll 驱动）
 *     1. 用 MqReceiver 打开三条队列
 *     2. poll 循环接收，打印收到的消息
 *     3. 收到 3 条 task + 2 条 mqtt 后主动退出
 *
 * 编译：
 *   g++ -std=c++17 -I.. -I../lib \
 *       -o experiment_mq experiment_mq.cpp \
 *       -lrt -lpthread
 *
 * 运行：
 *   ./experiment_mq
 *
 * 预期输出（顺序可能因优先级不同而变化）：
 *   [owner]  created /mq_task_dispatch ...
 *   [owner]  created /mq_mqtt_publish  ...
 *   [owner]  created /mq_log           ...
 *   [sender] sent TaskDispatch: assign  car=1 -> node=5
 *   [sender] sent TaskDispatch: assign  car=2 -> node=8
 *   [sender] sent TaskDispatch: cancel  car=1
 *   [sender] sent MqttPublish:  move    car=1 node=5
 *   [sender] sent MqttPublish:  stop    car=2
 *   [recv]   TaskDispatch: action=0(assign)  car=1 target=5
 *   [recv]   TaskDispatch: action=0(assign)  car=2 target=8
 *   [recv]   TaskDispatch: action=1(cancel)  car=1
 *   [recv]   MqttPublish:  cmd=0(move) car=1 node=5  topic=car/1/cmd
 *   [recv]   MqttPublish:  cmd=1(stop) car=2         topic=car/2/cmd
 *   [child]  all messages received, exiting
 *   [owner]  unlinked /mq_task_dispatch
 *   ...
 */

#include "mq_wrapper.h"
#include "../lib/ipc/signal_handler.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

// ─── 子进程：消费者 ────────────────────────────────────────────────────────

static void run_consumer() {
    // 等父进程把消息发完（简单 sleep，实际系统由 poll 自然阻塞等待）
    ::usleep(50 * 1000);  // 50ms

    // 打开三条队列的接收端
    agv::MqReceiver<agv::TaskDispatchMsg> rx_task;
    agv::MqReceiver<agv::MqttPublishMsg>  rx_mqtt;

    try {
        rx_task.init(agv::kMqTaskDispatch, agv::kTaskDispatchMaxMsg,
                     agv::kTaskDispatchMsgSize);
        rx_mqtt.init(agv::kMqMqttPublish,  agv::kMqttPublishMaxMsg,
                     agv::kMqttPublishMsgSize);
    } catch (const std::exception& e) {
        fprintf(stderr, "[child] FATAL: %s\n", e.what());
        ::exit(1);
    }

    // poll 数组
    enum { FD_TASK = 0, FD_MQTT = 1, FD_COUNT };
    struct pollfd fds[FD_COUNT];
    fds[FD_TASK].fd = rx_task.get_fd(); fds[FD_TASK].events = POLLIN;
    fds[FD_MQTT].fd = rx_mqtt.get_fd(); fds[FD_MQTT].events = POLLIN;

    int task_count = 0, mqtt_count = 0;
    const int kExpectTask = 3, kExpectMqtt = 2;

    fprintf(stderr, "[child]  poll loop started, expecting %d task + %d mqtt messages\n",
            kExpectTask, kExpectMqtt);

    while (task_count < kExpectTask || mqtt_count < kExpectMqtt) {
        int ret = ::poll(fds, FD_COUNT, 2000);  // 2s 超时，防止卡死
        if (ret <= 0) {
            fprintf(stderr, "[child]  poll timeout or error, giving up\n");
            break;
        }

        // 读 task 队列
        if (fds[FD_TASK].revents & POLLIN) {
            agv::TaskDispatchMsg msg{};
            unsigned prio = 0;
            while (rx_task.receive(msg, prio)) {
                ++task_count;
                const char* act_str =
                    msg.action == agv::TaskAction::kAssign ? "assign" :
                    msg.action == agv::TaskAction::kCancel ? "cancel" : "replan";
                fprintf(stderr,
                    "[recv]   TaskDispatch [prio=%u] action=%d(%s) car=%u target=%u\n",
                    prio, (int)msg.action, act_str, msg.car_id, msg.target_node);
            }
        }

        // 读 mqtt 队列
        if (fds[FD_MQTT].revents & POLLIN) {
            char topic[128], payload[256];
            agv::MqttPublishMsg msg{};
            unsigned prio = 0;
            while (rx_mqtt.receive(msg, prio)) {
                ++mqtt_count;
                const char* cmd_str =
                    msg.cmd_type == agv::MqttCmdType::CMD_angle ? "CMD_angle" :
                    msg.cmd_type == agv::MqttCmdType::QUERY ? "QUERY" : "others";
                msg.to_mqtt(topic, sizeof(topic), payload, sizeof(payload));
                fprintf(stderr,
                    "[recv]   MqttPublish  [prio=%u] cmd=%d(%s) car=%u node=%u topic=%s info%s\n",
                    prio, (int)msg.cmd_type, cmd_str,
                    msg.car_id, msg.next_node, topic,payload);
            }
        }
    }

    fprintf(stderr, "[child]  all messages received (task=%d mqtt=%d), exiting\n",
            task_count, mqtt_count);
    ::exit(0);
}

// ─── 父进程：owner + 生产者 ───────────────────────────────────────────────

int main() {
    fprintf(stderr, "=== experiment_mq ===\n\n");

    // Step 1: 创建三条队列
    agv::MqOwner owner;
    try {
        owner.create(agv::kMqTaskDispatch, 10, agv::kTaskDispatchMsgSize);
        owner.create(agv::kMqMqttPublish,  10, agv::kMqttPublishMsgSize);
    } catch (const std::exception& e) {
        fprintf(stderr, "[main] FATAL: %s\n", e.what());
        return 1;
    }
    fprintf(stderr, "\n");

    // Step 2: fork 出消费者子进程
    pid_t child = ::fork();
    if (child < 0) {
        fprintf(stderr, "[main] fork failed: %s\n", strerror(errno));
        return 1;
    }
    if (child == 0) {
        run_consumer();  // 子进程不返回
    }

    // ── 父进程：发送消息 ────────────────────────────────────────────────

    // 稍等子进程打开队列
    ::usleep(20 * 1000);  // 20ms

    // 打开发送端
    agv::MqSender<agv::TaskDispatchMsg> tx_task;
    agv::MqSender<agv::MqttPublishMsg>  tx_mqtt;

    try {
        tx_task.init(agv::kMqTaskDispatch, agv::kTaskDispatchMsgSize);
        tx_mqtt.init(agv::kMqMqttPublish,  agv::kMqttPublishMsgSize);
    } catch (const std::exception& e) {
        fprintf(stderr, "[main] FATAL sender init: %s\n", e.what());
        ::kill(child, SIGTERM);
        return 1;
    }

    // 发 task 消息
    {
        auto m = agv::TaskDispatchMsg::assign(1, 5, agv::ImmeStra::kImme);
        tx_task.send(m, agv::kPrioNormal);
        fprintf(stderr, "[sender] sent TaskDispatch: assign  car=1 -> node=5\n");
    }
    {
        auto m = agv::TaskDispatchMsg::assign(2, 8, agv::ImmeStra::kImme);
        tx_task.send(m, agv::kPrioNormal);
        fprintf(stderr, "[sender] sent TaskDispatch: assign  car=2 -> node=8\n");
    }
    {
        auto m = agv::TaskDispatchMsg::cancel(1);
        tx_task.send(m, agv::kPrioHigh);    // 取消任务用高优先级
        fprintf(stderr, "[sender] sent TaskDispatch: cancel  car=1  [high prio]\n");
    }

    // 发 mqtt 命令
    {
        auto m = agv::MqttPublishMsg::make_angle(1, 5);
        tx_mqtt.send(m, agv::kPrioNormal);
        fprintf(stderr, "[sender] sent MqttPublish:  angle-move    car=1 angle=5\n");
    }
    {
        auto m = agv::MqttPublishMsg::make_query(2,agv::QueryCmd::kLog);
        tx_mqtt.send(m, agv::kPrioNormal);
        fprintf(stderr, "[sender] sent MqttPublish:  stop    car=2\n");
    }

    fprintf(stderr, "\n");

    // 等子进程退出
    int status = 0;
    ::waitpid(child, &status, 0);

    fprintf(stderr, "\n[main]   child exited with status %d\n", WEXITSTATUS(status));
    fprintf(stderr, "[main]   MqOwner destructor will now unlink all queues\n\n");

    // owner 析构时自动 unlink
    return WEXITSTATUS(status);
}