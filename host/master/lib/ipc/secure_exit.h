#pragma once
 //注明：本部分的规划与编写均是ai完成，本人只是在其中调试&学习

#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <vector>
 
namespace agv {
 
class SecureExit {
public:
    struct CleanupEntry {
        std::string            name;
        std::function<void()>  fn;
    };
 
    explicit SecureExit(const char* proc_name) : proc_name_(proc_name) {}
 
    /*注册一个清理动作，名称仅用于日志。*/
    void add_cleanup(std::string name, std::function<void()> fn) {
        entries_.push_back({std::move(name), std::move(fn)});
    }
 
    /*执行完整退出序列，调用后进程结束。
     * @param wait_ms  等待当前任务完成的最长毫秒数（0 = 不等待）*/
    [[ noreturn ]] void run(int wait_ms = 200) {
        log_info("shutdown initiated, starting exit sequence");
 
        if (wait_ms > 0) {
            log_info("waiting up to " + std::to_string(wait_ms) + "ms for in-flight work");
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
        }

        for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
            log_info("cleanup: " + it->name);
            try {
                it->fn();
            } catch (const std::exception& e) {
                log_err("cleanup '" + it->name + "' threw: " + e.what());
            } catch (...) {
                log_err("cleanup '" + it->name + "' threw unknown exception");
            }
        }
        log_info("exit(0)");
        ::exit(0);
    }
 
private:
    void log_info(const std::string& msg) {
        fprintf(stderr, "[%s] INFO  exit_seq: %s\n", proc_name_, msg.c_str());
    }
    void log_err(const std::string& msg) {
        fprintf(stderr, "[%s] ERROR exit_seq: %s\n", proc_name_, msg.c_str());
    }
 
    const char*              proc_name_;
    std::vector<CleanupEntry> entries_;
};
 
} // namespace agv
 