#define RH_ENABLE_EXPLICIT_RETRY_DEDUP 1

#include <pins.h>
#include <Arduino.h>
#include <string>
#include <unordered_map>
#include <RH_RF95.h>
#include <RHReliableDatagram.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>

#define printBuf(buf) do { \
  for(int ij = 0; ij < buf[BYTE_COUNT]; ij++) \
    printf("%d ", buf[ij]); \
  printf("\n"); \
} while(0)

// Константы и макросы
#define JSON_MAX_LEN_FIELD 32
#define JSON_MAX_NESTEDOBJECT 16

// Код для клиент/сервер
#define COMMAND 0
#define BYTE_COUNT 1
#define START_PAYLOAD 2

#define NOP 0
#define REGISTRATION 1
#define DATA 2
#define CLEAR_INFO 3

#define REPLY_TRUE 1
#define REPLY_FALSE 0

#define INTEGER 0x00		        //00000000
#define CUSTOM_FLOAT 0x40	      //01000000
#define STRING 0x80		          //10000000
#define SPECIFIC 0xC0 		      //11000000
#define _8BIT 0x00   		        //00000000
#define _16BIT 0x10		          //00010000
#define _32BIT 0x20		          //00100000
#define IP_ADDR 0x01		        //00000001
#define MAC_ADDR 0x02		        //00000010
#define DATE_YYYYMMDD 0x03      //00000011
#define TIME_HHMMSS 0x04	      //00000100
#define JSON_OBJECT 0x05	      //00000101
#define JSON_ARRAY 0x06         //00000110
#define JSON_LEVEL_UP 0xFF	    //11111111

#define GENERAL_TYPE_MASK 0xC0	//11000000
#define SPECIFIC_TYPE_MASK 0x3F	//00111111
#define NUMBER_SIZE_MASK 0x30	  //00110000
#define DECIMAL_POINT_MASK 0x0F	//00001111

#define SERVER_ADDRESS 200

#define WIFITRUES 15
#define WIFICOOLDOWN 500

// Собранные старые данные
#define DATACOL_COOLDOWN 120000 // в миллисекундах (120e3)
#define DATACOL_TIMESTORE 180000 // в миллисекундах (180e3)
// Ping работы шлюза
#define GATEWORKPING_INTERVAL 300000 // в миллисекундах (300e3)

// Параметры для генерации JSON
#define PARAM_SerialDevice "0"
#define PARAM_Akey "NeKKxx2"
#define PARAM_VersionDevice "TestSecond"

// Глобальные переменные
unsigned long int packetcntr = 0;

typedef struct deviceInfo {
  std::unordered_map<uint8_t, std::string> maskKeys;
  unsigned long int lastSendTime = 0;
  uint8_t *device_buf = NULL;
  size_t device_buf_size = 0;
} deviceInfo;

std::unordered_map<uint8_t, deviceInfo> devicesInfo;
uint8_t curDeviceIdHandling = 0x00;

// LoRa Packet
struct LoRaPacket {
  uint8_t from;
  uint8_t len;
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
};

// LoRa и менеджер
RH_RF95 driver(LORA_CS, LORA_DIO0);
RHReliableDatagram manager(driver, SERVER_ADDRESS);

// Wi-Fi настройки
const char* ssid     = "MyNetWork";
const char* password = "mmmmmmmm";

IPAddress staticIP(192,168,0,95);
IPAddress gateway(192,168,0,1);
IPAddress mask(255,255,255,0);

String curDeviceJson = "";

// Очереди
QueueHandle_t loraQueue;
QueueHandle_t processQueue;
QueueHandle_t serverQueue;
QueueHandle_t sendQueue;

// Семафор для защиты доступа к devicesInfo
SemaphoreHandle_t devicesInfoMutex;

// Функции
String decodeJsonFromBytes(uint8_t *recv_buf, uint8_t dev_id);
String getKeyFromId(uint8_t idKey);
int64_t quick_pow10(int n);

// Задачи
void loraReceiveTask(void *pvParameters);
void processPackageTask(void *pvParameters);
void sendToServerTask(void *pvParameters);
//void deviceManagementTask(void *pvParameters);
void gatePingTask(void *pvParameters);
void LoRa_sendTask(void *pvParameters);

// Функции отправки на сервер и печати
void send_to_server(String data, String server_addr);
void registationKeys(uint8_t from, uint8_t *recv_buf);
void deleteInfo(uint8_t from);

void setup()  {
  Serial.begin(115200);
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
  loraQueue = xQueueCreate(10, sizeof(LoRaPacket));
  processQueue = xQueueCreate(10, sizeof(uint8_t) * RH_RF95_MAX_MESSAGE_LEN);
  serverQueue = xQueueCreate(10, sizeof(String));
  sendQueue = xQueueCreate(10, sizeof(LoRaPacket));

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
    LoRa_sendTask,       // Функция задачи
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
        packetcntr++;
      }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS); // Немного задержки для снижения нагрузки
  }
}

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

// Глобальные переменные для адресов серверов
const char* server_addresses[] = {
  "http://robo.itkm.ru/core/jsonapp.php",
  "http://46.254.216.214/core/jsonapp.php",
  "http://dbrobo1.mf.bmstu.ru/core/jsonapp.php"
};

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
      json += "\"packetTotal\":\"" + String(packetcntr) + "\", ";
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

// Задача для отправки данных
void LoRa_sendTask(void *pvParameters) {
  LoRaPacket data = {0};
  while (1) {
    if (xQueueReceive(sendQueue, &data, portMAX_DELAY)) {
      // Отправка ответа
      if (!manager.sendtoWait(data.buf, data.len, data.from)) {
        printf("sendtoWait failed\n");
      }
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
