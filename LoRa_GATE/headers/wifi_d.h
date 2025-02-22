// Copyright [2025] Мальцев Максим Дмитриевич <maksdm007@gmail.com>

#ifndef LORA_GATE_HEADERS_WIFI_D_H_
#define LORA_GATE_HEADERS_WIFI_D_H_

#include <Arduino.h>
#include <WiFi.h>
#include <queue.h>

// Очередь для отправки данных на сервер
extern QueueHandle_t wifiSendQueue;

// Задачи
void sendToServerTask(void *pvParameters);
void gatePingTask(void *pvParameters);
void timeSyncTask(void *pvParameters);
void webServerTask(void *pvParameters);

// Веб-сервер
static void handleRoot();
static void handleSaveSettings();
static void handleClearNVS();
static void handleNotFound();
static void handleRestart();

// Подключение к Wi-Fi
static void wifiConnect();

// Структура для хранения информации о времени
extern struct tm timeinfo;

#endif // LORA_GATE_HEADERS_WIFI_D_H_