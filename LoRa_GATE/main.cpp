#define RH_ENABLE_EXPLICIT_RETRY_DEDUP 1

#include "pins.h"
#include <Arduino.h>
#include <string>
#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include <WiFi.h>

#include "main.h"
#include "lora.h"
#include "wifi_d.h"
#include "dataprocess.h"
#include "settings.h"

// Глобальные переменные
std::unordered_map<uint8_t, deviceInfo> devicesInfo;
SemaphoreHandle_t devicesInfoMutex;
//unsigned long int packetcntr = 0;

void setup()  {
  Serial.begin(9600);
  while (!Serial); // Останавливает программу, пока не подключен USB
  printf("\n");

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);

  if (!manager.init())
    printf("LoRa init failed\n");
  else
    printf("LoRa init successful\n");

  driver.setTxPower(20, false);
  driver.setFrequency(869.2);
  driver.setCodingRate4(5);
  driver.setSignalBandwidth(125000);
  driver.setSpreadingFactor(9); 

  driver.setModeRx();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  // Настройка статического IP
  if (!WiFi.config(staticIP, gateway, mask))
    printf("WiFi configuration failed\n");
  
  printf("\nConnecting to WiFi");
  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(WIFICOOLDOWN);
    printf(".");
    if(i > WIFITRUES)
      break;
    i++;
  }
  if(WiFi.status() == WL_CONNECTED) {
    printf("\n");
    printf("Connected. IP address: %s\n", WiFi.localIP().toString().c_str());
    printf("SSID: %s\n", WiFi.SSID().c_str());
  }
  else printf("WiFi not connected\n");

  // Инициализация очередей
  loraReciveQueue = xQueueCreate(10, sizeof(LoRaPacket));
  processQueue = xQueueCreate(10, sizeof(uint8_t) * RH_RF95_MAX_MESSAGE_LEN);
  wifiSendQueue = xQueueCreate(10, sizeof(String));
  loraSendQueue = xQueueCreate(10, sizeof(LoRaPacket));

  // Инициализация семафора
  devicesInfoMutex = xSemaphoreCreateMutex();

  // Создание задач
  xTaskCreatePinnedToCore(
    loraReceiveTask,   // Функция задачи
    "LoRa Receive Task", // Имя задачи
    4096,              // Размер стека
    NULL,              // Параметры задачи
    1,                 // Приоритет
    NULL,              // Дескриптор задачи
    0);                // Ядро

  xTaskCreatePinnedToCore(
    processPackageTask,
    "Process Package Task",
    8192,
    NULL,
    1,
    NULL,
    1);

  xTaskCreatePinnedToCore(
    sendToServerTask,
    "Send To Server Task",
    4096,
    NULL,
    1,
    NULL,
    1); // Отправка на сервер может быть ресурсоемкой, поэтому можем закрепить за другим ядром

  /*xTaskCreatePinnedToCore(
    deviceManagementTask,
    "Device Management Task",
    4096,
    NULL,
    1,
    NULL,
    1);*/

  xTaskCreatePinnedToCore(
    gatePingTask,
    "Gate Ping Task",
    4096,
    NULL,
    1,
    NULL,
    1);

  xTaskCreatePinnedToCore(
    loraSendTask,       // Функция задачи
    "Send Task",    // Имя задачи
    4096,           // Размер стека
    NULL,           // Параметры задачи
    1,              // Приоритет
    NULL,           // Дескриптор задачи
    0);             // Ядро
}

void loop() {
  // Пусто, все работает в задачах FreeRTOS
  vTaskDelay(portMAX_DELAY);
}

/*// Задача управления устройствами (сбор мусора и вывод информации)
void deviceManagementTask(void *pvParameters) { //переписать нормально
  unsigned long prevWatchTime = millis();
  while (1) {
    unsigned long currentTime = millis();
    if (currentTime - prevWatchTime > DATACOL_COOLDOWN) {
      // Сбор мусора
      if(xSemaphoreTake(devicesInfoMutex, portMAX_DELAY)){
        auto it = devicesInfo.begin();
        while (it != devicesInfo.end()) {
          uint8_t nowId = it->first;
          unsigned long elapsedTimeDataCol = currentTime - it->second.lastSendTime;
          if(elapsedTimeDataCol > DATACOL_TIMESTORE) {
            if(it->second.device_buf != NULL){
              free(it->second.device_buf);
            }
            it = devicesInfo.erase(it);
            printf("Информация о 0x%X удалена\n", nowId);
          }
          else {
            ++it;
          }
        }
        xSemaphoreGive(devicesInfoMutex);
      }

      // Вывод информации об устройствах
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
      }

      prevWatchTime = currentTime;
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS); // Проверяем каждую секунду
  }
}*/



