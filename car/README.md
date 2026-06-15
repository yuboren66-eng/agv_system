# ESP32-S3 小车从机固件

基于 FreeRTOS 多任务架构的小车嵌入式控制系统，实现视觉巡线、障碍物分类识别与避障、MQTT 远程指令通信、双车协同。

## 硬件

- **主控**：ESP32-S3-DevKitM-1（双核 Xtensa LX7）
- **视觉**：K230 摄像头，UART 通信，输出 `dx,node_flag,obstacle_type:x` 格式数据帧
- **显示**：SH1106 OLED 128×64（I2C）
- **驱动**：TB6612 双路电机驱动 + 双编码器里程计

## 软件架构

### FreeRTOS 任务分配

| 任务 | 核心 | 优先级 | 职责 |
|------|------|--------|------|
| ControlTask | Core 1 | 5 | 状态机决策、障碍物响应、运动控制 |
| VisionTask | Core 1 | 4 | K230 串口数据接收与解析 |
| MQTTTask | Core 0 | 3 | MQTT 连接维护与消息收发 |
| DisplayTask | Core 1 | 1 | OLED 实时状态刷新 |
| SafetyTask | Core 1 | 2 | 安全监控（预留扩展） |
| KeyTask | Core 1 | 2 | 按键中断处理 |

### 有限状态机

```
IDLE ⇄ FOLLOW → TURN → ACQUIRE → FOLLOW
  ↑                ↓
  └── WAIT_COMMAND ←──┘
        ↑
      CAPTURE
```

7 个主状态，6 种状态转移。其中 ACQUIRE 负责转弯后视觉微调对线，CAPTURE 负责 K230 拍照工作流。

### 路口三阶段模型

针对路口场景的时序冲突问题，将路口处理细化：

| 阶段 | 说明 | 障碍物策略 |
|------|------|-----------|
| JS_NONE | 正常循线 | 立即响应 |
| JS_IN | 检测到路口，定长直行至中心 | 暂存，等 orient 指令后判断 |
| JS_OUT | 转弯/直行后抑制期 | 完全抑制，避免误触发 |

### 障碍物分层处理

根据障碍物类型和当前阶段，采用四种应对策略：

| 场景 | 策略 |
|------|------|
| 路口标记线高电平 + 非抑制期 | 紧急停车 |
| 路口定长期 / 等待指令期 | 暂存障碍物信息 |
| 正常循线 | 立即停车上报 |
| 转弯后抑制期 | 不响应 |

临时障碍（dog/cat/people）消失后自动恢复并上报 REPAIRED；永久障碍（hole/stone）记录编码器位置，等待掉头指令后返回原点。

### 双车协同

通过视觉识别前车尾部的 slave 标记实现跟车协调。1 号车负责上报障碍物，2 号车静默等待，slave 消失后 2 号车延迟 1 秒恢复，防止 1 号车未完全离开时误触发。

## MQTT 通信

- **订阅** `car/{id}/cmd` — 接收主机指令（ORIENT / ACTION / CNT / CAPTURE）
- **发布** `car/{id}/event` — 上报事件（ARRIVE / OBSTACLE / REPAIRED / INFO / POSITION）

## 项目文件

```
car/
├── src/
│   ├── main.cpp          控制主逻辑、状态机、障碍物处理
│   ├── vision.cpp         K230 串口数据解析
│   ├── mqtt_handler.cpp   MQTT 回调与消息发送
│   ├── drive.cpp          电机 PWM 驱动
│   ├── encoder.cpp        编码器 ISR 与里程计
│   └── key.cpp            按键
├── include/
│   ├── config.h           系统参数（任务周期、PID、阈值）
│   ├── credentials.example.h 凭据模板
│   └── *.h                各模块头文件
└── platformio.ini         PlatformIO 配置
```

## 依赖

- PlatformIO (espressif32, Arduino 框架)
- U8g2 — OLED 显示
- PubSubClient — MQTT
- ArduinoJson v6 — JSON 解析
