#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

extern WiFiClient espClient;
extern PubSubClient mqttClient;
extern char netBuffer[64];

enum Orient
{
    O_NONE,
    O_STRAIGHT,
    O_LEFT,
    O_RIGHT,
    O_ARRIVED,
    O_UTURN
};

enum Action
{
    A_NONE,
    A_PAUSE,
    A_PROCESS,
    A_UTURN,
    A_SETN,
    A_CAPTURE
};

typedef struct
{
    Action action;
    Orient orient;
    int roadnum;
} Cmd_t;

void initMQTT();
void reconnectMQTT();
void handleMQTTLoop();

void mqtt_send_arrive();
void mqtt_send_obstacle(const String &obstacle_type);
void mqtt_send_repaired();
void mqtt_send_info(const String &info_msg);
void mqtt_send_ack(const String &query_type, const String &data);
void mqtt_send_position();

#endif