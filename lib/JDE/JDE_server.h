// Copyright [2025] <>

#ifndef LIB_JDE_JDE_SERVER_H_
#define LIB_JDE_JDE_SERVER_H_

#include <Arduino.h>
#include <string>
#include <unordered_map>
#include <JDE_protocol.h>

// Определение максимальной длины поля JSON и максимального уровня вложенности
#define JSON_MAX_LEN_FIELD 32
#define JSON_MAX_NESTEDOBJECT 16

// Структура для хранения информации об устройстве
struct deviceInfo
{
    std::unordered_map<uint8_t, std::string> maskKeys; // Ключи для декодирования
    uint8_t *device_buf;                               // Буфер для данных устройства
    size_t device_buf_size;                            // Размер буфера
    uint64_t lastSendTime;                             // Время последней отправки
};

// Глобальная карта для хранения информации об устройствах
extern std::unordered_map<uint8_t, deviceInfo> devicesInfo;

String decodeJsonFromBytes(uint8_t *recv_buf, uint8_t dev_id, int16_t lastRssi);
String getKeyFromId(uint8_t idKey, uint8_t dev_id);
int64_t quick_pow10(int n);

#endif // LIB_JDE_JDE_SERVER_H_