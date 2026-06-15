#ifndef VISION_H
#define VISION_H

#include <Arduino.h>

// ===================== 视觉数据包 =====================
typedef struct
{
    int dx;
    int node_flag;
    char obstacle_type[32];
    int obs_x;          // 障碍物 x 坐标 (0~320)，无则为 -1
} VisionData_t;

// ===================== 视觉任务 =====================
void VisionTask(void *pvParameters);

#endif
