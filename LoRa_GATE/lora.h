#ifndef LORA_H
#define LORA_H

#include <RH_RF95.h>
#include <RHReliableDatagram.h>
#include <FreeRTOS.h>
#include <queue.h>

// LoRa Packet
struct LoRaPacket {
  uint8_t from;
  uint8_t len;
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
};

// LoRa и менеджер
extern RH_RF95 driver;
extern RHReliableDatagram manager;

void loraReceiveTask(void *pvParameters);
void loraSendTask(void *pvParameters);

extern QueueHandle_t loraQueue;
extern QueueHandle_t sendQueue;

#endif // LORA_H