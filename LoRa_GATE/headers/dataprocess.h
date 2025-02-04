// Copyright [2025] Мальцев Максим Дмитриевич <maksdm007@gmail.com>

#ifndef LORA_GATE_HEADERS_DATAPROCESS_H_
#define LORA_GATE_HEADERS_DATAPROCESS_H_

#include <Arduino.h>

#define printBuf(buf)                                \
    do                                               \
    {                                                \
        for (int ij = 0; ij < buf[BYTE_COUNT]; ij++) \
            printf("%d ", buf[ij]);                  \
        printf("\n\n");                              \
    } while (0)

// Константы и макросы
#define JSON_MAX_LEN_FIELD 32
#define JSON_MAX_NESTEDOBJECT 16

// Код для клиента/сервера
#define COMMAND 0
#define BYTE_COUNT 1
#define START_PAYLOAD 2

#define NOP 0
#define REGISTRATION 1
#define DATA 2
#define CLEAR_INFO 3

#define REPLY_TRUE 1
#define REPLY_FALSE 0

#define INTEGER 0x00       // 00000000
#define CUSTOM_FLOAT 0x40  // 01000000
#define STRING 0x80        // 10000000
#define SPECIFIC 0xC0      // 11000000
#define _8BIT 0x00         // 00000000
#define _16BIT 0x10        // 00010000
#define _32BIT 0x20        // 00100000
#define IP_ADDR 0x01       // 00000001
#define MAC_ADDR 0x02      // 00000010
#define DATE_YYYYMMDD 0x03 // 00000011
#define TIME_HHMMSS 0x04   // 00000100
#define JSON_OBJECT 0x05   // 00000101
#define JSON_ARRAY 0x06    // 00000110
#define JSON_LEVEL_UP 0xFF // 11111111

#define GENERAL_TYPE_MASK 0xC0  // 11000000
#define SPECIFIC_TYPE_MASK 0x3F // 00111111
#define NUMBER_SIZE_MASK 0x30   // 00110000
#define DECIMAL_POINT_MASK 0x0F // 00001111

// Задача обработки пакетов перед отправкой на сервер
void processPackageTask(void *pvParameters);

// Функции
String decodeJsonFromBytes(uint8_t *recv_buf, uint8_t dev_id, int16_t rssi); // Декодирование JSON из байтов
String getKeyFromId(uint8_t idKey);                                          // Получение ключа по ID
int64_t quick_pow10(int n);                                                  // Быстрое возведение 10 в степень
void registationKeys(uint8_t from, uint8_t *recv_buf);                       // Функция регистрации ключей
void deleteInfo(uint8_t from);                                               // Удаление инфрмации об устройстве

// Очередь для обработки данных
extern QueueHandle_t processQueue;

// Счетчик полученных пакетов с JSON
extern uint64_t receivedPacketCounter;
// Данные последнего полученного пакета
struct packetInfo
{
    uint8_t from;
    int16_t rssi;
    struct tm date;
};
extern struct packetInfo lastPacketData;

#endif // LORA_GATE_HEADERS_DATAPROCESS_H_