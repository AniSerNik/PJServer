// Copyright [2025] <>

#include "headers/garbagecollector.h"
#include "headers/main.h"
#include <Arduino.h>
#include "headers/settings.h"

// Задача сбора мусора (удаление информации о устройствах которые не отвечают)
void garbageCollectorTask(void *pvParameters)
{
  while (1)
  {
    if (xSemaphoreTake(devicesInfoMutex, portMAX_DELAY))
    {
      printf("Запущен сбор мусора\n");
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
    vTaskDelay(GARBAGE_COLLECT_COOLDOWN / portTICK_PERIOD_MS);
  }
}