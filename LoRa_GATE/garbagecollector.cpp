// Copyright [2025] Мальцев Максим Дмитриевич <maksdm007@gmail.com>

#include <garbagecollector.h>
#include <main.h>
#include <settings.h>
#include <Arduino.h>
#include <queue.h>

// Задача сбора мусора (удаление информации о устройствах которые не отвечают)
void garbageCollectorTask(void *pvParameters)
{
  while (1)
  {
    printf("Запущен сбор мусора\n");
    if (xSemaphoreTake(devicesInfoMutex, portMAX_DELAY))
    {
      auto it = devicesInfo.begin();
      while (it != devicesInfo.end())
      {
        uint8_t nowId = it->first;
        uint64_t elapsed_time = millis() - it->second.lastSendTime;
        if (elapsed_time > DATACOL_TIMESTORE)
        {
          if (it->second.device_buf != NULL)
          {
            free(it->second.device_buf);
          }
          it = devicesInfo.erase(it);
          printf("Информация о 0x%X удалена\n", nowId);
        }
        else
        {
          ++it;
        }
      }
      xSemaphoreGive(devicesInfoMutex);
    }

    /*// Вывод информации об устройствах
    if(xSemaphoreTake(devicesInfoMutex, portMAX_DELAY)){
      printf("\n***********DEBUG INFO***********\n");
      printf("Информация о девайсах\n");
      for (auto it : devicesInfo) {
        printf("ID device: %d\n", it.first);
        unsigned long elapsedTime = millis() - it.second.lastSendTime;
        printf("Время последнего сообщения: %lu ms\n", it.second.lastSendTime);
        printf("Время с последнего сообщения: %lu ms\n", elapsedTime);
        printf("Ключи: \n");
        for (auto x : it.second.maskKeys)
          printf("%d\t%s\n", x.first, x.second.c_str());
        printf("\n");
      }
      printf("***********END DEBUG INFO***********\n");
      xSemaphoreGive(devicesInfoMutex);
    }*/

    vTaskDelay(GARBAGE_COLLECT_COOLDOWN / portTICK_PERIOD_MS);
  }
}