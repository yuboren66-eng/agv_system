#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <U8g2lib.h>

#include "config.h"
#include "drive.h"
#include "encoder.h"
#include "key.h"
#include "mqtt_handler.h"
#include "vision.h"

// ===================== 硬件引脚 =====================
#define SDA_PIN 4
#define SCL_PIN 5
#define LED_PIN 2
#define RXPIN 18
#define TXPIN 17

// ===================== 显示 =====================
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// ===================== 状态枚举 =====================
enum State
{
    STATE_IDLE,
    STATE_FOLLOW,
    STATE_TURN,
    STATE_ACQUIRE,     // 转弯后相机找线微调
    STATE_WAIT_COMMAND,
    STATE_CAPTURE,     // K230 拍照工作流
};

// ===================== 路口阶段（替代 isTurnPending + nodeDetectionLocked + postTurnActive）=====================
enum JuncSt
{
    JS_NONE,       // 正常循线，障碍物立即响应
    JS_IN,   // 检测到路口 → 直行走向中心 → 等待指令（障碍物暂存）
    JS_OUT,  // 转弯/直行后抑制期（障碍物和节点都不响应）
};

// ===================== 系统状态快照（用于 DisplayTask）=====================
typedef struct
{
    State currentState;
    int dx;
    int node_flag;
    char obstacle_type[32];
    long encoder1;
    long encoder2;
    int obsX;
    Orient orient;
    int roadnum;
    bool wifiConnected;
    int roadcount;
} SysInfo;

// ===================== 控制上下文（ControlTask 全部可变状态）=====================
typedef struct
{
    bool obsSent;               // 障碍已上报，避免重复
    bool tempStop;             // dog/cat/people 停车标记，消失后自动恢复
    bool slaveStop;              // slave 停车标记，3s 后 1 号上报
    bool slaveSent;              // 已上报 SLAVE，避免重复
    bool obsWait;    // 路口接近期间暂存障碍物
    char obsType[32];   // 暂存的障碍物类型
    int obsX;                        // 障碍物 x 坐标 (0~320)
    uint32_t slaveT;         // slave 停车时刻，用于 3s 超时
} ObsCtx;

// 拍照工作流子阶段
enum CapSt
{
    CAP_IN,          // 发 "1" 给 K230
    CAP_ACK,       // 等 ACK
    CAP_NAME,      // 发命名串
    CAP_RESP,      // 等 NEXT 或 DONE
    CAP_ROT,         // 转 70°
    CAP_TURN,      // 等转向完成
};

typedef struct
{
    State state;
    Orient orient;
    int roadNum;
    int roadCount;
    int lastNd;
    JuncSt juncPhase;
    bool gotOrient;
    ObsCtx obs;
    long endE1;
    long endE2;
    uint32_t waitT;
    int ncnt;                   // "none" 连续帧计数，≥3 才确认消失
    int acqCnt;                    // STATE_ACQUIRE 中线稳定帧计数
    long bkE1;                  // 永久障碍停车时的编码器值，掉头后回原点用
    long bkE2;
    bool backing;                // 正在返回原点（掉头回路口）
    uint32_t blinkT;           // LED 闪烁计时
    // 拍照工作流
    CapSt capSt;
    char capName[48];              // 命名串 "C1+N5+20260613220846"
    int capCnt;                  // 已拍张数 0~4
    uint32_t capT;         // 等待超时起点
    bool capWait;               // 路口区间内收到 CAPTURE，延后执行
    State preState;             // CAPTURE 前状态，拍完恢复
    JuncSt preCapSt;     // CAPTURE 前路口阶段
    bool fromCap;              // ACQUIRE 来自 CAPTURE，完成后恢复状态
    bool atJunc;               // 掉头后在路口，ACQUIRE 完直接 WAIT_COMMAND
    float fDx;              // 循线低通滤波值
} Ctrl;

// ===================== 全局共享资源 =====================
static SysInfo g_sys;
static uint8_t g_rstRsn = 0;

QueueHandle_t g_visQ;
QueueHandle_t g_cmdQ;
SemaphoreHandle_t g_mtx;
QueueHandle_t g_keyQ;
volatile bool g_capMode = false; // 拍照期间暂停 VisionTask

// CAPTURE 命令参数，由 mqtt_handler 写入
extern char g_capNode[16];
extern char g_capTs[20];
extern bool g_capReq;

// ===================== 函数声明 =====================
void initHardware();
void initDisplay();
void initWIFI();

void ControlTask(void *pvParameters);
void MQTTTask(void *pvParameters);
void DisplayTask(void *pvParameters);
void SafetyTask(void *pvParameters);
void KeyTask(void *pvParameters);

void IRAM_ATTR key1_isr();
void IRAM_ATTR key2_isr();

// ===================== 工具函数 =====================
static inline uint32_t nowMs()
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

// ===================== 控制辅助函数 =====================

// 统一转向分发：orient → 驱动指令 + 目标状态
static State doOrient(Orient orient)
{
    switch (orient)
    {
    case O_LEFT:
        Turn(TURN_ANGLE_LEFT);
        drive_setPWM34(TURN_DIFF_PWM, -TURN_DIFF_PWM);
        return STATE_TURN;

    case O_RIGHT:
        Turn(TURN_ANGLE_RIGHT);
        drive_setPWM34(-TURN_DIFF_PWM, TURN_DIFF_PWM);
        return STATE_TURN;

    case O_UTURN:
        Turn(TURN_ANGLE_UTURN);
        drive_setPWM34(-TURN_DIFF_PWM, TURN_DIFF_PWM);
        return STATE_TURN;

    case O_STRAIGHT:
        resetEncoders();
        return STATE_FOLLOW;

    case O_ARRIVED:
        drive_setPWM34(0, 0);
        return STATE_IDLE;

    default:
        drive_setPWM34(0, 0);
        mqtt_send_info("No valid orient, stopped");
        return STATE_IDLE;
    }
}

// 重置路口上下文（进入 IDLE 或新路口时调用）
static void rstJunc(Ctrl &ctx)
{
    ctx.juncPhase = JS_NONE;
    ctx.gotOrient = false;
    ctx.obs.obsWait = false;
}

// 检测到路口：初始化路口阶段 + 发 ARRIVE + 计数
static void onJunc(Ctrl &ctx)
{
    ctx.juncPhase = JS_IN;
    resetEncoders();
    ctx.gotOrient = false;
    ctx.obs.obsWait = false;
    ctx.obs.obsSent = false; // 新路口，旧障碍数据作废
    ctx.fDx = 0.0f;        // 进路口清零滤波，防止旧偏差干扰
    mqtt_send_arrive();
    ctx.roadCount++;
}

// 进入拍照工作流
static void startCap(Ctrl &ctx)
{
    ctx.preState = ctx.state;
    ctx.preCapSt = ctx.juncPhase;
    ctx.capSt = CAP_IN;
    ctx.state = STATE_CAPTURE;
    drive_setPWM34(0, 0);
}

// 清除永久障碍返回状态
static void rstBack(Ctrl &ctx)
{
    ctx.backing = false;
    ctx.bkE1 = 0;
    ctx.bkE2 = 0;
}

// 统一障碍物停车上报
static void stopObs(Ctrl &ctx, const char *obsType, bool afterThreshold)
{
    if (!ctx.obs.obsSent)
    {
        ctx.obs.obsSent = true;
        if (strcmp(obsType, "slave") == 0)
        {
            ctx.obs.slaveStop = true;
            ctx.obs.slaveT = nowMs();
            // 1 号报障碍物，2 号静默等待
            if (CAR_ID == 1)
            {
                String label = String(obsType);
                label = (afterThreshold ? "y" : "n") + label;
                mqtt_send_obstacle(label);
            }
        }
        else
        {
            String label = String(obsType);
            label = (afterThreshold ? "y" : "n") + label;
            mqtt_send_obstacle(label);
            if (strcmp(obsType, "dog") == 0 ||
                strcmp(obsType, "cat") == 0 ||
                strcmp(obsType, "people") == 0)
                ctx.obs.tempStop = true;
        }
    }
    drive_setPWM34(0, 0);
    ctx.state = STATE_IDLE;
    rstJunc(ctx);

    // 非临时障碍（非 dog/cat/people）：记录停车位置，-100 补偿刹车过冲
    if (!ctx.obs.tempStop)
    {
        ctx.bkE1 = abs(getEncoder1Count()) - 100;
        ctx.bkE2 = abs(getEncoder2Count()) - 100;
        if (ctx.bkE1 < 0) ctx.bkE1 = 0;
        if (ctx.bkE2 < 0) ctx.bkE2 = 0;
        ctx.backing = false;
    }

    // 所有障碍物：主动刹车 100ms，减少滑行距离
    drive_setPWM34(-30, -30);
    vTaskDelay(pdMS_TO_TICKS(100));
    drive_setPWM34(0, 0);
}

// 路口 orient 决策 + 暂存障碍物判断（FOLLOW 和 WAIT_COMMAND 共用）
static void execOrient(Ctrl &ctx)
{
    if (ctx.obs.obsWait)
    {
        if (ctx.orient == O_STRAIGHT)
        {
            // 直行路径上有障碍物 → 停车上报（已在路口中心，掉头即到）
            stopObs(ctx, ctx.obs.obsType, true);
            ctx.bkE1 = 0;
            ctx.bkE2 = 0;
            ctx.atJunc = true;
            ctx.orient = O_NONE;
            ctx.gotOrient = false;
            ctx.obs.obsWait = false;
            return;
        }
        // 转弯路径不受直线障碍影响，忽略
        ctx.obs.obsWait = false;
    }

    ctx.state = doOrient(ctx.orient);
    ctx.orient = O_NONE;
    ctx.gotOrient = false;

    if (ctx.state == STATE_FOLLOW)
    {
        // 直行：进入抑制期
        ctx.endE1 = 0;
        ctx.endE2 = 0;
        ctx.juncPhase = JS_OUT;
    }
}

// ============================================================
//  setup
// ============================================================
void setup()
{
    Serial.begin(115200);
    g_rstRsn = (uint8_t)esp_reset_reason();

    initHardware();

    g_visQ = xQueueCreate(5, sizeof(VisionData_t));
    g_cmdQ = xQueueCreate(5, sizeof(Cmd_t));
    g_mtx = xSemaphoreCreateMutex();
    g_keyQ = xQueueCreate(5, sizeof(uint8_t));

    configASSERT(g_visQ);
    configASSERT(g_cmdQ);
    configASSERT(g_mtx);
    configASSERT(g_keyQ);

    xTaskCreatePinnedToCore(ControlTask, "ControlTask", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(VisionTask, "VisionTask", 4096, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(MQTTTask, "MQTTTask", 8192, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(DisplayTask, "DisplayTask", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(SafetyTask, "SafetyTask", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(KeyTask, "KeyTask", 2048, NULL, 2, NULL, 1);
}

void loop()
{
    vTaskDelete(NULL);
}

// ============================================================
//  硬件初始化
// ============================================================
void initHardware()
{
    pinMode(LED_PIN, OUTPUT);

    encoder_init();
    key_init();
    drive_init();
    resetEncoders();

    Serial1.begin(115200, SERIAL_8N1, RXPIN, TXPIN);

    initDisplay();
    initWIFI();
    initMQTT();

    attachInterrupt(digitalPinToInterrupt(KEY1), key1_isr, FALLING);
    attachInterrupt(digitalPinToInterrupt(KEY2), key2_isr, FALLING);
}

void initDisplay()
{
    Wire.begin(SDA_PIN, SCL_PIN);
    u8g2.begin();
    u8g2.enableUTF8Print();
}

void initWIFI()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// ============================================================
//  ControlTask
// ============================================================
void ControlTask(void *pvParameters)
{
    QueueSetHandle_t queueSet = xQueueCreateSet(5 + 5);
    configASSERT(queueSet);
    xQueueAddToSet(g_visQ, queueSet);
    xQueueAddToSet(g_cmdQ, queueSet);

    Ctrl ctx = {};
    ctx.state = STATE_IDLE;

    VisionData_t visionData = {};
    Cmd_t command = {};

    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1)
    {
        xQueueSelectFromSet(queueSet, pdMS_TO_TICKS(CONTROL_TASK_PERIOD_MS));

        // 1. 清空视觉队列，只保留最新帧
        while (xQueueReceive(g_visQ, &visionData, 0) == pdTRUE)
            ;

        // 2. 清空命令队列，逐条处理
        while (xQueueReceive(g_cmdQ, &command, 0) == pdTRUE)
        {
            if (command.orient != O_NONE)
            {
                ctx.orient = command.orient;
                ctx.gotOrient = true;
            }

            switch (command.action)
            {
            case A_PAUSE:
                drive_setPWM34(0, 0);
                ctx.state = STATE_IDLE;
                break;

            case A_PROCESS:
                if (ctx.state == STATE_IDLE)
                {
                    ctx.state = STATE_FOLLOW;
                    ctx.lastNd = 0;
                    rstBack(ctx);
                    ctx.obs.obsSent = false;
                    ctx.atJunc = false;
                }
                break;

            case A_UTURN:
                Turn(TURN_ANGLE_UTURN);
                drive_setPWM34(-TURN_DIFF_PWM, TURN_DIFF_PWM);
                ctx.state = STATE_TURN;
                // 若因永久障碍停车，掉头后返回原点
                if (ctx.bkE1 > 0 || ctx.bkE2 > 0)
                    ctx.backing = true;
                // 清除 slave 状态，后续和 motor 完全一致
                ctx.obs.slaveStop = false;
                ctx.obs.slaveSent = false;
                digitalWrite(LED_PIN, LOW);
                break;

            case A_SETN:
                ctx.roadNum = command.roadnum;
                ctx.roadCount = 0;
                break;

            case A_CAPTURE:
                if (g_capReq)
                {
                    g_capReq = false;
                    snprintf(ctx.capName, sizeof(ctx.capName),
                             "C%d+%s+%s", CAR_ID, g_capNode, g_capTs);
                    ctx.capCnt = 0;
                    // 路口区间内则延后到完成后执行
                    if (ctx.state == STATE_TURN ||
                        ctx.state == STATE_ACQUIRE ||
                        ctx.state == STATE_WAIT_COMMAND ||
                        (ctx.state == STATE_FOLLOW && ctx.juncPhase != JS_NONE))
                    {
                        ctx.capWait = true;
                    }
                    else
                    {
                        startCap(ctx);
                    }
                }
                break;

            default:
                break;
            }

            command.action = A_NONE;
            command.orient = O_NONE;
            command.roadnum = 0;
        }

        // ============================================================
        //  障碍检测（分层处理）
        //  A. node_flag=1 → 障碍物在路口 → 急停
        //  B+C. APPROACH 或 WAIT_COMMAND → 暂存，等 orient
        //  D. 正常循线 → 立即停车
        //  POST_TURN 期间全部抑制
        // ============================================================
        if (strcmp(visionData.obstacle_type, "none") != 0 &&
            strcmp(visionData.obstacle_type, "car") != 0)
        {
            ctx.ncnt = 0; // 障碍物出现，清零消抖计数
            ctx.obs.obsX = visionData.obs_x; // 记录障碍物 x 坐标

            // x 坐标在 120~230 外 → 相邻道路，忽略；无坐标(-1)也处理
            if (visionData.obs_x < 0 ||
                (visionData.obs_x >= 120 && visionData.obs_x <= 230))
            {
                // --- 情况 A：路口标记线高电平 + 非抑制期 → 急停 ---
            if (ctx.state == STATE_FOLLOW &&
                visionData.node_flag == 1 &&
                ctx.juncPhase != JS_OUT)
            {
                // 若此周期正好是上升沿，先上报 ARRIVE 再上报 OBSTACLE
                if (ctx.juncPhase == JS_NONE && ctx.lastNd == 0)
                {
                    onJunc(ctx);
                }
                stopObs(ctx, visionData.obstacle_type, true);
            }
            // --- 情况 B+C：APPROACH 定长期 或 WAIT_COMMAND 等待期 → 暂存 ---
            else if ((ctx.state == STATE_FOLLOW &&
                      ctx.juncPhase == JS_IN &&
                      visionData.node_flag == 0) ||
                     ctx.state == STATE_WAIT_COMMAND)
            {
                if (!ctx.obs.obsWait)
                {
                    ctx.obs.obsWait = true;
                    strncpy(ctx.obs.obsType, visionData.obstacle_type,
                            sizeof(ctx.obs.obsType) - 1);
                    ctx.obs.obsType[sizeof(ctx.obs.obsType) - 1] = '\0';
                }
            }
            // --- 情况 D：正常循线 + 非抑制期 → 立即停车 ---
            else if (ctx.state == STATE_FOLLOW &&
                     ctx.juncPhase != JS_OUT)
            {
                stopObs(ctx, visionData.obstacle_type, true);
            } // end case D
            } // end x-range filter
        }
        else if (strcmp(visionData.obstacle_type, "none") == 0)
        {
            ctx.ncnt++;

            // 连续 3 帧 "none" 才确认障碍物真正消失，防止相机 flicker 误触发
            if (ctx.ncnt >= 3)
            {
                if (ctx.obs.tempStop)
                {
                    ctx.obs.tempStop = false;
                    mqtt_send_repaired();
                    digitalWrite(LED_PIN, LOW);
                    ctx.lastNd = visionData.node_flag;
                    ctx.state = STATE_FOLLOW;
                }
                if (ctx.obs.slaveStop && !ctx.obs.slaveSent)
                {
                    // 车2: slave消失后最小等待，防止车1未完全离开时视觉闪烁导致误恢复
                    if (CAR_ID == 2 &&
                        (nowMs() - ctx.obs.slaveT) < SLAVE_MIN_WAIT_MS)
                    {
                        // 在最小等待期内，暂不恢复
                    }
                    else
                    {
                        ctx.obs.slaveStop = false;
                        mqtt_send_repaired();
                        digitalWrite(LED_PIN, LOW);
                        ctx.lastNd = visionData.node_flag;
                        ctx.state = STATE_FOLLOW;
                    }
                }
                ctx.obs.obsSent = false;
                ctx.obs.obsWait = false;
            }
        }

        // ============================================================
        //  状态机
        // ============================================================
        switch (ctx.state)
        {
        // ---- IDLE ----
        case STATE_IDLE:
            // 延后的 CAPTURE
            if (ctx.capWait)
            {
                ctx.capWait = false;
                startCap(ctx);
                break;
            }

            // 临时障碍/slave 停车时 LED 闪烁
            if (ctx.obs.tempStop || ctx.obs.slaveStop)
            {
                if (nowMs() - ctx.blinkT >= 500)
                {
                    ctx.blinkT = nowMs();
                    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
                }

                // slave 超时 3s：1 号上报 SLAVE（仅一次），2 号继续等
                if (ctx.obs.slaveStop &&
                    !ctx.obs.slaveSent &&
                    nowMs() - ctx.obs.slaveT > 3000)
                {
                    if (CAR_ID == 1)
                    {
                        ctx.obs.slaveSent = true;
                        mqtt_send_obstacle("yslave");
                    }
                }
            }
            else
            {
                digitalWrite(LED_PIN, LOW);
            }
            break;

        // ---- FOLLOW ----
        case STATE_FOLLOW:
        {
            // 延后的 CAPTURE：路口操作完成后立即执行
            if (ctx.capWait && ctx.juncPhase == JS_NONE)
            {
                ctx.capWait = false;
                startCap(ctx);
                break;
            }

            int dx;

            // APPROACH 或高电平期间直行，其余时间正常循线
            if (ctx.juncPhase == JS_IN || visionData.node_flag == 1)
            {
                ctx.fDx = 0.0f;
                dx = 0;
            }
            else
            {
                ctx.fDx = ctx.fDx * DX_FILTER_BETA + (float)visionData.dx * DX_FILTER_ALPHA;
                dx = (int)ctx.fDx;

                if (abs(dx) < DX_DEADZONE)
                    dx = 0;

                if (dx > DX_CLAMP)  dx = DX_CLAMP;
                if (dx < -DX_CLAMP) dx = -DX_CLAMP;
            }

            drive_DIFFsetPWM34(-dx * KP_DEFAULT);

            // 路口上升沿检测
            if (ctx.juncPhase == JS_NONE &&
                visionData.node_flag == 1 && ctx.lastNd == 0)
            {
                onJunc(ctx);
            }

            // 道路计数上限检查
            if (ctx.roadNum != 0 && ctx.roadCount >= ctx.roadNum + 1)
            {
                drive_setPWM34(0, 0);
                ctx.state = STATE_IDLE;
                ctx.roadCount = 0;
                ctx.orient = O_NONE;
                rstJunc(ctx);
                mqtt_send_info("Road count reached");
                break;
            }

            // 通过路口中心点后执行转向决策
            if (getEncoder1Count() > ENCODER_PASS_THRESHOLD &&
                getEncoder2Count() > ENCODER_PASS_THRESHOLD &&
                ctx.juncPhase == JS_IN)
            {
                if (ctx.gotOrient)
                {
                    execOrient(ctx);
                }
                else
                {
                    drive_setPWM34(0, 0);
                    ctx.waitT = nowMs();
                    ctx.state = STATE_WAIT_COMMAND;
                    mqtt_send_info("Waiting for orient at node");
                }
            }

            ctx.lastNd = visionData.node_flag;

            // 永久障碍返回原点：走回记录的编码器距离，到达路口
            if (ctx.backing &&
                abs(getEncoder1Count()) >= ctx.bkE1 &&
                abs(getEncoder2Count()) >= ctx.bkE2)
            {
                rstBack(ctx);
                ctx.obs.obsSent = false;
                drive_setPWM34(0, 0);
                onJunc(ctx);
                ctx.waitT = nowMs();
                ctx.state = STATE_WAIT_COMMAND;
                break;
            }

            // 转弯/直行后抑制解锁
            if (ctx.juncPhase == JS_OUT && visionData.node_flag == 0)
            {
                long d1 = abs(getEncoder1Count() - ctx.endE1);
                long d2 = abs(getEncoder2Count() - ctx.endE2);
                if (d1 >= POST_TURN_DISTANCE && d2 >= POST_TURN_DISTANCE)
                {
                    ctx.juncPhase = JS_NONE;
                }
            }

            break;
        }

        // ---- WAIT_COMMAND ----
        case STATE_WAIT_COMMAND:
        {
            if (ctx.gotOrient)
            {
                execOrient(ctx);
            }
            else if ((nowMs() - ctx.waitT) >= COMMAND_WAIT_TIMEOUT_MS)
            {
                drive_setPWM34(0, 0);
                ctx.state = STATE_IDLE;
                rstJunc(ctx);
                mqtt_send_info("ERROR: orient timeout (>5s)");
            }
            break;
        }

        // ---- TURN ----
        case STATE_TURN:
            if (checkTurnDone())
            {
                ctx.state = STATE_ACQUIRE;
                ctx.acqCnt = 0;
            }
            break;

        // ---- ACQUIRE：转弯后相机找线微调 ----
        case STATE_ACQUIRE:
        {
            int dx = visionData.dx;
            // 低速旋转找线
            // 比例微调：偏离越大转越快，接近中线自然减速
            int acqPwm = constrain(abs(dx) / 4, 0, 20);
            if (acqPwm < 5) acqPwm = 0;  // 死区

            if (dx > 0)
                drive_setPWM34(-acqPwm, acqPwm);   // 线在右，右转
            else if (dx < 0)
                drive_setPWM34(acqPwm, -acqPwm);   // 线在左，左转
            else
                drive_setPWM34(0, 0);

            if (acqPwm == 0)
                ctx.acqCnt++;
            else
                ctx.acqCnt = 0;

            // 连续 8 帧稳定 → 对准完成
            if (ctx.acqCnt >= 8)
            {
                if (ctx.fromCap)
                {
                    ctx.fromCap = false;
                    ctx.state = ctx.preState;
                    ctx.juncPhase = ctx.preCapSt;
                }
                else if (ctx.atJunc)
                {
                    // 掉头后在路口：直接进 WAIT_COMMAND 等新 orient
                    ctx.atJunc = false;
                    rstBack(ctx);
                    onJunc(ctx);
                    ctx.waitT = nowMs();
                    ctx.state = STATE_WAIT_COMMAND;
                }
                else
                {
                    ctx.state = STATE_FOLLOW;
                    ctx.endE1 = 0;
                    ctx.endE2 = 0;
                    resetEncoders();
                    ctx.juncPhase = JS_OUT;
                }
            }
            break;
        }
        // ---- CAPTURE：K230 拍照工作流 ----
        case STATE_CAPTURE:
        {
            // 循环读取 Serial1，读完所有行（VisionTask 已暂停）
            if (ctx.capSt == CAP_ACK || ctx.capSt == CAP_RESP)
            {
                while (Serial1.available())
                {
                    String rx = Serial1.readStringUntil('\n');
                    rx.trim();
                    if (rx.length() == 0) continue;

                    if (ctx.capSt == CAP_ACK && rx == "ACK")
                    {
                        ctx.capSt = CAP_NAME;
                    }
                    else if (ctx.capSt == CAP_RESP)
                    {
                        if (rx == "NEXT")
                        {
                            ctx.capSt = CAP_ROT;
                        }
                        else if (rx == "DONE")
                        {
                            // 最后再转一次，补足 5 次旋转
                            ctx.capSt = CAP_ROT;
                            break;
                        }
                    }
                }
            }

            // 超时保护：10 秒无响应则退出
            if (nowMs() - ctx.capT > 10000 &&
                (ctx.capSt == CAP_ACK || ctx.capSt == CAP_RESP ||
                 ctx.capSt == CAP_TURN))
            {
                g_capMode = false;
                ctx.state = STATE_IDLE;
                mqtt_send_info("CAPTURE timeout");
                break;
            }

            // 各阶段动作
            switch (ctx.capSt)
            {
            case CAP_IN:
                g_capMode = true;
                Serial1.print("1\r\n");
                ctx.capT = nowMs();
                ctx.capSt = CAP_ACK;
                break;

            case CAP_NAME:
                Serial1.print(ctx.capName);
                Serial1.print("\r\n");
                ctx.capT = nowMs();
                ctx.capSt = CAP_RESP;
                break;

            case CAP_ROT:
                Turn(60);
                drive_setPWM34(TURN_DIFF_PWM, -TURN_DIFF_PWM);
                ctx.capSt = CAP_TURN;
                ctx.capT = nowMs();
                break;

            case CAP_TURN:
                if (checkTurnDone())
                {
                    drive_setPWM34(0, 0);
                    ctx.capCnt++;
                    if (ctx.capCnt >= 5)
                    {
                        // 5 次转完，退出拍照
                        g_capMode = false;
                        ctx.fromCap = true;
                        ctx.state = STATE_ACQUIRE;
                        ctx.acqCnt = 0;
                    }
                    else
                    {
                        Serial1.print("CAPTURE\r\n");
                        ctx.capT = nowMs();
                        ctx.capSt = CAP_RESP;
                    }
                }
                break;

            default:
                break;
            }
            break;
        }

        } // end switch

        // ============================================================
        //  集中写入共享状态，只持锁一次
        // ============================================================
        xSemaphoreTake(g_mtx, portMAX_DELAY);
        g_sys.currentState = ctx.state;
        g_sys.dx = visionData.dx;
        g_sys.node_flag = visionData.node_flag;
        strncpy(g_sys.obstacle_type, visionData.obstacle_type,
                sizeof(g_sys.obstacle_type) - 1);
        g_sys.obstacle_type[sizeof(g_sys.obstacle_type) - 1] = '\0';
        g_sys.encoder1 = getEncoder1Count();
        g_sys.encoder2 = getEncoder2Count();
        g_sys.obsX = ctx.obs.obsX;
        g_sys.orient = ctx.orient;
        g_sys.roadnum = ctx.roadNum;
        g_sys.roadcount = ctx.roadCount;
        g_sys.wifiConnected = (WiFi.status() == WL_CONNECTED);
        xSemaphoreGive(g_mtx);

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(CONTROL_TASK_PERIOD_MS));
    }
}

// ============================================================
//  MQTTTask
// ============================================================
void MQTTTask(void *pvParameters)
{
    while (1)
    {
        handleMQTTLoop();
        vTaskDelay(pdMS_TO_TICKS(MQTT_TASK_PERIOD_MS));
    }
}

// ============================================================
//  DisplayTask
// ============================================================
void DisplayTask(void *pvParameters)
{
    SysInfo localStatus;

    while (1)
    {
        xSemaphoreTake(g_mtx, portMAX_DELAY);
        memcpy(&localStatus, &g_sys, sizeof(SysInfo));
        xSemaphoreGive(g_mtx);

        u8g2.clearBuffer();
        u8g2.drawFrame(0, 0, 128, 64);
        u8g2.setFont(u8g2_font_6x12_tf);

        u8g2.setCursor(5, 12);
        u8g2.print("State: ");
        switch (localStatus.currentState)
        {
        case STATE_IDLE:
            u8g2.print("IDLE");
            break;
        case STATE_FOLLOW:
            u8g2.print("FOLLOW");
            break;
        case STATE_TURN:
            u8g2.print("TURN");
            break;
        case STATE_ACQUIRE:
            u8g2.print("ACQ");
            break;
        case STATE_WAIT_COMMAND:
            u8g2.print("WAIT_CMD");
            break;
        case STATE_CAPTURE:
            u8g2.print("CAPTURE");
            break;
        default:
            u8g2.print("?");
            break;
        }

        u8g2.setCursor(5, 24);
        u8g2.printf("dx:%d", localStatus.dx);

        u8g2.setCursor(5, 36);
        u8g2.printf("obs:%s x:%d", localStatus.obstacle_type, localStatus.obsX);

        u8g2.setCursor(5, 48);
        u8g2.printf("E1:%ld E2:%ld", localStatus.encoder1, localStatus.encoder2);

        u8g2.setCursor(5, 60);
        u8g2.printf("WiFi:%s", localStatus.wifiConnected ? "OK" : "--");

        u8g2.sendBuffer();

        vTaskDelay(pdMS_TO_TICKS(DISPLAY_TASK_PERIOD_MS));
    }
}

// ============================================================
//  SafetyTask（预留扩展）
// ============================================================
void SafetyTask(void *pvParameters)
{
    while (1)
    {
        /*
         * 预留扩展：
         * 1. 通信超时停车
         * 2. 任务心跳检测
         * 3. 编码器卡死检测
         * 4. 电机保护
         */
        vTaskDelay(pdMS_TO_TICKS(SAFETY_TASK_PERIOD_MS));
    }
}

// ============================================================
//  KeyTask
// ============================================================
void KeyTask(void *pvParameters)
{
    while (1)
    {
        uint8_t key = 0;
        if (xQueueReceive(g_keyQ, &key, portMAX_DELAY) == pdTRUE)
        {
            if (key == 1)
                mqtt_send_position();
            else if (key == 2)
                mqtt_send_position();
        }
    }
}

// ============================================================
//  ISR
// ============================================================
void IRAM_ATTR key1_isr()
{
    uint8_t key = 1;
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(g_keyQ, &key, &woken);
    portYIELD_FROM_ISR(woken);
}

void IRAM_ATTR key2_isr()
{
    uint8_t key = 2;
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(g_keyQ, &key, &woken);
    portYIELD_FROM_ISR(woken);
}
