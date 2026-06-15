#ifndef __KEY_H
#define __KEY_H

#include <Arduino.h>

#define KEY1 36
#define KEY2 37
#define KEY3 38

/* =========================================================
 *                      初始化
 * ========================================================= */

/*
 * 注意：
 * FreeRTOS版本中：
 *
 * 1. key模块不再负责 attachInterrupt
 * 2. key模块不再保存 flag
 * 3. 中断统一由 main.cpp 管理
 *
 * 原因：
 * 防止重复注册ISR
 */
void key_init(void);

#endif