#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

#define printBuf(buf) do { \
  for(int ij = 0; ij < buf[BYTE_COUNT]; ij++) \
    printf("%d ", buf[ij]); \
  printf("\n"); \
} while(0)

#define SERVER_ADDRESS 200

// Собранные старые данные
#define DATACOL_COOLDOWN 120000 // в миллисекундах (120e3)
#define DATACOL_TIMESTORE 180000 // в миллисекундах (180e3)
// Ping работы шлюза
#define GATEWORKPING_INTERVAL 300000 // в миллисекундах (300e3)

// Параметры для генерации JSON
#define PARAM_SerialDevice "0"
#define PARAM_Akey "NeKKxx2"
#define PARAM_VersionDevice "TestSecond"

// Wi-Fi настройки
extern const char* ssid;
extern const char* password;
extern const uint8_t net_ip[4];
extern const uint8_t net_gateway_ip[4];
extern const uint8_t net_mask[4];
extern const String servers[3];

#endif // SETTINGS_H