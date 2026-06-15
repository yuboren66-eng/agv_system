#ifndef ENCODER_H
#define ENCODER_H

#include <Arduino.h>

#define E1A 16
#define E1B 48
#define E2A 47
#define E2B 21

// 转向角度到编码器计数的换算系数
#define K1 (1500.0f / 360.0f)
#define K2 (-1300.0f / 360.0f)

// 【修复】全部改为 volatile，防止编译器优化掉对这些变量的读取
extern volatile long targetCount1;
extern volatile long targetCount2;
extern volatile bool isTurning;

void encoder_init();

long getEncoder1Count();
long getEncoder2Count();

void resetEncoders();

void Turn(float angle);
bool checkTurnDone();

#endif