// Copyright [2025] <>

#ifndef LIB_JDE_JDE_PROTOCOL_H_
#define LIB_JDE_JDE_PROTOCOL_H_

// Определение максимальной длины поля JSON и максимального уровня вложенности
#define JSON_MAX_LEN_FIELD 32    // Максимальная длина поля JSON
#define JSON_MAX_NESTEDOBJECT 16 // Максимальный уровень вложенности JSON

// Определения для типов данных в JSON
#define COMMAND 0       // [00000000] Команда
#define BYTE_COUNT 1    // [00000001] Количество байт в пакете
#define START_PAYLOAD 2 // [00000010] Начало полезной нагрузки

#define REPLY_TRUE 0x01  // [00000001] Ответ ДА
#define REPLY_FALSE 0x00 // [00000000] Ответ НЕТ

// Константы и макросы
#define JDE_NOP 0x00      // [00000000] Пустая команда
#define REGISTRATION 0x01 // [00000001] Регистрация устройства
#define DATA 0x02         // [00000010] Отправка данных
#define CLEAR_INFO 0x03   // [00000011] Очистка информации об устройстве

#define GENERAL_TYPE_MASK 0xC0  // [11000000] Маска для определения общего типа
#define SPECIFIC_TYPE_MASK 0x3F // [00111111] Маска для определения конкретного типа
#define NUMBER_SIZE_MASK 0x30   // [00110000] Маска для определения размера числа
#define DECIMAL_POINT_MASK 0x0F // [00001111] Маска для определения количества знаков после запятой

#define INTEGER 0x00      // [00000000] Целое число
#define CUSTOM_FLOAT 0x40 // [01000000] Число с плавающей точкой
#define STRING 0x80       // [10000000] Строка
#define SPECIFIC 0xC0     // [11000000] Специфический тип данных

// Специфические типы данных
#define IP_ADDR 0x01       // [00000001] IP адрес
#define MAC_ADDR 0x02      // [00000010] MAC адрес
#define DATE_YYYYMMDD 0x03 // [00000011] Дата ГГГГММДД
#define TIME_HHMMSS 0x04   // [00000100] Время ЧЧММСС
#define JSON_OBJECT 0x05   // [00000101] Объект JSON
#define JSON_ARRAY 0x06    // [00000110] Массив JSON
#define JSON_LEVEL_UP 0xFF // [11111111] Подняться на уровень выше во вложенности JSON

// Размеры чисел
#define _8BIT 0x00  // [00000000] 8 бит
#define _16BIT 0x10 // [00010000] 16 бит
#define _32BIT 0x20 // [00100000] 32 бит

#endif // LIB_JDE_JDE_PROTOCOL_H_