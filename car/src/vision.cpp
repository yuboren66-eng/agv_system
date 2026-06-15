#include "vision.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// 由 main.cpp 定义，视觉帧输出队列
extern QueueHandle_t g_visQ;
extern volatile bool g_capMode;

// ============================================================
//  手写解析：格式 "<dx>,<node_flag>,<obstacle_type>"
//  比 sscanf 快约 10 倍（~2μs vs ~20μs）
// ============================================================
static bool fast_parse_vision(const char *line, VisionData_t *out)
{
    if (!line || !out)
        return false;

    // 字段1：dx
    char *endPtr = nullptr;
    out->dx = (int)strtol(line, &endPtr, 10);
    if (!endPtr || *endPtr != ',')
        return false;

    // 字段2：node_flag
    const char *p = endPtr + 1;
    out->node_flag = (int)strtol(p, &endPtr, 10);
    if (!endPtr)
        return false;

    // 字段3：obstacle_type[:x]（可选，缺省填 "NONE"）
    // 格式：纯类型如 "none" 或 带坐标如 "dog:160"
    out->obs_x = -1;
    if (*endPtr == ',')
    {
        p = endPtr + 1;
        size_t len = strlen(p);
        if (len == 0 || len >= sizeof(out->obstacle_type))
        {
            strncpy(out->obstacle_type, "NONE", sizeof(out->obstacle_type));
        }
        else
        {
            // 查找冒号，拆分类型和 x 坐标
            char buf[32];
            strncpy(buf, p, sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            char *colon = strchr(buf, ':');
            if (colon)
            {
                *colon = '\0';
                strncpy(out->obstacle_type, buf, sizeof(out->obstacle_type) - 1);
                out->obstacle_type[sizeof(out->obstacle_type) - 1] = '\0';
                out->obs_x = atoi(colon + 1);
            }
            else
            {
                strncpy(out->obstacle_type, buf, sizeof(out->obstacle_type) - 1);
                out->obstacle_type[sizeof(out->obstacle_type) - 1] = '\0';
            }
        }
    }
    else
    {
        strncpy(out->obstacle_type, "NONE", sizeof(out->obstacle_type));
    }

    return true;
}

// ============================================================
//  VisionTask
//  - 从 K230 串口接收视觉数据，解析后入队
//  - 块读 + Overwrite 策略：队列满时丢弃最旧帧
// ============================================================
void VisionTask(void *pvParameters)
{
    char rxBuffer[VISION_RX_BUFFER_SIZE];
    int rxIndex = 0;
    VisionData_t visionData;

    while (1)
    {
        // 拍照模式暂停读串口，让 STATE_CAPTURE 独占
        if (g_capMode)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        int avail = Serial1.available();
        if (avail <= 0)
        {
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }

        // 块读，减少调度次数
        uint8_t tmp[VISION_BLOCK_READ_SIZE];
        int toRead = (avail > VISION_BLOCK_READ_SIZE) ? VISION_BLOCK_READ_SIZE : avail;
        int got = Serial1.readBytes(tmp, toRead);

        for (int i = 0; i < got; i++)
        {
            char ch = (char)tmp[i];

            if (ch == '\n' || ch == '\r')
            {
                if (rxIndex > 0)
                {
                    rxBuffer[rxIndex] = '\0';
                    rxIndex = 0;

                    if (fast_parse_vision(rxBuffer, &visionData))
                    {
                        // Overwrite 策略：队列满则丢最旧帧，保留最新帧
                        if (xQueueSend(g_visQ, &visionData, 0) != pdTRUE)
                        {
                            VisionData_t dummy;
                            xQueueReceive(g_visQ, &dummy, 0);
                            xQueueSend(g_visQ, &visionData, 0);
                        }
                    }
                }
            }
            else
            {
                if (rxIndex < (int)sizeof(rxBuffer) - 1)
                    rxBuffer[rxIndex++] = ch;
                else
                    rxIndex = 0; // 帧过长，丢弃重新对齐
            }
        }
    }
}
