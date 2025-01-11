#ifndef WIFI_D_H
#define WIFI_D_H

#include <Arduino.h>
#include <WiFi.h>
#include <queue.h>

#define WIFI_CONNECT_RETRY_COUNT 15
#define WIFI_CONNECT_COOLDOWN 1000

// Очередь для отправки данных на сервер
extern QueueHandle_t wifiSendQueue;

// Задачи
void sendToServerTask(void *pvParameters);
void gatePingTask(void *pvParameters);

// Подключение к Wi-Fi
static void wifiConnect();

// Настройки Wi-Fi
extern IPAddress staticIP;
extern IPAddress gateway;
extern IPAddress mask;

#endif // WIFI_D_H