// Copyright [2025] Мальцев Максим Дмитриевич <maksdm007@gmail.com>

#include "headers/pins.h"
#include "headers/lora_d.h"
#include "headers/settings.h"

RH_RF95 driver(LORA_CS, LORA_DIO0);
RHReliableDatagram manager(driver, SERVER_ADDRESS);
QueueHandle_t loraReciveQueue;
QueueHandle_t loraSendQueue;

// Задача приема LoRa-пакетов
void loraReceiveTask(void *pvParameters)
{
  struct loraPacket packet = {0};
  while (1)
  {
    if (manager.available())
    {
      packet.len = sizeof(packet.buf);
      if (manager.recvfromAck(packet.buf, &packet.len, &packet.from))
      {
        if (xQueueSend(loraReciveQueue, &packet, portMAX_DELAY) != pdPASS)
        {
          printf("Failed to send to loraReciveQueue\n");
        }
      }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS); // Задержка на вызов manager.available()
  }
}

// Задача отправки LoRa-пакетов
void loraSendTask(void *pvParameters)
{
  loraPacket data = {0};
  while (1)
  {
    if (xQueueReceive(loraSendQueue, &data, portMAX_DELAY))
    {
      if (!manager.sendtoWait(data.buf, data.len, data.from))
      {
        printf("LoRa sendtoWait failed\n");
      }
    }
  }
}