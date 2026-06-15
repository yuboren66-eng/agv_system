#pragma once
#include "agv/mq_msg.h"
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <fcntl.h>
#include <mqueue.h>
#include <unistd.h>

namespace agv {

// ─────────────────────────────────────────────────────────────────────────────
// 内部基类：持有 mqd_t，处理 close
// ─────────────────────────────────────────────────────────────────────────────

class MqBase {
public:
    MqBase()  = default;
    ~MqBase() { close(); }

    MqBase(const MqBase&)            = delete;
    MqBase& operator=(const MqBase&) = delete;

    /// 返回 mqd_t，可直接加入 poll/select 的 fd 数组。
    /// Linux 上 mqd_t 本质是 fd，可以 poll。
    mqd_t get_fd() const { return mqd_; }

    bool is_open() const { return mqd_ != static_cast<mqd_t>(-1); }

    void close() {
        if (is_open()) {
            ::mq_close(mqd_);
            mqd_ = static_cast<mqd_t>(-1);
        }
    }

protected:
    mqd_t mqd_ {static_cast<mqd_t>(-1)};

    static std::string errmsg(const char* ctx) {
        return std::string(ctx) + ": " + ::strerror(errno);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// MqReceiver<Msg> — 只读端（消费者进程使用）
// ─────────────────────────────────────────────────────────────────────────────

template <typename Msg>
class MqReceiver : public MqBase {
public:
    /**
     * 打开队列（以只读 + 非阻塞模式）。
     * 若队列不存在则创建（owner 语义：第一个 init 的进程创建队列）。
     *
     * @param name      队列名，如 "/mq_task_dispatch"
     * @param max_msg   队列最大消息条数（创建时有效）
     * @param msg_size  单条消息最大字节数（创建时有效，必须 >= sizeof(Msg)）
     * @throws std::runtime_error
     */
    void init(const char* name,
              long        max_msg  = 16,
              long        msg_size = sizeof(Msg))
    {
        if (msg_size < static_cast<long>(sizeof(Msg))) {
            throw std::runtime_error(
                std::string("MqReceiver::init: msg_size ") +
                std::to_string(msg_size) + " < sizeof(Msg) " +
                std::to_string(sizeof(Msg)));
        }

        struct mq_attr attr{};
        attr.mq_maxmsg  = max_msg;
        attr.mq_msgsize = msg_size;

        // O_CREAT | O_RDONLY | O_NONBLOCK
        //   CREAT：不存在则创建（mode 0666，实际受 umask 影响）
        //   RDONLY：接收端不需要写权限
        //   NONBLOCK：配合 poll，receive 时不阻塞
        mqd_ = ::mq_open(name,
                          O_CREAT | O_RDONLY | O_NONBLOCK,
                          0666, &attr);
        if (!is_open()) {
            throw std::runtime_error(errmsg("mq_open(recv)"));
        }

        name_     = name;
        msg_size_ = msg_size;
    }

    /**
     * 从队列中读取一条消息。
     * 应在 poll 检测到 fd 可读后循环调用，直到返回 false。
     *
     * @param[out] msg   接收到的消息
     * @param[out] prio  消息优先级
     * @return true  = 成功读到一条消息
     *         false = 队列暂时为空（EAGAIN），或出错
     */
    bool receive(Msg& msg, unsigned& prio) {
        ssize_t n = ::mq_receive(mqd_,
                                  reinterpret_cast<char*>(&msg),
                                  msg_size_,
                                  &prio);
        if (n < 0) {
            // EAGAIN：非阻塞模式下队列为空，正常情况
            if (errno == EAGAIN) return false;
            // 其他错误：打印后返回 false，不抛出（主循环不应因此崩溃）
            fprintf(stderr, "[mq] mq_receive(%s): %s\n", name_, strerror(errno));
            return false;
        }
        return true;
    }

    const char* name() const { return name_; }

private:
    const char* name_     {nullptr};
    long        msg_size_ {sizeof(Msg)};
};

// ─────────────────────────────────────────────────────────────────────────────
// MqSender<Msg> — 只写端（生产者进程使用）
// ─────────────────────────────────────────────────────────────────────────────

template <typename Msg>
class MqSender : public MqBase {
public:
    /**
     * 打开队列（以只写 + 非阻塞模式）。
     * 队列必须已由接收端或独立的 setup 程序创建，否则抛出异常。
     *
     * @param name  队列名
     * @throws std::runtime_error
     */
    void init(const char* name, long msg_size = sizeof(Msg)) {
        // O_WRONLY | O_NONBLOCK：发送端不创建队列，只写
        mqd_ = ::mq_open(name, O_WRONLY | O_NONBLOCK);
        if (!is_open()) {
            throw std::runtime_error(errmsg("mq_open(send)"));
        }
        // msg_size 必须由调用方显式传入（mq_getattr 在某些内核/容器环境
        // 对 O_WRONLY 打开的 mqd 返回 msgsize=0，属于内核限制）。
        // 使用 mq_messages.h 中对应的 k*MsgSize 常量即可。
        msg_size_ = msg_size;
        name_     = name;
    }

    /**
     * 发送一条消息。
     *
     * @param msg   要发送的消息（按值拷贝）
     * @param prio  优先级，见 MsgPriority 枚举
     * @return true  = 发送成功
     *         false = 队列已满（EAGAIN），调用方可决定是丢弃还是重试
     * @throws std::runtime_error  消息大小超过队列限制时
     */
    bool send(const Msg& msg, unsigned prio = kPrioNormal) {
        if (static_cast<long>(sizeof(Msg)) > msg_size_) {
            throw std::runtime_error(
                std::string("MqSender::send: sizeof(Msg) ") +
                std::to_string(sizeof(Msg)) +
                " > queue msg_size " + std::to_string(msg_size_));
        }

        int ret = ::mq_send(mqd_,
                             reinterpret_cast<const char*>(&msg),
                             sizeof(Msg),
                             prio);
        if (ret < 0) {
            if (errno == EAGAIN) {
                fprintf(stderr, "[mq] mq_send(%s): queue full, message dropped\n", name_);
                return false;
            }
            fprintf(stderr, "[mq] mq_send(%s): %s\n", name_, strerror(errno));
            return false;
        }
        return true;
    }

    const char* name() const { return name_; }

private:
    const char* name_     {nullptr};
    long        msg_size_ {sizeof(Msg)};
};

// ─────────────────────────────────────────────────────────────────────────────
// MqOwner — 负责创建和销毁队列（每条队列只需一个 owner）
// ─────────────────────────────────────────────────────────────────────────────

/**
 * 在进程启动时创建队列，在进程退出时 unlink。
 * 通常由 model_manager（进程 A）持有，保证队列生命周期与系统一致。
 *
 * 用法：
 *   agv::MqOwner owner;
 *   owner.create(agv::kMqTaskDispatch, agv::kTaskDispatchMaxMsg,
 *                agv::kTaskDispatchMsgSize);
 *   // ... 系统运行 ...
 *   owner.unlink_all();  // 或在析构时自动 unlink
 */
class MqOwner {
public:
    struct QueueEntry {
        char name[64];
    };

    ~MqOwner() { unlink_all(); }

    /**
     * 创建一条队列（仅创建，不持有 mqd_t，open 由各进程自己做）。
     * 若队列已存在则先删除再创建，保证参数正确（重启场景）。
     */
    void create(const char* name, long max_msg, long msg_size) {
        // 先 unlink 避免上次异常退出残留的队列参数不一致
        ::mq_unlink(name);

        struct mq_attr attr{};
        attr.mq_maxmsg  = max_msg;
        attr.mq_msgsize = msg_size;

        mqd_t mqd = ::mq_open(name, O_CREAT | O_RDWR, 0666, &attr);
        if (mqd == static_cast<mqd_t>(-1)) {
            throw std::runtime_error(
                std::string("MqOwner::create(") + name + "): " + strerror(errno));
        }
        ::mq_close(mqd);  // 立即关闭，各进程自行 open

        // 记录名称，析构时 unlink
        QueueEntry e{};
        std::strncpy(e.name, name, sizeof(e.name) - 1);
        queues_.push_back(e);

        fprintf(stderr, "[mq_owner] created %s (maxmsg=%ld msgsize=%ld)\n",
                name, max_msg, msg_size);
    }

    /// 创建系统所需的全部三条队列（model_manager 启动时调用）
    void create_all() {
        create(kMqTaskDispatch, kTaskDispatchMaxMsg, kTaskDispatchMsgSize);
        create(kMqMqttPublish,  kMqttPublishMaxMsg,  kMqttPublishMsgSize);
        create(kMqRouteExert,  kRouteExertMaxMsg,  kRouteExertMsgSize);
    }

    void unlink_all() {
        for (auto& e : queues_) {
            if (e.name[0] != '\0') {
                ::mq_unlink(e.name);
                fprintf(stderr, "[mq_owner] unlinked %s\n", e.name);
                e.name[0] = '\0';
            }
        }
        queues_.clear();
    }

private:
    // 不用 std::string 避免额外依赖
    std::vector<QueueEntry> queues_;
};

} // namespace agv

/**
 * mq_wrapper.h — POSIX MQ 封装
 *
 * 设计原则：
 *   - 用模板参数绑定消息类型，send/receive 直接操作结构体，
 *     无需调用方手动转型或记忆 msg_size。
 *   - 以 O_NONBLOCK 模式打开，配合 poll() 主循环使用，
 *     不在 receive() 内部阻塞。
 *   - 构造时不做系统调用，init() 时才打开队列，
 *     和 SignalHandler 保持一致的初始化风格。
 *   - 析构自动 mq_close，不负责 mq_unlink（只有 owner 进程负责删除队列）。
 *
 * 用法（接收端，加入 poll 循环）：
 *
 *   agv::MqReceiver<agv::TaskDispatchMsg> mq_task;
 *   mq_task.init(agv::kMqTaskDispatch, agv::kTaskDispatchMaxMsg,
 *                agv::kTaskDispatchMsgSize);
 *
 *   // 加入 poll
 *   fds[FD_MQ].fd     = mq_task.get_fd();
 *   fds[FD_MQ].events = POLLIN;
 *
 *   // poll 触发后
 *   agv::TaskDispatchMsg msg;
 *   unsigned prio;
 *   while (mq_task.receive(msg, prio)) {
 *       handle(msg);
 *   }
 *
 * 用法（发送端）：
 *
 *   agv::MqSender<agv::TaskDispatchMsg> mq_pub;
 *   mq_pub.init(agv::kMqTaskDispatch);
 *
 *   auto msg = agv::TaskDispatchMsg::assign(car_id, target);
 *   mq_pub.send(msg, agv::kPrioNormal);
 */