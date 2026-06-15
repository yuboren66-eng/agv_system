#include "encoder.h"

static volatile long count1 = 0;
static volatile long count2 = 0;

// 【修复】对应头文件声明，加 volatile
volatile long targetCount1 = 0;
volatile long targetCount2 = 0;
volatile bool isTurning = false;

portMUX_TYPE encoderMux = portMUX_INITIALIZER_UNLOCKED;

// ----------------------------------------------------------------
//  ISR：放在 IRAM，ESP32 两个编码器 ISR 优先级相同，
//  用 portENTER_CRITICAL_ISR 防止两个 ISR 互相抢占时的数据撕裂
// ----------------------------------------------------------------
void IRAM_ATTR isr1()
{
    portENTER_CRITICAL_ISR(&encoderMux);
    if (digitalRead(E1A) == HIGH)
        (digitalRead(E1B) == LOW) ? count1++ : count1--;
    else
        (digitalRead(E1B) == HIGH) ? count1++ : count1--;
    portEXIT_CRITICAL_ISR(&encoderMux);
}

void IRAM_ATTR isr2()
{
    portENTER_CRITICAL_ISR(&encoderMux);
    if (digitalRead(E2A) == HIGH)
        (digitalRead(E2B) == LOW) ? count2-- : count2++;
    else
        (digitalRead(E2B) == HIGH) ? count2-- : count2++;
    portEXIT_CRITICAL_ISR(&encoderMux);
}

void encoder_init()
{
    pinMode(E1A, INPUT_PULLUP);
    pinMode(E1B, INPUT_PULLUP);
    pinMode(E2A, INPUT_PULLUP);
    pinMode(E2B, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(E1A), isr1, CHANGE);
    attachInterrupt(digitalPinToInterrupt(E2A), isr2, CHANGE);
}

long getEncoder1Count()
{
    long temp;
    portENTER_CRITICAL(&encoderMux);
    temp = count1;
    portEXIT_CRITICAL(&encoderMux);
    return temp;
}

long getEncoder2Count()
{
    long temp;
    portENTER_CRITICAL(&encoderMux);
    temp = count2;
    portEXIT_CRITICAL(&encoderMux);
    return temp;
}

void resetEncoders()
{
    portENTER_CRITICAL(&encoderMux);
    count1 = 0;
    count2 = 0;
    portEXIT_CRITICAL(&encoderMux);
}

void Turn(float angle)
{
    resetEncoders();
    if (angle > 0)
    {
        targetCount1 = (long)(angle * K1);
        targetCount2 = (long)(angle * K2);
    }
    else
    {
        targetCount1 = (long)(angle * K2);
        targetCount2 = (long)(angle * K1);
    }
    isTurning = true;
}

bool checkTurnDone()
{
    if (!isTurning)
        return true;

    bool m1_done = abs(getEncoder1Count()) >= abs(targetCount1);
    bool m2_done = abs(getEncoder2Count()) >= abs(targetCount2);

    if (m1_done || m2_done)
    {
        isTurning = false;
        return true;
    }
    return false;
}