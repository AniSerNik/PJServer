#include "settings.h"
#include <Arduino.h>

// Настройки подключения Wi-Fi
const char* ssid = "MyNetWork";
const char* password = "mmmmmmmm";

const uint8_t net_ip[4] = {192,168,0,95};
const uint8_t net_gateway_ip[4] = {192,168,0,1};
const uint8_t net_mask[4] = {255,255,255,0};

// Адреса серверов
const String servers[3] = {
  "http://robo.itkm.ru/core/jsonapp.php",
  "http://46.254.216.214/core/jsonapp.php",
  "http://dbrobo1.mf.bmstu.ru/core/jsonapp.php"
};

// Счетчик отправленных пакетов
unsigned long int packetcntr = 0;