#ifndef CONFIG_H
#define CONFIG_H

#include "credentials.h"

// ===================== 任务周期 (ms) =====================
#define CONTROL_TASK_PERIOD_MS     5
#define DISPLAY_TASK_PERIOD_MS     200
#define MQTT_TASK_PERIOD_MS        20
#define SAFETY_TASK_PERIOD_MS      100
#define COMMAND_WAIT_TIMEOUT_MS    5000
#define SLAVE_MIN_WAIT_MS          1000    // 车2 slave消失后最小等待时间，防视觉闪烁误恢复

// ===================== 编码器阈值 =====================
#define ENCODER_PASS_THRESHOLD     800     // 路口定长（编码器计数）
#define POST_TURN_DISTANCE         200     // 转弯/直行后抑制距离

// ===================== 循线控制参数 =====================
#define KP_DEFAULT                 0.18f   // 比例系数
#define DX_FILTER_ALPHA            0.75f   // 低通滤波：新样本权重
#define DX_FILTER_BETA             0.25f   // 低通滤波：旧值权重 (= 1-alpha)
#define DX_DEADZONE                3       // 死区
#define DX_CLAMP                   40      // dx 限幅

// ===================== 转向参数 =====================
#define TURN_ANGLE_LEFT            90
#define TURN_ANGLE_RIGHT          -90
#define TURN_ANGLE_UTURN           215
#define TURN_DIFF_PWM              25      // 转弯差速 PWM

// ===================== 视觉解析 =====================
#define VISION_RX_BUFFER_SIZE      80
#define VISION_BLOCK_READ_SIZE     32

// ===================== 车辆编号 =====================
#define CAR_ID       2            // 1 或 2，自动影响 MQTT topic 和防碰撞逻辑
#define CAR_POSITION "2-5"         // 按键上报的车辆位置

#endif
