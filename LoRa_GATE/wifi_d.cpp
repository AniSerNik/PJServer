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
    printf("\n---Sending info about gate---\n");
    String json;
    json = "{\"system\":{ ";
    json += "\"Akey\":\"" + String(PARAM_Akey) + "\",";
    json += "\"Serial\":\"" + String(PARAM_SerialDevice) + "\", ";
    //json += "\"packetTotal\":\"" + String(packetcntr) + "\", ";
    json += "\"Version\":\"" + String(PARAM_VersionDevice) + "\"}";
    json += "}";
    printf("%s\n", json.c_str());
    // Отправляем JSON в очередь отправки на сервер
    if(xQueueSend(wifiSendQueue, &json, portMAX_DELAY) != pdPASS){
      printf("Failed to send gate ping to serverQueue\n");
    }
    printf("\n---Sending finished---\n");
    
    // Задержка на интервал GATEWORKPING_INTERVAL
    vTaskDelay(GATEWORKPING_INTERVAL / portTICK_PERIOD_MS);
  }
}

// Задача отправки данных на сервер
void sendToServerTask(void *pvParameters) {
  String data;
  while (1) {
    if(xQueueReceive(wifiSendQueue, &data, portMAX_DELAY)) {
      // Проверяем подключение к WiFi
      if (WiFi.status() != WL_CONNECTED) {
        printf("WiFi disconnected. Attempting to reconnect...\n");
        WiFi.begin(ssid, password);
        int i = 0;
        while (WiFi.status() != WL_CONNECTED && i < WIFITRUES) {
          vTaskDelay(WIFICOOLDOWN / portTICK_PERIOD_MS);
          printf(".");
          i++;
        }
        if (WiFi.status() == WL_CONNECTED) {
          printf("\n");
          printf("Reconnected. IP address: %s\n", WiFi.localIP().toString().c_str());
        } else {
          printf("Failed to reconnect to WiFi.\n");
          continue; // Пропускаем отправку данных, если не удалось подключиться
        }
      }

      // Отправляем данные на все серверы
      for (const String& server : servers) {
        WiFiClient client;
        if (WiFi.status() == WL_CONNECTED) {
          HTTPClient http;
          http.begin(client, server.c_str());
          http.addHeader("Content-Type", "application/json");
          int httpResponseCode = http.POST(data);
          printf("HTTP response code: %d\n", httpResponseCode);
          http.end();
        } else {
          printf("WiFi disconnected.\n");
        }
      }
    }
  }
}