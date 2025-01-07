#ifndef WIFI_D_H
#define WIFI_D_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <FreeRTOS.h>
#include <queue.h>

#define WIFITRUES 15
#define WIFICOOLDOWN 500

// Очередь для отправки данных на сервер
extern QueueHandle_t wifiSendQueue;

// Задачи
void sendToServerTask(void *pvParameters);
void gatePingTask(void *pvParameters); 

extern IPAddress staticIP;
extern IPAddress gateway;
extern IPAddress mask;

#endif // WIFI_D_H