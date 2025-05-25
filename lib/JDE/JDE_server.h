// Copyright [2025] <>

/** 
  * @defgroup JDE_server 
  * @brief Серверный модуль для работы с протоколом LoRa кодирования и декодирования JSON
  * @details Модуль реализует функции декодирования JSON для получения данных от устройств
  * @{
*/

#ifndef LIB_JDE_JDE_SERVER_H_
#define LIB_JDE_JDE_SERVER_H_

#include <Arduino.h>
#include <string>
#include <unordered_map>
#include "JDE_protocol.h"

// Структура для хранения информации об устройстве
struct deviceInfo
{
    std::unordered_map<uint8_t, std::string> maskKeys; // Ключи для декодирования
    uint8_t *device_buf;                               // Буфер для временного хранения пакета LoRa
    size_t device_buf_size;                            // Размер буфера
    uint64_t lastSendTime;                             // Время последнего пакета от устройства
};

// Глобальная карта для хранения информации об устройствах
extern std::unordered_map<uint8_t, deviceInfo> devicesInfo;

/**
 * @brief Декодирует пакет LoRa в JSON
 * @details Декодирует пакет LoRa и возвращает строку с декодированным JSON
 * @param recv_buf Буфер с данными (Пакет LoRa)
 * @param dev_id ID устройства от которого пришел пакет
 * @param lastRssi RSSI последнего пакета
 * @return Строка с декодированным JSON
*/
String decodeJsonFromBytes(uint8_t *recv_buf, uint8_t dev_id, int16_t lastRssi);

/**
 * @brief Получает ключ по маске JSON ключа
 * @details Получает ключ по маске JSON ключа и возвращает строку с ключом
 * @param idKey ID ключа
 * @param dev_id ID устройства от которого пришел пакет
 * @return Строка с ключом
 */
String getKeyFromId(uint8_t idKey, uint8_t dev_id);

/**
 * @brief Быстрое возведение в степень 10т
 * @param n Степень
 * @return Результат возведения в степень
 */
int64_t quick_pow10(int n);

/*! @} */

#endif // LIB_JDE_JDE_SERVER_H_