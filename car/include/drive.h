#ifndef DRIVE_H
#define DRIVE_H

#include <Arduino.h>

// ===================== 引脚定义 =====================
#define DRIVE_AIN1_PIN 10
#define DRIVE_AIN2_PIN 9
#define DRIVE_BIN1_PIN 12
#define DRIVE_BIN2_PIN 13
#define STBY_PIN 11

#define DRIVE_PWM3_PIN 8  // 电机3 PWM
#define DRIVE_PWM4_PIN 14 // 电机4 PWM

// ===================== PWM配置 =====================
#define PWM_FREQ 1000    // 1kHz
#define PWM_RESOLUTION 8 // 8位，范围0~255
#define PWM_CHANNEL_3 0
#define PWM_CHANNEL_4 1

// ===================== 速度参数 =====================
#define BASIC_SPEEDR 20
#define BASIC_SPEEDL 20

// 是否允许反转
#define INV_ABLE 1

// 宏定义
#define _ENABLE 1
#define _DISABLE 0

void drive_init(void);

void drive_setPWM34(int16_t duty3, int16_t duty4);
void drive_DIFFsetPWM34(int16_t DIFFduty);

void drive_setPWM3(int16_t speed);
void drive_setPWM4(int16_t speed);

void drive_invPWM3(int8_t alter);
void drive_invPWM4(int8_t alter);

#endif