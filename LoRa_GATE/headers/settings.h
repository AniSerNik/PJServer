// Copyright [2025] Мальцев Максим Дмитриевич <maksdm007@gmail.com>

#ifndef LORA_GATE_HEADERS_SETTINGS_H_
#define LORA_GATE_HEADERS_SETTINGS_H_

#include <Arduino.h>

#define printBuf(buf)                            \
  do                                             \
  {                                              \
    for (int ij = 0; ij < buf[BYTE_COUNT]; ij++) \
      printf("%d ", buf[ij]);                    \
    printf("\n\n");                              \
  } while (0)

// Включение поддержки дедупликации
#define RH_ENABLE_EXPLICIT_RETRY_DEDUP 1

// Настройки модуля LoRa
#define LORA_TXPOWER 20
#define LORA_FREQUNCY 869.2
#define LORA_CODING_RATE4 5
#define LORA_SIGNAL_BANDWIDTH 125000
#define LORA_SPREADING_FACTOR 9

// Адрес шлюза LoRa
#define SERVER_ADDRESS 200

// Очистка мусора, в миллисекундах (21600e3)
#define GARBAGE_COLLECT_COOLDOWN 21600000
// Таймаут на очистку данных об устройстве, в миллисекундах (3600e3)
#define DATACOL_TIMESTORE 3600000
// Ping работы шлюза
#define GATEWORKPING_INTERVAL 300000 // в миллисекундах (300e3)
// Интервал отрисовки данных на дисплее
#define DISPLAY_INTERVAL 10000 // в миллисекундах (10e3)
// Интервал синхронизации времени
#define TIME_SYNC_INTERVAL 3600000 // в миллисекундах (1e3)

// Параметры для генерации JSON
#define PARAM_SerialDevice "0"
#define PARAM_Akey "NeKKxx2"
#define PARAM_VersionDevice "TestSecond"

// Конфигурировать ли статический IP
#define CONFIG_STATIC_IP 0

// Настройки Wi-Fi (в settings.cpp)
extern const char *ssid;
extern const char *password;
extern const uint8_t net_ip[4];
extern const uint8_t net_gateway_ip[4];
extern const uint8_t net_mask[4];

// Адрес NTP сервера
extern const char *net_ntp;
// UTC смещение в секундах
#define UTC_OFFSET 10800 // Timezone: UTC+3

// Адреса серверов (в settings.cpp)
extern const String servers[3];

#endif // LORA_GATE_HEADERS_SETTINGS_H_