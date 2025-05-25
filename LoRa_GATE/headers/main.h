// Copyright [2025] <>

#ifndef LORA_GATE_HEADERS_MAIN_H_
#define LORA_GATE_HEADERS_MAIN_H_

#include <Arduino.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include <unordered_map>
#include <string>
#include <JDE_server.h>

// Глобальные переменные
extern std::unordered_map<uint8_t, deviceInfo> devicesInfo;
extern SemaphoreHandle_t devicesInfoMutex;
extern SemaphoreHandle_t wifiConnectMutex;

#endif // LORA_GATE_HEADERS_MAIN_H_