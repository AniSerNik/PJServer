#ifndef WIFI_D_H
#define WIFI_D_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <FreeRTOS.h>
#include <queue.h>

// Очередь для отправки данных на сервер
extern QueueHandle_t serverQueue;

// Настройки Wi-Fi
extern IPAddress staticIP;
extern IPAddress gateway;
extern IPAddress mask;

void sendToServerTask(void *pvParameters);
void gatePingTask(void *pvParameters);

#endif // WIFI_D_H