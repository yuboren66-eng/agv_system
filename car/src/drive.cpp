#include "drive.h"

// 将百分比占空比转换为 ledcWrite 的值
static uint32_t dutyPWM_calCCR(int16_t dutyPercent)
{
    if (dutyPercent < 0)
        dutyPercent = 0;
    if (dutyPercent > 100)
        dutyPercent = 100;

    uint32_t maxDuty = (1 << PWM_RESOLUTION) - 1; // 8位时为255
    return (uint32_t)dutyPercent * maxDuty / 100;
}

void drive_init(void)
{
    // PWM 通道初始化
    ledcSetup(PWM_CHANNEL_3, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(PWM_CHANNEL_4, PWM_FREQ, PWM_RESOLUTION);

    ledcAttachPin(DRIVE_PWM3_PIN, PWM_CHANNEL_3);
    ledcAttachPin(DRIVE_PWM4_PIN, PWM_CHANNEL_4);

    // 方向引脚初始化
    pinMode(DRIVE_AIN1_PIN, OUTPUT);
    pinMode(DRIVE_AIN2_PIN, OUTPUT);
    pinMode(DRIVE_BIN1_PIN, OUTPUT);
    pinMode(DRIVE_BIN2_PIN, OUTPUT);
    pinMode(STBY_PIN, OUTPUT);

    digitalWrite(DRIVE_AIN1_PIN, LOW);
    digitalWrite(DRIVE_AIN2_PIN, LOW);
    digitalWrite(DRIVE_BIN1_PIN, LOW);
    digitalWrite(DRIVE_BIN2_PIN, LOW);
    digitalWrite(STBY_PIN, HIGH); // 使能电机驱动

    // 初始占空比为0
    ledcWrite(PWM_CHANNEL_3, 0);
    ledcWrite(PWM_CHANNEL_4, 0);
}

void drive_setPWM34(int16_t duty3, int16_t duty4)
{
    drive_setPWM3(duty3);
    drive_setPWM4(duty4);
}

void drive_DIFFsetPWM34(int16_t DIFFduty)
{
    drive_setPWM34(BASIC_SPEEDR + DIFFduty, BASIC_SPEEDL - DIFFduty);
}

void drive_setPWM3(int16_t speed)
{
    if (speed >= 0)
    {
        drive_invPWM3(_DISABLE);
        ledcWrite(PWM_CHANNEL_3, dutyPWM_calCCR(speed));
    }
    else
    {
        if (!INV_ABLE)
        {
            ledcWrite(PWM_CHANNEL_3, 0);
            return;
        }
        drive_invPWM3(_ENABLE);
        ledcWrite(PWM_CHANNEL_3, dutyPWM_calCCR(-speed));
    }
}

void drive_setPWM4(int16_t speed)
{
    if (speed >= 0)
    {
        drive_invPWM4(_DISABLE);
        ledcWrite(PWM_CHANNEL_4, dutyPWM_calCCR(speed));
    }
    else
    {
        if (!INV_ABLE)
        {
            ledcWrite(PWM_CHANNEL_4, 0);
            return;
        }
        drive_invPWM4(_ENABLE);
        ledcWrite(PWM_CHANNEL_4, dutyPWM_calCCR(-speed));
    }
}

void drive_invPWM3(int8_t alter)
{
    if (alter == -1)
    {
        digitalWrite(DRIVE_AIN1_PIN, LOW);
        digitalWrite(DRIVE_AIN2_PIN, LOW);
        return;
    }
    else
    {
        digitalWrite(DRIVE_AIN1_PIN, alter);
        digitalWrite(DRIVE_AIN2_PIN, !alter);
    }
}

void drive_invPWM4(int8_t alter)
{
    if (alter == -1)
    {
        digitalWrite(DRIVE_BIN1_PIN, LOW);
        digitalWrite(DRIVE_BIN2_PIN, LOW);
        return;
    }
    else
    {
        digitalWrite(DRIVE_BIN1_PIN, alter);
        digitalWrite(DRIVE_BIN2_PIN, !alter);
    }
}