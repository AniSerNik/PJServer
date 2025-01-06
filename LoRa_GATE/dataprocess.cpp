#include "dataprocess.h"
#include "lora.h"
#include "wifi_d.h"
#include "main.h"
#include "settings.h"
#include <Arduino.h>

// Очередь для отправки данных на сервер
QueueHandle_t processQueue;

// Задача обработки пакетов
void processPackageTask(void *pvParameters) {
  struct LoRaPacket packet = {0};
  // Буферы
  uint8_t send_buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t recv_buf[RH_RF95_MAX_MESSAGE_LEN];

  while (1) {
    if(xQueueReceive(loraQueue, &packet, portMAX_DELAY)) {
      uint8_t from = packet.from; // Предполагается, что первый байт - ID устройства
      curDeviceIdHandling = from;
      printf("Получен запрос от 0x%X [%d]: ", from, driver.lastRssi());
      printBuf(packet.buf);
      
      memccpy(recv_buf, packet.buf, 0, sizeof(packet.buf));

      // Обработка пакета
      send_buf[COMMAND] = packet.buf[COMMAND];
      send_buf[BYTE_COUNT] = START_PAYLOAD;

      // Работа с устройствами с защитой семафором
      if(xSemaphoreTake(devicesInfoMutex, portMAX_DELAY)){
        deviceInfo &curDeviceInfo = devicesInfo[from];

        switch (packet.buf[COMMAND]) {
          case REGISTRATION:
            registationKeys(from, recv_buf);
            if(curDeviceInfo.device_buf != NULL) {
              memcpy(&recv_buf[START_PAYLOAD - 1], curDeviceInfo.device_buf, sizeof(recv_buf) - 1);
              curDeviceJson = decodeJsonFromBytes(recv_buf, from); //Проблема тута
              printf("%s\n", curDeviceJson.c_str());
              free(curDeviceInfo.device_buf);
              curDeviceInfo.device_buf = NULL;
              curDeviceInfo.device_buf_size = 0;
              // Отправляем JSON в очередь отправки на сервер, если есть ключи
              if(xQueueSend(serverQueue, &curDeviceJson, portMAX_DELAY) != pdPASS){
                printf("Failed to send to serverQueue\n");
              }
            }
            break;
          case DATA:
            curDeviceInfo.device_buf_size = 0;

            if(devicesInfo.find(from) != devicesInfo.end() &&
               curDeviceInfo.maskKeys.size() > 0) {
              curDeviceJson = decodeJsonFromBytes(recv_buf, from);
              send_buf[send_buf[BYTE_COUNT]++] = REPLY_TRUE;
              printf("%s\n", curDeviceJson.c_str());
              // Отправляем JSON в очередь отправки на сервер, если нет ключей
              if(xQueueSend(serverQueue, &curDeviceJson, portMAX_DELAY) != pdPASS){
                printf("Failed to send to serverQueue\n");
              }
            }
            else {
              if(curDeviceInfo.device_buf != NULL) {
                free(curDeviceInfo.device_buf);
                curDeviceInfo.device_buf = NULL;
              }
              curDeviceInfo.device_buf_size = packet.buf[BYTE_COUNT] - 1;
              printf("%d\n", curDeviceInfo.device_buf_size);
              curDeviceInfo.device_buf = (uint8_t*)calloc(curDeviceInfo.device_buf_size, sizeof(uint8_t));
              if(curDeviceInfo.device_buf != NULL) {
                memcpy(curDeviceInfo.device_buf, &packet.buf[COMMAND + 1], curDeviceInfo.device_buf_size);
                printf("Данные сохранены в ожидании ключей\n");
              }
              send_buf[send_buf[BYTE_COUNT]++] = REPLY_FALSE;
            }
            break;
          case CLEAR_INFO:
            deleteInfo(from);
            send_buf[send_buf[BYTE_COUNT]++] = REPLY_TRUE;
            break;
        }

        // Обновляем время последнего сообщения
        if(devicesInfo.find(from) != devicesInfo.end()) {
          devicesInfo[from].lastSendTime = millis();
        }

        xSemaphoreGive(devicesInfoMutex);
      }

      // Пример добавления данных в очередь для отправки
      LoRaPacket data = {0};
      // Заполнение данных
      memcpy(data.buf, send_buf, RH_RF95_MAX_MESSAGE_LEN);
      data.len = send_buf[BYTE_COUNT];
      data.from = from;

      if (xQueueSend(sendQueue, &data, portMAX_DELAY) != pdPASS) {
        printf("Failed to send to sendQueue\n");
      }

      // Очистка буфера отправки
      memset(send_buf, 0, sizeof(send_buf));
    }
  }
}

// Функция регистрации ключей
void registationKeys(uint8_t from, uint8_t *recv_buf) {
  if(devicesInfo.find(from) != devicesInfo.end())
    devicesInfo[from].maskKeys.clear();
  char *pch = strtok((char *)recv_buf, " ");
  pch = strtok(NULL, " "); // Пропускаем первый токен, если он нужен
  if(pch == NULL){
    printf("No keys received for registration\n");
    return;
  }
  for (uint8_t i = 0; pch != NULL; i++) {
    devicesInfo[from].maskKeys.insert(std::make_pair(i, pch));
    pch = strtok(NULL, " ");
  }
  for (auto x : devicesInfo[from].maskKeys)
    printf("%d\t%s\n", x.first, x.second.c_str());
}

// Функция удаления информации об устройстве
void deleteInfo(uint8_t from) {
  auto it = devicesInfo.find(from);
  if(it != devicesInfo.end()) {
    if(it->second.device_buf != NULL){
      free(it->second.device_buf);
    }
    devicesInfo.erase(it);
    printf("Информация о 0x%X удалена\n", from);
  }
}

// Получение ключа по ID
String getKeyFromId(uint8_t idKey, uint8_t dev_id) {
  //if(xSemaphoreTake(devicesInfoMutex, portMAX_DELAY)){
    auto it = devicesInfo.find(dev_id);
    if(it != devicesInfo.end()){
      auto res = it->second.maskKeys.find(idKey);
      if (res != it->second.maskKeys.end()) {
        //xSemaphoreGive(devicesInfoMutex);
        return String(res->second.c_str());
      }
    }
    //xSemaphoreGive(devicesInfoMutex);
  //}
  return "NotRegisteredKey";
}

// Декодирование JSON из байтов
String decodeJsonFromBytes(uint8_t *recv_buf, uint8_t dev_id) {
  uint8_t nestedObject[JSON_MAX_NESTEDOBJECT], nesObjCount = 0;
  String result = "{";
  for (int i = START_PAYLOAD; i < recv_buf[BYTE_COUNT];) {
    uint8_t key_id = recv_buf[i++];
    if (key_id == JSON_LEVEL_UP) {
      if(nesObjCount == 0) {
        printf("Error with nested object!\n");
        break;
      }
      nesObjCount--;
      if(nestedObject[nesObjCount] == JSON_OBJECT)
        result += '}';
      else if(nestedObject[nesObjCount] == JSON_ARRAY)
        result += ']';
      if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
        result += ", ";
      continue;
    }
    uint8_t tag;
    if(nesObjCount > 0 && nestedObject[nesObjCount - 1] == JSON_ARRAY)
      tag = key_id;
    else {
      result += '\"' + getKeyFromId(key_id, dev_id) + '\"' + ": ";
      tag = recv_buf[i++];
    }
    switch (tag & GENERAL_TYPE_MASK) {
      case SPECIFIC: {
        switch (tag & SPECIFIC_TYPE_MASK) {
          case JSON_OBJECT: {
            result += '{';
            if(nesObjCount == JSON_MAX_NESTEDOBJECT)
              printf("Error. Max nested object reach\n");
            nestedObject[nesObjCount] = JSON_OBJECT;
            nesObjCount++;
            continue;
          }
          case JSON_ARRAY: {
            result += '[';
            if(nesObjCount == JSON_MAX_NESTEDOBJECT)
              printf("Error. Max nested object reach\n");
            nestedObject[nesObjCount] = JSON_ARRAY;
            nesObjCount++;
            continue;
          }
          case IP_ADDR: {
            uint8_t ip[4];
            char ip_str[16];
            for(int j = 0; j < 4; j++)
              ip[j] = recv_buf[i++];
            sprintf(ip_str, "%hhu.%hhu.%hhu.%hhu", ip[0], ip[1], ip[2], ip[3]);
            result += '\"' + String(ip_str) + '\"';
            if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
              result += ", ";
            continue;
          }
          case MAC_ADDR: {
            uint8_t mac[6];
            char mac_str[18];
            for(int j = 0; j < 6; j++)
              mac[j] = recv_buf[i++];
            sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            result += '\"' + String(mac_str) + '\"';
            if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
              result += ", ";
            continue;
          }
          case DATE_YYYYMMDD: {
            uint16_t year = 0;
            year |= recv_buf[i++];
            year |= recv_buf[i++] << 8;
            uint8_t mon = recv_buf[i++];
            uint8_t day = recv_buf[i++];
            char date[11];
            sprintf(date, "%d-%02hhu-%02hhu", year, mon, day);
            result += '\"' + String(date) + '\"';
            if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
              result += ", ";
            continue;
          }
          case TIME_HHMMSS: {
            uint8_t hour = recv_buf[i++];
            uint8_t min = recv_buf[i++];
            uint8_t sec = recv_buf[i++];
            char time[11];
            sprintf(time, "%02hhu:%02hhu:%02hhu", hour, min, sec);

            result += '\"' + String(time) + '\"';
            if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
              result += ", ";
            continue;
          }
        }
      }
      case STRING: {
        char unpack_buf_cstr[JSON_MAX_LEN_FIELD];
        strcpy(unpack_buf_cstr, (char*)&recv_buf[i]);
        i += strlen((char*)&recv_buf[i]) + 1;
        result += '\"' + String(unpack_buf_cstr) + '\"';
        if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
          result += ", ";
        continue;
      }
      case INTEGER: {
        switch (tag & NUMBER_SIZE_MASK) {
          case _8BIT: {
            int8_t num = (int8_t)recv_buf[i++];
            result += String(num);
            if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
              result += ", ";
            continue;
          }
          case _16BIT: {
            int16_t num = 0;
            num |= recv_buf[i++];
            num |= recv_buf[i++] << 8;
            result += String(num);
            if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
              result += ", ";
            continue;
          }
          case _32BIT: {
            int32_t num = 0;
            for(int j = 0; j < 32; j += 8)
              num |= recv_buf[i++] << j;
            result += String(num);
            if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
              result += ", ";
            continue;
          }
        }
      }
      case CUSTOM_FLOAT: {
        switch (tag & NUMBER_SIZE_MASK) {
          case _8BIT: {
            int8_t num = (int8_t)recv_buf[i++];
            uint8_t dec_places = tag & DECIMAL_POINT_MASK;
            int8_t integer = num / quick_pow10(dec_places);
            uint8_t fraction = abs(num) % quick_pow10(dec_places);
            char num_str[18];
            sprintf(num_str, "%d.%0*d", integer, dec_places, fraction);
            if (integer == 0 && num < 0)
              result += '-';
            result += String(num_str);
            if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
              result += ", ";
            continue;
          }
          case _16BIT: {
            int16_t num = 0;
            num |= recv_buf[i++];
            num |= recv_buf[i++] << 8;
            uint8_t dec_places = tag & DECIMAL_POINT_MASK;
            int16_t integer = num / quick_pow10(dec_places);
            uint16_t fraction = abs(num) % quick_pow10(dec_places);
            char num_str[18];
            sprintf(num_str, "%d.%0*d", integer, dec_places, fraction);
            if (integer == 0 && num < 0)
              result += '-';
            result += String(num_str);
            if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
              result += ", ";
            continue;
          }
          case _32BIT: {
            int32_t num = 0;
            for(int j = 0; j < 32; j += 8)
              num |= recv_buf[i++] << j;
            uint8_t dec_places = tag & DECIMAL_POINT_MASK;
            int32_t integer = num / quick_pow10(dec_places);
            uint32_t fraction = abs(num) % quick_pow10(dec_places);
            char num_str[18];
            sprintf(num_str, "%ld.%0*ld", integer, dec_places, fraction);
            if (integer == 0 && num < 0)
              result += '-';
            result += String(num_str);
            if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
              result += ", ";
            continue;
          }
        }
      }
    }
  }
  result += '}';
  return result;
}

// Быстрое возведение 10 в степень
int64_t quick_pow10(int n) {
  static int64_t pow10[] = {
    1, 10, 100, 1000,
    10000, 100000, 1000000, 10000000,
    100000000, 1000000000, 10000000000, 100000000000,
    1000000000000, 10000000000000, 100000000000000, 1000000000000000
  };
  if(n < 0 || n >= (sizeof(pow10)/sizeof(pow10[0])))
    return 1;
  return pow10[n];
}