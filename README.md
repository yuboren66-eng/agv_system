# 协同避障AGV系统

> 嵌入式系统课程设计项目

一个完整的多 AGV 协同运输系统，包含从机固件、主机调度、Web 监控三个部分。小车通过 K230 视觉模块巡线、识别障碍物类型并分类避障，主机基于 A* 算法实现路径规划与多车调度，Web 前端提供实时状态监控。

---

## 系统组成

```
├── car/               ESP32-S3 小车从机固件
│                      视觉巡线、障碍物检测、状态机控制、MQTT 通信
│
└── host/
    ├── master/        主机调度后端 (C++, Linux)
    │                  路径规划、转向指引、MQTT 服务、HTTP API
    │
    └── web/           Web 监控前端
                       SVG 拓扑地图、实时状态显示、任务下发
```

## 通信架构

```
K230 摄像头 ──UART──▶ ESP32从机 ◀──MQTT──▶ 主机 (i.MX6ULL) ◀──HTTP──▶ Web前端
                         ▲                        │
                         │      MQTT Broker        │
                         └────────────────────────┘
```

## 硬件平台

| 组件 | 型号 |
|------|------|
| 小车主控 | ESP32-S3-DevKitM-1 |
| 视觉模块 | K230 |
| 主机平台 | i.MX6ULL (Linux + Debian) |
| 显示 | SH1106 OLED 128×64 |
| 驱动 | TB6612 双路电机驱动 |

## 来源声明

`host/master/` 代码来源于 [uplf/ENavigate](https://github.com/uplf/ENavigate)，原作者保留一切权利。
