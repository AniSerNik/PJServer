// Copyright [2025] Мальцев Максим Дмитриевич <maksdm007@gmail.com>

#include "headers/wifi_d.h"
#include "headers/settings.h"
#include "headers/main.h"
#include "headers/dataprocess.h"
#include <HTTPClient.h>

// Очередь для отправки данных на сервер
QueueHandle_t wifiSendQueue;

// Мьютекс для защиты функции wifiConnect
extern SemaphoreHandle_t wifiConnectMutex;

// Структура для хранения информации о времени
struct tm timeinfo;

// Задача периодической отправки пинга шлюза
void gatePingTask(void *pvParameters)
{
  while (1)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      wifiConnect();
    }

    printf("\nОтправка информации о шлюзе\n");
    String json;
    json = "{\"system\":{";
    json += "\"Akey\":\"" + String(PARAM_Akey) + "\",";
    json += "\"Serial\":\"" + String(PARAM_SerialDevice) + "\",";
    json += "\"Version\":\"" + String(PARAM_VersionDevice) + "\",";
    json += "\"Recived\":\"" + String(receivedPacketCounter) + "\",";
    json += "\"RSSI\":\"" + String(WiFi.RSSI()) + "\"}";
    json += "}";
    printf("JSON информация о шлюзе:\n%s\n", json.c_str());

    if (xQueueSend(wifiSendQueue, &json, portMAX_DELAY) != pdPASS)
    {
      printf("Ошибка постановки в wifiSendQueue информации о шлюзе\n");
    }

    // Задержка на интервал GATEWORKPING_INTERVAL
    vTaskDelay(GATEWORKPING_INTERVAL / portTICK_PERIOD_MS);
  }
}

// Задача отправки данных на сервер
void sendToServerTask(void *pvParameters)
{
  String json;
  while (1)
  {
    if (xQueueReceive(wifiSendQueue, &json, portMAX_DELAY))
    {
      if (WiFi.status() != WL_CONNECTED)
      {
        wifiConnect();
      }

      // Отправляем данные на все серверы
      for (const String &server : servers)
      {
        WiFiClient client;
        if (WiFi.status() == WL_CONNECTED)
        {
          HTTPClient http;
          http.begin(client, server.c_str());
          http.addHeader("Content-Type", "application/json");
          printf("%s:\n   Ответ HTTP: %i\n", server.c_str(), http.POST(json));
          http.end();
        }
        else
        {
          printf("%s:\nНет Wi-Fi подключения. Данные не переданы.\n", server.c_str());
        }
      }
    }
  }
}

// Задача периодической синхронизации времени
void timeSyncTask(void *pvParameters)
{
  while (1)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      wifiConnect();
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      uint8_t retry_count = 1U;
      while (retry_count <= TIME_SYNC_RETRY_COUNT)
      {
        configTime(UTC_OFFSET, 0U, net_ntp.c_str());
        if (!getLocalTime(&timeinfo))
        {
          printf("Не удалось получить текущее время %u/%u\n", retry_count, TIME_SYNC_RETRY_COUNT);
          retry_count++;
          vTaskDelay(TIME_SYNC_DELAY / portTICK_PERIOD_MS);
          continue;
        }
        else
        {
          break;
        }
      }
      char timeStr[53];
      strftime(timeStr, sizeof(timeStr), "%d-%m-%Y %H:%M:%S", &timeinfo);
      printf("Синхронизировано задачей: %s\n", timeStr);
    }
    else
    {
      printf("Ошибка синхронизации времени, нет WiFi подключения\n");
    }
    vTaskDelay(TIME_SYNC_INTERVAL / portTICK_PERIOD_MS);
  }
}

// Подключение к Wi-Fi
static void wifiConnect()
{
  if (xSemaphoreTake(wifiConnectMutex, portMAX_DELAY) == pdTRUE)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      WiFi.mode(WIFI_STA);

      uint8_t bestNetworkIndex = -1;
      int16_t bestRSSI = -256; // минимальное значение RSSI

      for (int i = 0; i < MAX_WIFI_NETWORKS; i++)
      {
        if (wifiNetworks[i].ssid.length() == 0)
        {
          continue;
        }

        // Настройка статического IP, если указано
        if (wifiNetworks[i].useStaticSettings)
        {
          if (!WiFi.config(IPAddress(wifiNetworks[i].net_ip), IPAddress(wifiNetworks[i].net_gateway_ip), IPAddress(wifiNetworks[i].net_mask)))
          {
            printf("Не удалось выставить настройки подключения к Wi-Fi для сети %s\n", wifiNetworks[i].ssid.c_str());
          }
        }

        WiFi.begin(wifiNetworks[i].ssid.c_str(), wifiNetworks[i].password.c_str());

        printf("Подключение к WiFi сети \"%s\"\n", wifiNetworks[i].ssid.c_str());
        uint8_t retry_count = 0;
        while (WiFi.status() != WL_CONNECTED && retry_count < WIFI_CONNECT_RETRY_COUNT)
        {
          vTaskDelay(WIFI_CONNECT_COOLDOWN / portTICK_PERIOD_MS);
          printf("Попытка подключения к Wi-Fi %i/%i\n", retry_count + 1, WIFI_CONNECT_RETRY_COUNT);
          retry_count++;
        }

        if (WiFi.status() == WL_CONNECTED)
        {
          int rssi = WiFi.RSSI();
          printf("Успешно подключение к WiFi сети \"%s\", RSSI: %d\n", wifiNetworks[i].ssid.c_str(), rssi);
          if (rssi > bestRSSI)
          {
            bestRSSI = rssi;
            bestNetworkIndex = i;
          }
          WiFi.disconnect();
        }
        else
        {
          printf("Ошибка подключения к WiFi сети \"%s\"\n", wifiNetworks[i].ssid.c_str());
        }
      }

      if (bestNetworkIndex != -1)
      {
        vTaskDelay(WIFI_CONNECT_COOLDOWN * 3 / portTICK_PERIOD_MS);
        // Настройка статического IP, если указано
        if (wifiNetworks[bestNetworkIndex].useStaticSettings)
        {
          if (!WiFi.config(IPAddress(wifiNetworks[bestNetworkIndex].net_ip), IPAddress(wifiNetworks[bestNetworkIndex].net_gateway_ip), IPAddress(wifiNetworks[bestNetworkIndex].net_mask)))
          {
            printf("Не удалось выставить настройки подключения к Wi-Fi сети с лучшим RSSI \"%s\"\n", wifiNetworks[bestNetworkIndex].ssid.c_str());
          }
        }

        WiFi.begin(wifiNetworks[bestNetworkIndex].ssid.c_str(), wifiNetworks[bestNetworkIndex].password.c_str());

        printf("Подключение к WiFi сети \"%s\"\n", wifiNetworks[bestNetworkIndex].ssid.c_str());
        uint8_t retry_count = 0;
        while (WiFi.status() != WL_CONNECTED && retry_count < WIFI_CONNECT_RETRY_COUNT)
        {
          vTaskDelay(WIFI_CONNECT_COOLDOWN / portTICK_PERIOD_MS);
          printf("Попытка подключения к Wi-Fi сети с лучшим RSSI %i/%i\n", retry_count + 1, WIFI_CONNECT_RETRY_COUNT);
          retry_count++;
        }

        if (WiFi.status() == WL_CONNECTED)
        {
          printf("\nПодключено к сети с лучшим RSSI \"%s\"\n", wifiNetworks[bestNetworkIndex].ssid.c_str());
          printf("IP адрес: %s\nRSSI: %i\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());

          // Синхронизация времени при подключении к Wi-Fi
          uint8_t time_retry_count = 1U;
          while (time_retry_count <= TIME_SYNC_RETRY_COUNT)
          {
            configTime(UTC_OFFSET, 0U, net_ntp.c_str());
            if (!getLocalTime(&timeinfo))
            {
              printf("Не удалось получить текущее время %u/%u\n", time_retry_count, TIME_SYNC_RETRY_COUNT);
              time_retry_count++;
              vTaskDelay(TIME_SYNC_DELAY / portTICK_PERIOD_MS);
              continue;
            }
            else
            {
              char timeStr[53];
              strftime(timeStr, sizeof(timeStr), "%d-%m-%Y %H:%M:%S", &timeinfo);
              printf("Синхронизировано при подключении: %s\n", timeStr);
              break;
            }
          }
        }
        else
        {
          printf("Ошибка подключения к сети с лучшим RSSI \"%s\"\n", wifiNetworks[bestNetworkIndex].ssid.c_str());
        }
      }
      else
      {
        printf("Не удалось найти подходящую сеть для подключения\n");
      }
    }
    xSemaphoreGive(wifiConnectMutex);
  }
}