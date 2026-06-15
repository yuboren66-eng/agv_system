# 主机调度后端

运行于 i.MX6ULL（Linux + Debian）的多 AGV 调度系统，负责路径规划、转向指引、从机通信和 Web API。

## 系统架构

```
用户交互层     浏览器 (SVG 拓扑地图)  ←→  Nginx + FastCGI

应用服务层     planner     router     http_service     mqtt_service
               路径规划    转向指引     HTTP API         MQTT 通信

核心模型层     model_manager → 共享内存 (SHM)
               MapData (seqlock)    CarData (seqlock)

系统服务层     POSIX MQ  │  signalfd  │  timerfd  │  结构化日志

硬件平台       i.MX6ULL (Linux + Debian)
```

## 进程间通信

| 方式 | 用途 |
|------|------|
| POSIX MQ | 进程间任务调度、命令分发（`mq_task_dispatch`、`mq_mqtt_publish`、`mq_route_exert`） |
| 共享内存 (SHM) | 地图拓扑与车辆状态存储，Seqlock + ProcMutex 实现无锁读 |
| signalfd | 信号 → fd，融入 poll 事件循环 |
| timerfd | 内核定时器，高精度不丢事件 |

## 核心进程

| 进程 | 入口 | 职责 |
|------|------|------|
| model_manager | `src/model_manager/` | 创建 SHM、MQ，初始化拓扑与车辆数据 |
| planner | `src/planner/` | A* 路径规划，监听 `mq_task_dispatch` |
| router | `src/router/` | 处理到达事件，计算转向，下发 ORIENT |
| mqtt_subscriber | `src/mqtt_service/` | 接收从机事件，更新 SHM，触发调度 |
| mqtt_publisher | `src/mqtt_service/` | 从 MQ 取消息发往从机 |
| log_daemon | `src/log_daemon/` | 收集各进程日志，写 systemd journal |
| HTTP Status | `src/http_service/` | `GET /api/status` 输出 SHM JSON 快照 |
| HTTP Task | `src/http_service/` | `POST /api/assign|cancel` 任务管理 |
| HTTP Topo | `src/http_service/` | `POST /api/ban|access|repair` 拓扑管理 |

所有进程采用统一的 poll 事件循环，无轮询、纯事件驱动。

## 共享内存布局

```
ShmLayout
├── ShmHeader (64B)      魔数、版本、初始化标志
├── MapData (~8KB)       节点、边、邻接表
│   ├── nodes_[32]
│   ├── edges_[128]
│   └── adj_[32]
└── CarData (~1KB)       车辆状态
    └── cars_[2]
```

读写保护：Seqlock（读者无锁）+ ProcMutex（写者互斥），读者优先级高于写者。

## 项目文件

```
master/
├── src/
│   ├── model_manager/    进程A：SHM + MQ 初始化
│   ├── planner/          A* 路径规划引擎
│   ├── router/           转向指引（ARGC 算法）
│   ├── mqtt_service/     MQTT 发布/订阅（libmosquitto）
│   ├── http_service/     FastCGI API（Status / Task / Topo）
│   └── log_daemon/       systemd journal 日志收集
├── include/
│   ├── agv/              消息与日志结构定义
│   └── config/           系统常量（节点数、车辆数、MQTT 地址）
├── lib/
│   ├── ipc/              MQ 封装、Seqlock、SHM 管理、signalfd
│   ├── logger/           结构化日志（LOG_DEBUG/INFO/WARN/ERROR/FATAL）
│   └── timer/            timerfd 封装
├── tests/                各模块单元测试
└── scripts/              systemd 服务文件、Nginx 配置
```

## 依赖

- C++17, CMake >= 3.16
- libmosquitto — MQTT
- libfcgi — FastCGI
- nlohmann-json — JSON
- POSIX RT (librt)

## API 文档

- HTTP API：`system-api.md`
- MQTT 协议：`mqtt-api.md`
