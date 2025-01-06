#include <HTTPClient.h>
#include "settings.h"
#include "wifi_d.h"

// Очередь для отправки данных на сервер
QueueHandle_t serverQueue;

// Настройки Wi-Fi
IPAddress staticIP(192,168,0,95);
IPAddress gateway(192,168,0,1);
IPAddress mask(255,255,255,0);

// Глобальные переменные для адресов серверов
const char* server_addresses[] = {
  "http://robo.itkm.ru/core/jsonapp.php",
  "http://46.254.216.214/core/jsonapp.php",
  "http://dbrobo1.mf.bmstu.ru/core/jsonapp.php"
};

// Задача периодической отправки пинга шлюза
void gatePingTask(void *pvParameters) { //переписать нормально время
  unsigned long prevTimeGateWorking = 0;
  while (1) {
    unsigned long currentTime = millis();
    if(prevTimeGateWorking == 0 || (currentTime - prevTimeGateWorking) > GATEWORKPING_INTERVAL) {
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
      if(xQueueSend(serverQueue, &json, portMAX_DELAY) != pdPASS){
        printf("Failed to send gate ping to serverQueue\n");
      }
      printf("\n---Sending finished---\n"); //не корректно
      prevTimeGateWorking = currentTime;
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS); // Проверяем каждую секунду
  }
}

// Задача отправки данных на сервер
void sendToServerTask(void *pvParameters) {
  String data;
  while (1) {
    if(xQueueReceive(serverQueue, &data, portMAX_DELAY)) {
      // Проверяем подключение к WiFi
      if (WiFi.status() != WL_CONNECTED) {
        printf("WiFi disconnected. Attempting to reconnect...\n");
        WiFi.begin(ssid, password);
        int i = 0;
        while (WiFi.status() != WL_CONNECTED && i < WIFITRUES) {
          delay(WIFICOOLDOWN);
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
      for (const char* server_addr : server_addresses) {
        WiFiClient client;
        if (WiFi.status() == WL_CONNECTED) {
          HTTPClient http;
          http.begin(client, server_addr);
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