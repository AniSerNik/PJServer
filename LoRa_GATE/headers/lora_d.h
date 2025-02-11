// Copyright [2025] Мальцев Максим Дмитриевич <maksdm007@gmail.com>

#ifndef LORA_GATE_HEADERS_LORA_D_H_
#define LORA_GATE_HEADERS_LORA_D_H_

#include <RH_RF95.h>
#include <RHReliableDatagram.h>
#include <queue.h>

// Наш LoRa Packet
struct loraPacket
{
  uint8_t from;                         // id устройства в сети
  uint8_t len;                          // Длина буфрера
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN]; // Буфер для данных
};

// Для библиотеки LoRa RadioHead
extern RH_RF95 driver;
extern RHReliableDatagram manager;

// Задачи
void loraReceiveTask(void *pvParameters); // Получение пакетов
void loraSendTask(void *pvParameters);    // Отправка пакетов

// Очереди
extern QueueHandle_t loraReciveQueue; // Полученные пакеты
extern QueueHandle_t loraSendQueue;   // Пакеты на отправку

#endif // LORA_GATE_HEADERS_LORA_D_H_