#pragma once
#include <mosquitto.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>


namespace agv {

class MqttClientBase {
public:
    MqttClientBase()  = default;
    virtual ~MqttClientBase() { cleanup(); }

    MqttClientBase(const MqttClientBase&)            = delete;
    MqttClientBase& operator=(const MqttClientBase&) = delete;

    /**
     * 初始化 mosquitto 库和实例。
     * @param client_id  MQTT 客户端 ID，同一 broker 内必须唯一
     */
    void init(const char* client_id) {
        mosquitto_lib_init();

        mosq_ = mosquitto_new(client_id, /*clean_session=*/true, this);
        if (!mosq_) {
            throw std::runtime_error(
                std::string("mosquitto_new failed: ") + strerror(errno));
        }

        // 注册静态回调，转发到虚函数
        mosquitto_connect_callback_set(mosq_,    s_on_connect);
        mosquitto_disconnect_callback_set(mosq_, s_on_disconnect);
        mosquitto_message_callback_set(mosq_,    s_on_message);
        mosquitto_publish_callback_set(mosq_,    s_on_publish);
        mosquitto_log_callback_set(mosq_,        s_on_log);

        client_id_ = client_id;
    }

    /**
     * 连接 broker 并启动后台网络线程。
     * 连接是异步的，实际建立后会触发 on_connect 回调。
     * @param host      broker 地址（IP 或 hostname）
     * @param port      broker 端口，默认 1883
     * @param keepalive 心跳间隔（秒）
     */
    void connect(const char* host, int port = 1883, int keepalive = 60) {
        host_      = host;
        port_      = port;
        keepalive_ = keepalive;

        int rc = mosquitto_connect(mosq_, host, port, keepalive);
        if (rc != MOSQ_ERR_SUCCESS) {
            throw std::runtime_error(
                std::string("mosquitto_connect to ") + host + ":" +
                std::to_string(port) + " failed: " +
                mosquitto_strerror(rc));
        }

        // loop_start 启动后台线程处理网络 I/O，不阻塞主线程
        rc = mosquitto_loop_start(mosq_);
        if (rc != MOSQ_ERR_SUCCESS) {
            throw std::runtime_error(
                std::string("mosquitto_loop_start failed: ") +
                mosquitto_strerror(rc));
        }

        loop_started_ = true;
        fprintf(stderr, "[%s] connecting to %s:%d\n",
                client_id_.c_str(), host, port);
    }

    bool is_connected() const {
        return connected_.load(std::memory_order_acquire);
    }

    const std::string& client_id() const { return client_id_; }

protected:
    /// 连接成功，rc==0 表示正常，子类在此订阅 topic
    virtual void on_connect(int rc) { (void)rc; }

    /// 断线时调用，已在内部自动重连，子类可在此打日志
    virtual void on_disconnect(int rc) { (void)rc; }

    /// 收到消息，子类实现业务逻辑
    virtual void on_message(const mosquitto_message* msg) { (void)msg; }

    /// 发布完成确认（QoS 1/2 时有意义）
    virtual void on_publish(int mid) { (void)mid; }

    /// 内部日志，默认只转发 WARNING 以上
    virtual void on_log(int level, const char* str) {
        if (level <= MOSQ_LOG_WARNING) {
            fprintf(stderr, "[%s][mqtt] %s\n", client_id_.c_str(), str);
        }
    }

    /// 访问原始 mosquitto 指针（用于 subscribe / publish）
    mosquitto* mosq() { return mosq_; }

private:
    // ── 静态回调：从 userdata 取出 this，转发虚函数 ──────────────────────────

    static void s_on_connect(mosquitto*, void* ud, int rc) {
        auto* self = static_cast<MqttClientBase*>(ud);
        if (rc == 0) {
            self->connected_.store(true, std::memory_order_release);
            fprintf(stderr, "[%s] connected to %s:%d\n",
                    self->client_id_.c_str(),
                    self->host_.c_str(), self->port_);
        } else {
            fprintf(stderr, "[%s] connect failed rc=%d (%s)\n",
                    self->client_id_.c_str(), rc, mosquitto_connack_string(rc));
        }
        self->on_connect(rc);
    }

    static void s_on_disconnect(mosquitto*, void* ud, int rc) {
        auto* self = static_cast<MqttClientBase*>(ud);
        self->connected_.store(false, std::memory_order_release);
        fprintf(stderr, "[%s] disconnected rc=%d%s\n",
                self->client_id_.c_str(), rc,
                rc == 0 ? " (clean)" : " (unexpected, will reconnect)");
        self->on_disconnect(rc);

        // 非主动断开时自动重连
        if (rc != 0 && self->mosq_) {
            mosquitto_reconnect_async(self->mosq_);
        }
    }

    static void s_on_message(mosquitto*, void* ud,
                              const mosquitto_message* msg) {
        static_cast<MqttClientBase*>(ud)->on_message(msg);
    }

    static void s_on_publish(mosquitto*, void* ud, int mid) {
        static_cast<MqttClientBase*>(ud)->on_publish(mid);
    }

    static void s_on_log(mosquitto*, void* ud, int level, const char* str) {
        static_cast<MqttClientBase*>(ud)->on_log(level, str);
    }

    void cleanup() {
        if (mosq_) {
            if (loop_started_) {
                mosquitto_loop_stop(mosq_, /*force=*/false);
            }
            mosquitto_disconnect(mosq_);
            mosquitto_destroy(mosq_);
            mosq_ = nullptr;
        }
        mosquitto_lib_cleanup();
    }

    mosquitto*         mosq_         {nullptr};
    std::atomic<bool>  connected_    {false};
    bool               loop_started_ {false};

    std::string client_id_;
    std::string host_;
    int         port_      {1883};
    int         keepalive_ {60};
};

} // namespace agv