#include "wifi_d.h"
#include "settings.h"
#include <HTTPClient.h>

// Очередь для отправки данных на сервер
QueueHandle_t wifiSendQueue;

// Настройки Wi-Fi
IPAddress staticIP(net_ip);
IPAddress gateway(net_gateway_ip);
IPAddress mask(net_mask);

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
    json += "\"Recived\":\"" + String(packetcntr) + "\",";
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

// Подключение к Wi-Fi
static void wifiConnect()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Настройка статического IP
  // if (!WiFi.config(staticIP, gateway, mask))
  //  printf("Не удалось выставить настроки подключения к Wi-Fi\n");

  printf("Подключение к WiFi\n");
  uint8_t i = 1;
  while (WiFi.status() != WL_CONNECTED)
  {
    vTaskDelay(WIFI_CONNECT_COOLDOWN / portTICK_PERIOD_MS);
    printf("Попытка подключения к Wi-Fi %i/%i\n", i, WIFI_CONNECT_RETRY_COUNT);
    if (i >= WIFI_CONNECT_RETRY_COUNT)
      break;
    i++;
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    printf("\nУспешное подключение к Wi-Fi.\nSSID: %s\nIP адрес: %s\nRSSI: %i\n",
           WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
  }
  else
    printf("Ошибка подключения к WiFi\n");
};