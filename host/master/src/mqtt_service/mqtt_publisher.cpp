#define AGV_NO_SYSTEMD
#include "config/config.h"

#include "mqtt_client.h"
#include "signal_handler.h"
#include "secure_exit.h"
#include "mq_wrapper.h"
#include "config/config.h"
#include "logger.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <poll.h>
#include <unistd.h>

static const char* PROC_NAME = "mqtt_publisher";

class PublisherClient : public agv::MqttClientBase {
public:
    bool publish(const agv::MqttPublishMsg& msg) {
        if (!is_connected()) {
            LOG_ERROR(PROC_NAME,"car%u not connected, drop cmd %u",
                    msg.car_id,static_cast<uint8_t>(msg.cmd_type));
            return false;
        }
        char topic  [128] = {};
        char payload[512] = {};
        msg.to_mqtt(topic, sizeof(topic), payload, sizeof(payload));

        int rc = mosquitto_publish(
            mosq(),
            /*mid=*/nullptr,
            topic,
            static_cast<int>(strlen(payload)),
            payload,
            msg.qos,
            /*retain=*/false);

        if (rc != MOSQ_ERR_SUCCESS) {
            LOG_ERROR(PROC_NAME,"publish failed topic=%s rc=%d(%s)",
                    topic, rc, mosquitto_strerror(rc));
            return false;
        }
        LOG_INFO(PROC_NAME,"mqtt suc-publish->%s %s",topic, payload);
        return true;
    }

protected:
    // 连接成功后订阅回显 topic（调试用）
    void on_connect(int rc) override {
        if (rc != 0) return;
        #ifdef _AGV_PRINT_PRINT
        mosquitto_subscribe(mosq(), nullptr, "car/#", 0);
        LOG_DEBUG(PROC_NAME,"subscribed all car topic to debug");
        #endif
    }
    void on_message(const mosquitto_message* msg) override {
        #ifdef _AGV_PRINT_PRINT
        mosquitto_subscribe(mosq(), nullptr, "car/#", 0);
        LOG_DEBUG(PROC_NAME,"recv:topic=%-24s  payload=%.*s",
                msg->topic,
                msg->payloadlen,
                static_cast<const char*>(msg->payload));
        #endif
    }
};


int main() {
    const char* host = AGV_MQTT_HOST;
    int         port = AGV_MQTT_PORT;
    LOG_INFO(PROC_NAME,"begin, broker=%s:%d", host, port);

    agv::SignalHandler sig(PROC_NAME);
    try { sig.init(); }
    catch (const std::exception& e) {
        LOG_FATAL(PROC_NAME,"%s",e.what());
        return 1;
    }

    PublisherClient mqtt;
    try {
        mqtt.init("agv-publisher");
        mqtt.connect(host, port);
    } catch (const std::exception& e) {
        LOG_FATAL(PROC_NAME,"mqtt:%s",e.what());
        return 1;
    }

    //打开MQ接收端
    agv::MqReceiver<agv::MqttPublishMsg> mq;
    try {
        mq.init(agv::kMqMqttPublish, 10, agv::kMqttPublishMsgSize);
    } catch (const std::exception& e) {
        LOG_FATAL(PROC_NAME,"mq:%s",e.what());
        return 1;
    }

    //退出清理
    agv::SecureExit exit_seq(PROC_NAME);
    exit_seq.add_cleanup("mq_close", [&] { mq.close(); });

    // poll 主循环
    enum { FD_SIG = 0, FD_MQ = 1, FD_COUNT };
    struct pollfd fds[FD_COUNT];
    fds[FD_SIG].fd = sig.get_fd(); fds[FD_SIG].events = POLLIN;
    fds[FD_MQ ].fd = mq.get_fd(); fds[FD_MQ ].events = POLLIN;
    LOG_INFO(PROC_NAME,"ready,waiting for cmds on:%s",agv::kMqMqttPublish);
    while (!sig.shutdown_requested()) {
        int ret = ::poll(fds, FD_COUNT, -1);
        if (ret < 0) {
            if (errno == EINTR) continue;
            LOG_ERROR(PROC_NAME, "poll error: %s\n",strerror(errno));
            break;
        }
        if (fds[FD_SIG].revents & POLLIN) {
            sig.handle_read();
            continue;
        }

        if (fds[FD_MQ].revents & POLLIN) {
            agv::MqttPublishMsg msg{};
            unsigned prio = 0;
            while (mq.receive(msg, prio)) {
                mqtt.publish(msg);
            }
        }
    }
    LOG_INFO(PROC_NAME, "shutting down\n");
    exit_seq.run(100);
}
