// Copyright [2025] Мальцев Максим Дмитриевич <maksdm007@gmail.com>

#include "headers/settings.h"
#include <Preferences.h>

// Адрес шлюза LoRa
uint8_t SERVER_ADDRESS = 200U;

// Очистка мусора
uint32_t GARBAGE_COLLECT_COOLDOWN = 21600000U; // 6 часов в миллисекундах (21600e3)
uint32_t TIME_SYNC_INTERVAL = 43200000U;       // 12 часов в миллисекундах (43200e3)
uint32_t DATACOL_TIMESTORE = 7200000U;         // 2 часа в миллисекундах (7200e3)
uint32_t GATEWORKPING_INTERVAL = 300000U;      // 5 минут в миллисекундах (300e3)
uint32_t DISPLAY_INTERVAL = 10000U;            // 10 секунд в миллисекундах (10e3)

uint8_t TIME_SYNC_RETRY_COUNT = 5U;
uint8_t WIFI_CONNECT_RETRY_COUNT = 5U;
uint32_t WIFI_CONNECT_COOLDOWN = 1000U;
uint32_t TIME_SYNC_DELAY = 2000U;

// Параметры для генерации JSON
String PARAM_SerialDevice = "0";
String PARAM_Akey = "NeKKxx2";
String PARAM_VersionDevice = "TestSecond";

// Сети Wi-Fi
WiFiNetwork wifiNetworks[MAX_WIFI_NETWORKS] = {
    {"MyNetWork", "mmmmmmmm", false, {192, 168, 0, 95}, {192, 168, 0, 1}, {255, 255, 255, 0}},
    {"rt451-Ext", "apsI0gqvFn", false, {192, 168, 0, 95}, {192, 168, 0, 1}, {255, 255, 255, 0}},
    {"MyNetWork1", "mmmmmmmm", false, {192, 168, 0, 95}, {192, 168, 0, 1}, {255, 255, 255, 0}}};

// Адрес NTP сервера
String net_ntp = "pool.ntp.org";

// Адреса серверов
String servers[3] = {
    "http://robo.itkm.ru/core/jsonapp.php",
    "http://46.254.216.214/core/jsonapp.php",
    "http://dbrobo1.mf.bmstu.ru/core/jsonapp.php"};

// Настройки LoRa
uint8_t LORA_TXPOWER = 20;
float LORA_FREQUENCY = 869.2;
uint8_t LORA_CODING_RATE4 = 5;
uint32_t LORA_SIGNAL_BANDWIDTH = 125000;
uint8_t LORA_SPREADING_FACTOR = 9;

// Смещение UTC в секундах
int32_t UTC_OFFSET = 10800; // +3 часа

Preferences preferences;

void loadSettings()
{
    preferences.begin("settings", true);

    SERVER_ADDRESS = preferences.getUChar("SERVER_ADDRESS", SERVER_ADDRESS);
    GARBAGE_COLLECT_COOLDOWN = preferences.getUInt("GARBAGE_COLLECT_COOLDOWN", GARBAGE_COLLECT_COOLDOWN);
    TIME_SYNC_INTERVAL = preferences.getUInt("TIME_SYNC_INTERVAL", TIME_SYNC_INTERVAL);
    DATACOL_TIMESTORE = preferences.getUInt("DATACOL_TIMESTORE", DATACOL_TIMESTORE);
    GATEWORKPING_INTERVAL = preferences.getUInt("GATEWORKPING_INTERVAL", GATEWORKPING_INTERVAL);
    DISPLAY_INTERVAL = preferences.getUInt("DISPLAY_INTERVAL", DISPLAY_INTERVAL);
    TIME_SYNC_RETRY_COUNT = preferences.getUChar("TIME_SYNC_RETRY_COUNT", TIME_SYNC_RETRY_COUNT);
    WIFI_CONNECT_RETRY_COUNT = preferences.getUChar("WIFI_CONNECT_RETRY_COUNT", WIFI_CONNECT_RETRY_COUNT);
    WIFI_CONNECT_COOLDOWN = preferences.getUInt("WIFI_CONNECT_COOLDOWN", WIFI_CONNECT_COOLDOWN);
    TIME_SYNC_DELAY = preferences.getUInt("TIME_SYNC_DELAY", TIME_SYNC_DELAY);
    PARAM_SerialDevice = preferences.getString("PARAM_SerialDevice", PARAM_SerialDevice);
    PARAM_Akey = preferences.getString("PARAM_Akey", PARAM_Akey);
    PARAM_VersionDevice = preferences.getString("PARAM_VersionDevice", PARAM_VersionDevice);
    net_ntp = preferences.getString("net_ntp", net_ntp);
    for (int i = 0; i < MAX_WIFI_NETWORKS; i++)
    {
        wifiNetworks[i].ssid = preferences.getString(("wifi_ssid" + String(i)).c_str(), wifiNetworks[i].ssid);
        wifiNetworks[i].password = preferences.getString(("wifi_password" + String(i)).c_str(), wifiNetworks[i].password);
        wifiNetworks[i].useStaticSettings = preferences.getBool(("wifi_useStatic" + String(i)).c_str(), wifiNetworks[i].useStaticSettings);
        preferences.getBytes(("wifi_ip" + String(i)).c_str(), wifiNetworks[i].net_ip, sizeof(wifiNetworks[i].net_ip));
        preferences.getBytes(("wifi_gateway" + String(i)).c_str(), wifiNetworks[i].net_gateway_ip, sizeof(wifiNetworks[i].net_gateway_ip));
        preferences.getBytes(("wifi_mask" + String(i)).c_str(), wifiNetworks[i].net_mask, sizeof(wifiNetworks[i].net_mask));
    }
    for (int i = 0; i < 3; i++)
    {
        servers[i] = preferences.getString(("server" + String(i)).c_str(), servers[i]);
    }

    LORA_TXPOWER = preferences.getUChar("LORA_TXPOWER", LORA_TXPOWER);
    LORA_FREQUENCY = preferences.getFloat("LORA_FREQUENCY", LORA_FREQUENCY);
    LORA_CODING_RATE4 = preferences.getUChar("LORA_CODING_RATE4", LORA_CODING_RATE4);
    LORA_SIGNAL_BANDWIDTH = preferences.getUInt("LORA_SIGNAL_BANDWIDTH", LORA_SIGNAL_BANDWIDTH);
    LORA_SPREADING_FACTOR = preferences.getUChar("LORA_SPREADING_FACTOR", LORA_SPREADING_FACTOR);

    UTC_OFFSET = preferences.getInt("UTC_OFFSET", UTC_OFFSET);

    preferences.end();
}

void saveSettings()
{
    preferences.begin("settings", false);

    preferences.putUChar("SERVER_ADDRESS", SERVER_ADDRESS);
    preferences.putUInt("GARBAGE_COLLECT_COOLDOWN", GARBAGE_COLLECT_COOLDOWN);
    preferences.putUInt("TIME_SYNC_INTERVAL", TIME_SYNC_INTERVAL);
    preferences.putUInt("DATACOL_TIMESTORE", DATACOL_TIMESTORE);
    preferences.putUInt("GATEWORKPING_INTERVAL", GATEWORKPING_INTERVAL);
    preferences.putUInt("DISPLAY_INTERVAL", DISPLAY_INTERVAL);
    preferences.putUChar("TIME_SYNC_RETRY_COUNT", TIME_SYNC_RETRY_COUNT);
    preferences.putUChar("WIFI_CONNECT_RETRY_COUNT", WIFI_CONNECT_RETRY_COUNT);
    preferences.putUInt("WIFI_CONNECT_COOLDOWN", WIFI_CONNECT_COOLDOWN);
    preferences.putUInt("TIME_SYNC_DELAY", TIME_SYNC_DELAY);
    preferences.putString("PARAM_SerialDevice", PARAM_SerialDevice);
    preferences.putString("PARAM_Akey", PARAM_Akey);
    preferences.putString("PARAM_VersionDevice", PARAM_VersionDevice);
    preferences.putString("net_ntp", net_ntp);
    for (int i = 0; i < MAX_WIFI_NETWORKS; i++)
    {
        preferences.putString(("wifi_ssid" + String(i)).c_str(), wifiNetworks[i].ssid);
        preferences.putString(("wifi_password" + String(i)).c_str(), wifiNetworks[i].password);
        preferences.putBool(("wifi_useStatic" + String(i)).c_str(), wifiNetworks[i].useStaticSettings);
        preferences.putBytes(("wifi_ip" + String(i)).c_str(), wifiNetworks[i].net_ip, sizeof(wifiNetworks[i].net_ip));
        preferences.putBytes(("wifi_gateway" + String(i)).c_str(), wifiNetworks[i].net_gateway_ip, sizeof(wifiNetworks[i].net_gateway_ip));
        preferences.putBytes(("wifi_mask" + String(i)).c_str(), wifiNetworks[i].net_mask, sizeof(wifiNetworks[i].net_mask));
    }
    for (int i = 0; i < 3; i++)
    {
        preferences.putString(("server" + String(i)).c_str(), servers[i]);
    }

    preferences.putUChar("LORA_TXPOWER", LORA_TXPOWER);
    preferences.putFloat("LORA_FREQUENCY", LORA_FREQUENCY);
    preferences.putUChar("LORA_CODING_RATE4", LORA_CODING_RATE4);
    preferences.putUInt("LORA_SIGNAL_BANDWIDTH", LORA_SIGNAL_BANDWIDTH);
    preferences.putUChar("LORA_SPREADING_FACTOR", LORA_SPREADING_FACTOR);

    preferences.putInt("UTC_OFFSET", UTC_OFFSET);

    preferences.end();
}