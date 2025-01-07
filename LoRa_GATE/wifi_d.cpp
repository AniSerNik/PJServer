#include "wifi_d.h"
#include "settings.h"
#include <HTTPClient.h>

// Очередь для отправки данных на сервер
QueueHandle_t wifiSendQueue;

IPAddress staticIP(net_ip);
IPAddress gateway(net_gateway_ip);
IPAddress mask(net_mask);

// Задача периодической отправки пинга шлюза
void gatePingTask(void *pvParameters) {
  while (1) {
    printf("\n--- Отправка информации о шлюзе ---\n");
    String json;
    json = "{\"system\":{ ";
    json += "\"Akey\":\"" + String(PARAM_Akey) + "\",";
    json += "\"Serial\":\"" + String(PARAM_SerialDevice) + "\", ";
    json += "\"Recived\":\"" + String(packetcntr) + "\", ";
    json += "\"Version\":\"" + String(PARAM_VersionDevice) + "\"}";
    json += "}";
    printf("%s\n", json.c_str());
    // Отправляем JSON в очередь отправки на сервер
    if(xQueueSend(wifiSendQueue, &json, portMAX_DELAY) != pdPASS){
      printf("Ошибка постановки в wifiSendQueue информации о шлюзе\n");
    }
    printf("\n--- Отправка информации о шлюзе завершена ---\n");
    
    // Задержка на интервал GATEWORKPING_INTERVAL
    vTaskDelay(GATEWORKPING_INTERVAL / portTICK_PERIOD_MS);
  }
}

// Задача отправки данных на сервер
void sendToServerTask(void *pvParameters) {
  String json;
  while (1) {
    if(xQueueReceive(wifiSendQueue, &json, portMAX_DELAY)) {
      // Подключение к WiFi
      if (WiFi.status() != WL_CONNECTED) {
          WiFi.mode(WIFI_STA);
          WiFi.begin(ssid, password);
          // Настройка статического IP
          if (!WiFi.config(staticIP, gateway, mask))
            printf("Не удалось выставить настроки подключения к Wi-Fi\n");
          
          printf("Подключение к WiFi\n");
          uint8_t i = 0;
          while (WiFi.status() != WL_CONNECTED) {
            vTaskDelay(WIFI_CONNECT_COOLDOWN / portTICK_PERIOD_MS);
            printf("Попытка подключения к Wi-Fi %i/%i\n", i + 1, WIFI_CONNECT_RETRY_COUNT);
            if(i > WIFI_CONNECT_RETRY_COUNT)
              break;
            i++;
          }
          if(WiFi.status() == WL_CONNECTED) {
            printf("\nУспешное подключение к Wi-Fi.\nSSID: %s\nIP адрес: %s\n\n",
                   WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
          }
          else printf("Ошибка подключения к WiFi\n");
      }

      // Отправляем данные на все серверы
      for (const String& server : servers) {
        WiFiClient client;
        if (WiFi.status() == WL_CONNECTED) {
          HTTPClient http;
          http.begin(client, server.c_str());
          http.addHeader("Content-Type", "application/json");
          printf("%s:\n   Ответ HTTP: %i\n", server.c_str(), http.POST(json));
          http.end();
        } else {
          printf("%s:\nНет Wi-Fi подключения. Данные не переданы.\n", server.c_str());
        }
      }
    }
  }
}