#include "pins.h"
#include "lora.h"
#include "settings.h"

// Инициализация глобальных переменных
RH_RF95 driver(LORA_CS, LORA_DIO0);
RHReliableDatagram manager(driver, SERVER_ADDRESS);
QueueHandle_t loraQueue;
QueueHandle_t sendQueue;

// Задача приема LoRa-пакетов
void loraReceiveTask(void *pvParameters) {
  while (1) {
    if (manager.available()) {
      struct LoRaPacket packet = {0};
      packet.len = sizeof(packet.buf); 
      if (manager.recvfromAck(packet.buf, &packet.len, &packet.from)) {

        // Отправляем пакет в очередь обработки
        if(xQueueSend(loraQueue, &packet, portMAX_DELAY) != pdPASS){
          printf("Failed to send to loraQueue\n");
        }
        //packetcntr++;
      }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS); // Немного задержки для снижения нагрузки
  }
}

// Задача отправки данных
void loraSendTask(void *pvParameters) {
  LoRaPacket data = {0};
  while (1) {
    if (xQueueReceive(sendQueue, &data, portMAX_DELAY)) {
      printf("LoRa_sendTask\n");
      // Отправка ответа
      if (!manager.sendtoWait(data.buf, data.len, data.from)) {
        printf("sendtoWait failed\n");
      }
    }
  }
}