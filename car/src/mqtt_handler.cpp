#include "mqtt_handler.h"
#include "config.h"
#include "credentials.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

extern QueueHandle_t g_cmdQ;

// CAPTURE 命令参数，ControlTask 读取
char g_capNode[16] = "";
char g_capTs[20] = "";
bool g_capReq = false;

// MQTT_SERVER, MQTT_PORT, MQTT_USERNAME, MQTT_PASSWORD 定义在 credentials.h
const char *MQTT_CLIENT_ID = "esp32_car" STR(CAR_ID) "_001";
const char *MQTT_PUB_TOPIC = "car/" STR(CAR_ID) "/event";
const char *MQTT_SUB_TOPIC = "car/" STR(CAR_ID) "/cmd";

WiFiClient espClient;
PubSubClient mqttClient(espClient);
char netBuffer[64] = "No MQTT Data";

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    String msg = "";
    for (unsigned int i = 0; i < length; i++)
        msg += (char)payload[i];

    msg.substring(0, 60).toCharArray(netBuffer, 64);

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, msg);
    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }

    const char *type = doc["type"] | "";
    const char *param = doc["param"] | "";

    Cmd_t cmd;
    cmd.action = A_NONE;
    cmd.orient = O_NONE;
    cmd.roadnum = 0;

    if (strcmp(type, "ORIENT") == 0)
    {
        // 【修复】只有 ORIENT 消息才写 orient，ACTION 消息不再覆盖方向
        if (strcmp(param, "STRAIGHT") == 0)
            cmd.orient = O_STRAIGHT;
        else if (strcmp(param, "LEFT") == 0)
            cmd.orient = O_LEFT;
        else if (strcmp(param, "RIGHT") == 0)
            cmd.orient = O_RIGHT;
        else if (strcmp(param, "ARRIVED") == 0)
            cmd.orient = O_ARRIVED;
        else if (strcmp(param, "UTURN") == 0)
            cmd.orient = O_UTURN;

        xQueueSend(g_cmdQ, &cmd, 0);
    }
    else if (strcmp(type, "ACTION") == 0)
    {
        // 【修复】ACTION 消息的 cmd.orient 保持 O_NONE，不污染方向状态
        if (strcmp(param, "PAUSE") == 0)
            cmd.action = A_PAUSE;
        else if (strcmp(param, "PROCESS") == 0)
            cmd.action = A_PROCESS;
        else if (strcmp(param, "UTURN") == 0)
            cmd.action = A_UTURN;

        xQueueSend(g_cmdQ, &cmd, 0);
    }
    else if (strcmp(type, "CNT") == 0)
    {
        cmd.roadnum = doc["param"] | 0;
        cmd.action = A_SETN;
        xQueueSend(g_cmdQ, &cmd, 0);
    }
    else if (strcmp(type, "CAPTURE") == 0)
    {
        const char *node = doc["param"] | "";
        const char *ts = doc["ts"] | "";
        strncpy(g_capNode, node, sizeof(g_capNode) - 1);
        g_capNode[sizeof(g_capNode) - 1] = '\0';
        strncpy(g_capTs, ts, sizeof(g_capTs) - 1);
        g_capTs[sizeof(g_capTs) - 1] = '\0';
        g_capReq = true;
        cmd.action = A_CAPTURE;
        xQueueSend(g_cmdQ, &cmd, 0);
    }
}

void initMQTT()
{
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setKeepAlive(30);
}

void reconnectMQTT()
{
    if (mqttClient.connected())
        return;

    static uint32_t lastReconnect = 0;
    if (millis() - lastReconnect < 3000)
        return;

    lastReconnect = millis();
    Serial.println("MQTT reconnecting...");

    bool ok = mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD);
    if (ok)
    {
        strcpy(netBuffer, "MQTT Connected");
        Serial.println(netBuffer);
        if (mqttClient.subscribe(MQTT_SUB_TOPIC))
            strcpy(netBuffer, "Subscribe OK");
        else
            strcpy(netBuffer, "Subscribe Fail");

        mqtt_send_info("esp32 car1 online");
    }
    else
    {
        sprintf(netBuffer, "MQTT Fail:%d", mqttClient.state());
        Serial.println(netBuffer);
    }
}

void handleMQTTLoop()
{
    reconnectMQTT();
    mqttClient.loop();
}

void mqtt_send_arrive()
{
    StaticJsonDocument<128> doc;
    doc["type"] = "ARRIVE";
    doc["param"] = "";
    char output[128];
    serializeJson(doc, output);
    mqttClient.publish(MQTT_PUB_TOPIC, output);
}

void mqtt_send_obstacle(const String &obstacle_type)
{
    StaticJsonDocument<128> doc;
    doc["type"] = "OBSTACLE";
    doc["param"] = obstacle_type.substring(0, 8);
    char output[128];
    serializeJson(doc, output);
    mqttClient.publish(MQTT_PUB_TOPIC, output);
}

void mqtt_send_repaired()
{
    StaticJsonDocument<128> doc;
    doc["type"] = "REPAIRED";
    doc["param"] = "";
    char output[128];
    serializeJson(doc, output);
    mqttClient.publish(MQTT_PUB_TOPIC, output);
}

void mqtt_send_position()
{
    StaticJsonDocument<128> doc;
    doc["type"] = "POSITION";
    doc["param"] = CAR_POSITION;
    char output[128];
    serializeJson(doc, output);
    mqttClient.publish(MQTT_PUB_TOPIC, output);
}

void mqtt_send_info(const String &info_msg)
{
    StaticJsonDocument<128> doc;
    doc["type"] = "INFO";
    doc["param"] = info_msg;
    char output[128];
    serializeJson(doc, output);
    mqttClient.publish(MQTT_PUB_TOPIC, output);
}

void mqtt_send_ack(const String &query_type, const String &data)
{
    // 预留接口，暂不实现
}