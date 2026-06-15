#include <cstdio>
#include <cstring>
#include <string>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#include "mq_wrapper.h"
#include "logger.h"

#include <fcgi_stdio.h>
#include "fcgi_utils.h"
using namespace agv::http;
const char* proc_name = "task_api";

int main() {
    // 打开 MQ 发送端（进程启动时一次）
    agv::MqSender<agv::TaskDispatchMsg> mq;
    bool mq_ok = false;
    try {
        mq.init(agv::kMqTaskDispatch, agv::kTaskDispatchMsgSize);
        mq_ok = true;
        dprintf(2, "[status] MQ connected");
        //LOG_INFO(proc_name,"MQ connected");
    } catch (const std::exception& e) {
        dprintf(2, "[status] MQ not available: %s\n", e.what());
        //LOG_ERROR(proc_name,"MQ not available: %s",e.what());
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

        // ── /api/assign ──────────────────────────────────────────
        if (uri && strstr(uri, "/api/assign")) {
            
            std::string car_str    = json_str(body, "car");
            std::string target_str = json_str(body, "target");
            dprintf(2, "recv:%s->%s | %s\n", body.c_str(),car_str.c_str(),target_str.c_str());
            if (car_str.empty() || target_str.empty()) {
                reply_err(400, "缺少 car 或 target 字段");
                continue;
            }

            uint8_t  car_id  = car_id_from_str(car_str);
            uint16_t node_id = node_id_from_str(target_str);

            if (car_id == 0xFF) {
                reply_err(400, "car 格式错误，应为 C1/C2...");
                continue;
            }
            if (node_id == 0xFFFF) {
                reply_err(400, "target 格式错误，应为 N1/N2...");
                continue;
            }

            if (mq_ok) {
                auto msg = agv::TaskDispatchMsg::assign(
                    car_id,
                    static_cast<uint8_t>(node_id),
                    agv::ImmeStra::kImme);
                mq.send(msg, agv::kPrioNormal);
            }

            //LOG_INFO(proc_name, "assign car=%s target=%s", car_str.c_str(), target_str.c_str());
            dprintf(2, "[task_api] assign car=%s target=%s\n", car_str.c_str(), target_str.c_str());
            char data[128];
            snprintf(data, sizeof(data),
                     "{\"car\":\"%s\",\"target\":\"%s\"}",
                     car_str.c_str(), target_str.c_str());
            reply_ok("任务已成功指派", data);
        }

        // ── /api/cancel ──────────────────────────────────────────
        else if (uri && strstr(uri, "/api/cancel")) {
            std::string car_str = json_str(body, "car");

            if (car_str.empty()) {
                reply_err(400, "缺少 car 字段");
                continue;
            }

            uint8_t car_id = car_id_from_str(car_str);
            if (car_id == 0xFF) {
                reply_err(400, "car 格式错误");
                continue;
            }

            if (mq_ok) {
                auto msg = agv::TaskDispatchMsg::cancel(car_id);
                mq.send(msg, agv::kPrioHigh);  // 取消任务高优先级
            }
            //LOG_INFO(proc_name, "cancel car=%s", car_str.c_str());
            dprintf(2, "[task_api] cancel car=%s\n", car_str.c_str());
            char data[64];
            snprintf(data, sizeof(data), "{\"car\":\"%s\"}", car_str.c_str());
            reply_ok("任务已成功取消", data);
        }

        else {
            reply_err(404, "接口不存在");
        }
    }
    return 0;
}