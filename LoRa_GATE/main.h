#ifndef MAIN_H
#define MAIN_H

#include <Arduino.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include <unordered_map>
#include <string>

// Структура информации об устройстве
typedef struct deviceInfo {
  std::unordered_map<uint8_t, std::string> maskKeys;
  unsigned long int lastSendTime;
  uint8_t *device_buf;
  size_t device_buf_size;
} deviceInfo;

// Глобальные переменные
extern std::unordered_map<uint8_t, deviceInfo> devicesInfo;
extern uint8_t curDeviceIdHandling;
extern String curDeviceJson;
extern SemaphoreHandle_t devicesInfoMutex;
extern unsigned long int packetcntr;

#endif // MAIN_H