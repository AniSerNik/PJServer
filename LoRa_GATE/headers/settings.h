// Copyright [2025] Мальцев Максим Дмитриевич <maksdm007@gmail.com>

#ifndef LORA_GATE_HEADERS_SETTINGS_H_
#define LORA_GATE_HEADERS_SETTINGS_H_

#include <Arduino.h>

// Максимальное количество сетей WiFi
#define MAX_WIFI_NETWORKS 32
#define MAX_SERVERS 16

// Структура для хранения информации о сетях WiFi
struct WiFiNetwork
{
  String ssid;
  String password;
  bool useStaticSettings;
  IPAddress net_ip;
  IPAddress net_gateway_ip;
  IPAddress net_mask;
};

// Смещение UTC в секундах
extern int32_t UTC_OFFSET;

// Адрес шлюза LoRa
extern uint8_t SERVER_ADDRESS;

// Настройки LoRa
extern uint8_t LORA_TXPOWER;
extern float LORA_FREQUENCY;
extern uint8_t LORA_CODING_RATE4;
extern uint32_t LORA_SIGNAL_BANDWIDTH;
extern uint8_t LORA_SPREADING_FACTOR;

// Очистка мусора
extern uint32_t GARBAGE_COLLECT_COOLDOWN; // 6 часов в миллисекундах (21600e3)
// Интервал синхронизации реального времени
extern uint32_t TIME_SYNC_INTERVAL; // 12 часов в миллисекундах (43200e3)
// Таймаут на очистку данных об устройстве
extern uint32_t DATACOL_TIMESTORE; // 2 часа в миллисекундах (7200e3)
// Интервал отправки данных о шлюзе
extern uint32_t GATEWORKPING_INTERVAL; // 5 минут в миллисекундах (300e3)
// Интервал отрисовки данных на дисплее
extern uint32_t DISPLAY_INTERVAL; // 10 секунд в миллисекундах (10e3)

// Количество попыток синхронизации времени после подключения к Wi-Fi
extern uint8_t TIME_SYNC_RETRY_COUNT;
// Количество попыток подключения к Wi-Fi
extern uint8_t WIFI_CONNECT_RETRY_COUNT;
// Задержка на попытку подключения к Wi-Fi
extern uint32_t WIFI_CONNECT_COOLDOWN;
// Задержка на попытку синхронизации времени
extern uint32_t TIME_SYNC_DELAY;

// Параметры для генерации JSON
extern String PARAM_SerialDevice;
extern String PARAM_Akey;
extern String PARAM_VersionDevice;

// Настройки Wi-Fi
extern WiFiNetwork wifiNetworks[MAX_WIFI_NETWORKS];

// Адрес NTP сервера
extern String net_ntp;

// Адреса серверов
extern String servers[MAX_SERVERS];

// Функции для работы с NVS
void loadSettings();
void saveSettings();

#endif // LORA_GATE_HEADERS_SETTINGS_H_