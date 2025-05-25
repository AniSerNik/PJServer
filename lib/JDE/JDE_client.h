// Copyright [2025] <>

/** 
  * @defgroup JDE_client 
  * @brief Клиентский модуль для работы с протоколом LoRa кодирования и декодирования JSON
  * @details Модуль реализует функции кодирования JSON для отправки на сервер (шлюз)
  * @{
*/

#ifndef LIB_JDE_JDE_CLIENT_H_
#define LIB_JDE_JDE_CLIENT_H_

#include <RH_RF95.h>
#include "JDE_protocol.h"

/**
 * @brief Обрабатывает строку JSON и сохраняет его в jsonDoc для дальнейшей работы
 * @param str Строка JSON
 * @return true, если строка JSON обработана успешно, иначе false
 */
bool jsonStringProcess(String str);

/**
 * @brief Получает ключи из JSON
 * @details Получает ключи из JSON и сохраняет их в массив с ключами (Использует JSON полученный из jsonStringProcess)
 */
void parseJsonKeys();

/**
 * @brief Кодирует ключи в буфер для отправки на сервер
 * @details Кодирует ключи в буфер и сохраняет их в массив с ключами (Использует JSON полученный из jsonStringProcess)
 */
void encodeJsonKeys();

/**
 * @brief Кодирует JSON для отправки на сервер
 * @details Кодирует JSON для отправки на сервер и сохраняет его в send_buf (Использует JSON полученный из jsonStringProcess)
 */
void encodeJsonForSending();

/**
 * @brief Устанавливает уровни логирования для библиотеки JDE_client
 * @param debugPrint Включить/выключить основной отладочный вывод (эквивалент SerialP1)
 * @param encodePrint Включить/выключить отладочный вывод кодирования (эквивалент SerialP2)
 */
void JDESetLogging(bool debugPrint, bool encodePrint);

extern uint8_t send_buf[RH_RF95_MAX_MESSAGE_LEN];    //< Буфер для отправки на сервер

/*! @} */

#endif // LIB_JDE_JDE_CLIENT_H_