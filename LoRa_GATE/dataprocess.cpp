// Copyright [2025] Мальцев Максим Дмитриевич <maksdm007@gmail.com>

#include <utility>
#include "headers/lora_d.h"
#include "headers/wifi_d.h"
#include "headers/main.h"
#include "headers/settings.h"
#include "headers/dataprocess.h"
#include <JDE_server.h>
#include <Arduino.h>

// Очередь для отправки данных на сервер
QueueHandle_t processQueue;

// Счетчик полученных пакетов с JSON
uint64_t receivedPacketCounter = 0;
packetInfo lastPacketData = {0};

// Задача обработки пакетов
void processPackageTask(void *pvParameters)
{
  String decoded_json = "";
  struct loraPacket *recv_packet = (struct loraPacket *)malloc(sizeof(struct loraPacket));
  struct loraPacket *send_packet = (struct loraPacket *)malloc(sizeof(struct loraPacket));

  while (1)
  {
    if (xQueueReceive(loraReciveQueue, recv_packet, portMAX_DELAY))
    {
      printf("Получен запрос от 0x%X [%d]: ", recv_packet->from, driver.lastRssi());
      printBuf(recv_packet->buf);

      // Обработка пакета
      send_packet->buf[COMMAND] = recv_packet->buf[COMMAND];
      send_packet->buf[BYTE_COUNT] = START_PAYLOAD;
      send_packet->from = recv_packet->from;

      // Работа с устройствами с защитой семафором
      if (xSemaphoreTake(devicesInfoMutex, portMAX_DELAY))
      {
        deviceInfo &curDeviceInfo = devicesInfo[recv_packet->from];

        switch (recv_packet->buf[COMMAND])
        {
        case REGISTRATION:
          registationKeys(recv_packet->from, recv_packet->buf);
          if (curDeviceInfo.device_buf != NULL)
          {
            memcpy(&recv_packet->buf[START_PAYLOAD - 1], curDeviceInfo.device_buf, sizeof(recv_packet->buf) - 1);
            decoded_json = decodeJsonFromBytes(recv_packet->buf, recv_packet->from, driver.lastRssi());
            printf("\nРаскодированный JSON от 0x%X:\n%s\n", recv_packet->from, decoded_json.c_str());
            free(curDeviceInfo.device_buf);
            curDeviceInfo.device_buf = NULL;
            curDeviceInfo.device_buf_size = 0;

            receivedPacketCounter++;                 // Количество полученных пакетов
            lastPacketData.from = recv_packet->from; // ID устройства
            lastPacketData.rssi = driver.lastRssi(); // RSSI
            getLocalTime(&lastPacketData.date);      // Дата и время

            // Отправляем JSON в очередь отправки на сервер, если есть ключи
            if (xQueueSend(wifiSendQueue, &decoded_json, portMAX_DELAY) != pdPASS)
            {
              printf("Ошибка постановки в wifiSendQueue пакета от 0x%X\n", recv_packet->from);
            }
          }
          break;
        case DATA:
          curDeviceInfo.device_buf_size = 0;
          if (devicesInfo.find(recv_packet->from) != devicesInfo.end() &&
              curDeviceInfo.maskKeys.size() > 0)
          {
            decoded_json = decodeJsonFromBytes(recv_packet->buf, recv_packet->from, driver.lastRssi());
            send_packet->buf[send_packet->buf[BYTE_COUNT]++] = REPLY_TRUE;
            printf("\nРаскодированный JSON от 0x%X:\n%s\n", recv_packet->from, decoded_json.c_str());

            receivedPacketCounter++;                 // Количество полученных пакетов
            lastPacketData.from = recv_packet->from; // ID устройства
            lastPacketData.rssi = driver.lastRssi(); // RSSI
            getLocalTime(&lastPacketData.date);      // Дата и время

            // Отправляем JSON в очередь отправки на сервер, если нет ключей
            if (xQueueSend(wifiSendQueue, &decoded_json, portMAX_DELAY) != pdPASS)
            {
              printf("Ошибка постановки в wifiSendQueue пакета от 0x%X\n", recv_packet->from);
            }
          }
          else
          {
            if (curDeviceInfo.device_buf != NULL)
            {
              free(curDeviceInfo.device_buf);
              curDeviceInfo.device_buf = NULL;
            }
            curDeviceInfo.device_buf_size = recv_packet->buf[BYTE_COUNT] - 1;
            curDeviceInfo.device_buf = reinterpret_cast<uint8_t *>(calloc(curDeviceInfo.device_buf_size, sizeof(uint8_t)));
            if (curDeviceInfo.device_buf != NULL)
            {
              memcpy(curDeviceInfo.device_buf, &recv_packet->buf[COMMAND + 1], curDeviceInfo.device_buf_size);
              printf("Данные от 0x%X сохранены в ожидании ключей\n", recv_packet->from);
            }
            send_packet->buf[send_packet->buf[BYTE_COUNT]++] = REPLY_FALSE;
          }
          break;
        case CLEAR_INFO:
          deleteInfo(recv_packet->from);
          send_packet->buf[send_packet->buf[BYTE_COUNT]++] = REPLY_TRUE;
          break;
        }

        // Обновляем время последнего сообщения для очистки мусора
        if (devicesInfo.find(recv_packet->from) != devicesInfo.end())
        {
          devicesInfo[recv_packet->from].lastSendTime = millis();
        }

        xSemaphoreGive(devicesInfoMutex);
      }

      send_packet->len = send_packet->buf[BYTE_COUNT];

      if (xQueueSend(loraSendQueue, send_packet, portMAX_DELAY) != pdPASS)
      {
        printf("Ошибка постановки ответа для 0x%X в loraSendQueue\n", send_packet->from);
      }

      memset(recv_packet, 0, sizeof(struct loraPacket));
      memset(send_packet, 0, sizeof(struct loraPacket));
    }
  }
  free(recv_packet);
  free(send_packet);
}

void registationKeys(uint8_t from, uint8_t *recv_buf)
{
  if (devicesInfo.find(from) != devicesInfo.end())
    devicesInfo[from].maskKeys.clear();
  char *saveptr;
  char *pch = strtok_r(reinterpret_cast<char *>(recv_buf) + START_PAYLOAD, " ", &saveptr);
  for (uint8_t i = 0; pch != NULL; i++)
  {
    devicesInfo[from].maskKeys.insert(std::make_pair(i, pch));
    pch = strtok_r(NULL, " ", &saveptr);
  }
  for (auto x : devicesInfo[from].maskKeys)
    printf("%d\t%s\n", x.first, x.second.c_str());
}

// Удаление инфрмации об устройстве
void deleteInfo(uint8_t from)
{
  auto it = devicesInfo.find(from);
  if (it != devicesInfo.end())
  {
    if (it->second.device_buf != NULL)
    {
      free(it->second.device_buf);
    }
    devicesInfo.erase(it);
    printf("Информация о 0x%X удалена\n", from);
  }
}
