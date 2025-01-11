#ifndef SETTINGS_H
#define SETTINGS_H

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

// Параметры для генерации JSON
#define PARAM_SerialDevice "0"
#define PARAM_Akey "NeKKxx2"
#define PARAM_VersionDevice "TestSecond"

// Настройки Wi-Fi (в settings.cpp)
extern const char *ssid;
extern const char *password;
extern const uint8_t net_ip[4];
extern const uint8_t net_gateway_ip[4];
extern const uint8_t net_mask[4];

// Адреса серверов (в settings.cpp)
extern const String servers[3];

// Счетчик отправленных пакетов
extern unsigned long int packetcntr;

#endif // SETTINGS_H