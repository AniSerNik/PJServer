// Copyright [2025] Мальцев Максим Дмитриевич <maksdm007@gmail.com>

#ifndef LORA_GATE_HEADERS_MAIN_H_
#define LORA_GATE_HEADERS_MAIN_H_

#include <Arduino.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include <unordered_map>
#include <string>

// Структура информации об устройстве
typedef struct deviceInfo
{
  std::unordered_map<uint8_t, std::string> maskKeys;
  uint64_t lastSendTime;
  uint8_t *device_buf;
  size_t device_buf_size;
} deviceInfo;

// Глобальные переменные
extern std::unordered_map<uint8_t, deviceInfo> devicesInfo;
extern SemaphoreHandle_t devicesInfoMutex;
extern SemaphoreHandle_t wifiConnectMutex;

#endif // LORA_GATE_HEADERS_MAIN_H_