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

// Задача обработки пакетов перед отправкой на сервер
void processPackageTask(void *pvParameters);

// Функции
void registationKeys(uint8_t from, uint8_t *recv_buf); // Функция регистрации ключей
void deleteInfo(uint8_t from);                         // Удаление инфрмации об устройстве

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