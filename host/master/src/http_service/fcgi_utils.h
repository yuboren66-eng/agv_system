#pragma once

/**
 * fcgi_util.h — FastCGI 接口公共工具
 *
 * 提供：
 *   1. read_body()        — 读取 POST body（从 stdin）
 *   2. json_field()       — 极简 JSON 字段提取
 *   3. json_array_items() — 提取 JSON 数组元素（如 ["L2","L8"]）
 *   4. reply_json()       — 写标准响应头 + JSON body
 *   5. reply_ok()         — 200 成功响应
 *   6. reply_err()        — 错误响应
 *   7. car_id_from_str()  — "C1" → 数字 1
 *   8. node_id_from_str() — "N4" → 数字 4
 *   9. edge_id_from_str() — "L8" → 数字 8
 *
 * 注意：所有函数都在 FCGI_Accept 的请求上下文内调用，
 *       使用 printf/fread 即可（libfcgi 已重定向 stdio）。
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <string>
#include <vector>
// nlohmann/json 需要在 fcgi_stdio.h 之前 include，避免 FILE 宏污染
// 使用本头文件的 cpp 文件必须先 include <nlohmann/json.hpp>
// 参见 Status.cpp / Topo.cpp / Task.cpp 的 include 顺序

namespace agv::http {

// ─────────────────────────────────────────────────────────────────────────────
// HTTP / FastCGI 工具
// ─────────────────────────────────────────────────────────────────────────────

/// 读取 POST body，最多 max_len 字节
inline std::string read_body(size_t max_len = 4096) {
    const char* len_str = getenv("CONTENT_LENGTH");
    if (!len_str) return {};

    size_t content_len = static_cast<size_t>(atoi(len_str));
    if (content_len == 0 || content_len > max_len) return {};

    std::string body(content_len, '\0');
    size_t n = FCGI_fread(&body[0], 1, content_len, FCGI_stdin);
    body.resize(n);
    return body;
}

/// 返回当前时间字符串，格式 "2026-04-18 14:30:25"
inline std::string now_str() {
    time_t t = time(nullptr);
    struct tm tm_buf{};
    localtime_r(&t, &tm_buf);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
    return buf;
}

/// 写 HTTP 响应（带 CORS 头，方便前端开发调试）
inline void reply_json(int status_code, const char* body) {
    const char* status_text = (status_code == 200) ? "OK" : "Not Found";
    printf("Status: %d %s\r\n"
           "Content-Type: application/json; charset=utf-8\r\n"
           "Access-Control-Allow-Origin: *\r\n"
           "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
           "Access-Control-Allow-Headers: Content-Type\r\n"
           "\r\n"
           "%s", status_code, status_text, body);
}

/// 标准成功响应
inline void reply_ok(const char* msg, const char* data_json) {
    char buf[4096];
    snprintf(buf, sizeof(buf),
             "{\"code\":200,\"msg\":\"%s\",\"data\":%s}",
             msg, data_json ? data_json : "null");
    reply_json(200, buf);
}

/// 标准错误响应
inline void reply_err(int code, const char* msg) {
    char buf[512];
    snprintf(buf, sizeof(buf),
             "{\"code\":%d,\"msg\":\"%s\",\"data\":null}",
             code, msg);
    reply_json(code, buf);
}

/// 处理 OPTIONS 预检请求（CORS）
inline bool handle_preflight() {
    const char* method = getenv("REQUEST_METHOD");
    if (method && strcmp(method, "OPTIONS") == 0) {
        printf("Status: 204 No Content\r\n"
               "Access-Control-Allow-Origin: *\r\n"
               "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
               "Access-Control-Allow-Headers: Content-Type\r\n"
               "\r\n");
        return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// 极简 JSON 解析（无依赖，只处理文档中出现的扁平格式）
// ─────────────────────────────────────────────────────────────────────────────

/**
 * 从 JSON 字符串中提取 key 对应的字符串值（不含引号）。
 * 仅支持 "key":"value" 格式。
 * 找不到返回空字符串。
 */
 /*
inline std::string json_str(const std::string& json, const char* key) {
    // 构造搜索模式 "key":"
    std::string needle = std::string("\"") + key + "\":\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return {};

    size_t start = pos + needle.size();
    size_t end   = json.find('"', start);
    if (end == std::string::npos) return {};
    return json.substr(start, end - start);
}
*/
inline std::string json_str(const std::string& json_str, const char* key) {
    auto j = json::parse(json_str);
    if (j.contains(key) && j[key].is_string()) 
        return j[key].get<std::string>();
    return {};
}
/**
 * 从 JSON 中提取数组字段的所有字符串元素。
 * 支持 "key":["L2","L8"] 格式。
 * 返回元素列表（不含引号）。
 */
 /*
inline std::vector<std::string> json_str_array(const std::string& json,
                                                const char* key) {
    std::vector<std::string> result;

    try {
        // 解析 JSON 字符串
        auto j = nlohmann::json::parse(json);

        // 检查 key 是否存在且值为数组
        if (j.contains(key) && j[key].is_array()) {
            const auto& arr = j[key];
            for (const auto& elem : arr) {
                if (elem.is_string()) {
                    result.push_back(elem.get<std::string>());
                }
                // 如果数组中包含非字符串元素，按原函数行为忽略它们
            }
        }
        // 否则返回空 result
    } catch (const nlohmann::json::parse_error&) {
        // 解析失败时返回空向量（与原实现行为一致）
    }

    return result;
}*/

inline std::vector<std::string> json_str_array(const std::string& json_str,
                                                const char* key) {
    std::vector<std::string> result;

    try {
        // 解析 JSON 字符串
        auto j = json::parse(json_str);

        // 检查 key 是否存在且值为数组
        if (j.contains(key) && j[key].is_array()) {
            const auto& arr = j[key];
            for (const auto& elem : arr) {
                if (elem.is_string()) {
                    result.push_back(elem.get<std::string>());
                }
                // 如果数组中包含非字符串元素，按原函数行为忽略它们
            }
        }
        // 否则返回空 result
    } catch (const json::parse_error&) {
        // 解析失败时返回空向量（与原实现行为一致）
    }

    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// ID 字符串 ↔ 数字映射
// ─────────────────────────────────────────────────────────────────────────────

/**
 * "C1" → 1，"C12" → 12，失败返回 0xFF
 * 前缀必须是 'C'
 */
inline uint8_t car_id_from_str(const std::string& s) {
    if (s.empty() || s[0] != 'C') return 0xFF;
    char* end;
    long v = strtol(s.c_str() + 1, &end, 10);
    if (end == s.c_str() + 1 || v <= 0 || v > 254) return 0xFF;
    return static_cast<uint8_t>(v);
}

/**
 * "N4" → 4，前缀 'N'，失败返回 0xFFFF
 */
inline uint16_t node_id_from_str(const std::string& s) {
    if (s.empty() || s[0] != 'N') return 0xFFFF;
    char* end;
    long v = strtol(s.c_str() + 1, &end, 10);
    if (end == s.c_str() + 1 || v < 0 || v > 0xFFFE) return 0xFFFF;
    return static_cast<uint16_t>(v);
}

/**
 * "L8" → 8，前缀 'L'，失败返回 0xFFFF
 */
inline uint16_t edge_id_from_str(const std::string& s) {
    if (s.empty() || s[0] != 'L') return 0xFFFF;
    char* end;
    long v = strtol(s.c_str() + 1, &end, 10);
    if (end == s.c_str() + 1 || v < 0 || v > 0xFFFE) return 0xFFFF;
    return static_cast<uint16_t>(v);
}

/// 数字 → "C1" 格式字符串
inline std::string car_id_to_str(uint8_t id) {
    return std::string("C") + std::to_string(id);
}
inline std::string node_id_to_str(uint16_t id) {
    return std::string("N") + std::to_string(id);
}
inline std::string edge_id_to_str(uint16_t id,std::string errType="") {
    return std::string("L") + std::to_string(id)+errType;
}

// ─────────────────────────────────────────────────────────────────────────────
// SHM 状态 → JSON 字符串映射
// ─────────────────────────────────────────────────────────────────────────────

inline const char* car_status_str(uint8_t s) {
    // 对应 CarStatus 枚举
    switch (s) {
    case 0: return "idle";
    case 1: return "moving";
    case 2: return "fault";
    case 3: return "offline";
    case 4: return "wait";
    default: return "unknown";
    }
}

inline const char* node_status_str(uint8_t s) {
    switch (s) {
    case 0: return "idle";
    case 1: return "occupied";
    case 2: return "fault";
    default: return "unknown";
    }
}

inline const char* edge_status_str(uint8_t s) {
    switch (s) {
    case 0: return "idle";
    case 1: return "occupied";
    case 2: return "blocked";
    case 3: return "fault_temp";
    case 4: return "fault_repair";
    default: return "unknown";
    }
}

} // namespace agv::http