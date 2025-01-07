#ifndef LORA_H
#define LORA_H

#include <RH_RF95.h>
#include <RHReliableDatagram.h>
#include <FreeRTOS.h>
#include <queue.h>

// Наш LoRa Packet
struct LoRaPacket {
  uint8_t from; // id устройства в сети
  uint8_t len; // Длина буфрера
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN]; // Буфер для данных
};

// Для библиотеки LoRa RadioHead
extern RH_RF95 driver;
extern RHReliableDatagram manager;

// Задачи
void loraReceiveTask(void *pvParameters); // Получение пакетов
void loraSendTask(void *pvParameters); // Отправка пакетов

// Очереди
extern QueueHandle_t loraReciveQueue; // Полученные пакеты
extern QueueHandle_t loraSendQueue; // Пакеты на отправку

#endif // LORA_H