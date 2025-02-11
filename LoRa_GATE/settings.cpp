// Copyright [2025] Мальцев Максим Дмитриевич <maksdm007@gmail.com>

#include "headers/settings.h"
#include <Preferences.h>
#include <nvs_flash.h>

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
    {"MyNetWork", "mmmmmmmm", false, IPAddress(192, 168, 0, 95), IPAddress(192, 168, 0, 1), IPAddress(255, 255, 255, 0)},
    {"rt451-Ext", "apsI0gqvFn", false, IPAddress(192, 168, 0, 95), IPAddress(192, 168, 0, 1), IPAddress(255, 255, 255, 0)}};

// Адрес NTP сервера
String net_ntp = "pool.ntp.org";

// Адреса серверов
String servers[MAX_SERVERS] = {
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

    SERVER_ADDRESS = preferences.getUChar("srv_addr", SERVER_ADDRESS);
    GARBAGE_COLLECT_COOLDOWN = preferences.getUInt("gc_cd", GARBAGE_COLLECT_COOLDOWN);
    TIME_SYNC_INTERVAL = preferences.getUInt("ts_int", TIME_SYNC_INTERVAL);
    DATACOL_TIMESTORE = preferences.getUInt("dc_ts", DATACOL_TIMESTORE);
    GATEWORKPING_INTERVAL = preferences.getUInt("gw_ping_int", GATEWORKPING_INTERVAL);
    DISPLAY_INTERVAL = preferences.getUInt("disp_int", DISPLAY_INTERVAL);
    TIME_SYNC_RETRY_COUNT = preferences.getUChar("ts_retry", TIME_SYNC_RETRY_COUNT);
    WIFI_CONNECT_RETRY_COUNT = preferences.getUChar("wifi_retry", WIFI_CONNECT_RETRY_COUNT);
    WIFI_CONNECT_COOLDOWN = preferences.getUInt("wifi_cd", WIFI_CONNECT_COOLDOWN);
    TIME_SYNC_DELAY = preferences.getUInt("ts_delay", TIME_SYNC_DELAY);
    PARAM_SerialDevice = preferences.getString("serial_dev", PARAM_SerialDevice);
    PARAM_Akey = preferences.getString("akey", PARAM_Akey);
    PARAM_VersionDevice = preferences.getString("ver_dev", PARAM_VersionDevice);
    net_ntp = preferences.getString("ntp", net_ntp);

    for (int i = 0; i < MAX_WIFI_NETWORKS; i++)
    {
        wifiNetworks[i].ssid = preferences.getString(("wifi_ssid" + String(i)).c_str(), wifiNetworks[i].ssid);
        wifiNetworks[i].password = preferences.getString(("wifi_pwd" + String(i)).c_str(), wifiNetworks[i].password);
        wifiNetworks[i].useStaticSettings = preferences.getBool(("wifi_static" + String(i)).c_str(), wifiNetworks[i].useStaticSettings);
        wifiNetworks[i].net_ip.fromString(preferences.getString(("wifi_ip" + String(i)).c_str(), wifiNetworks[i].net_ip.toString()));
        wifiNetworks[i].net_gateway_ip.fromString(preferences.getString(("wifi_gw" + String(i)).c_str(), wifiNetworks[i].net_gateway_ip.toString()));
        wifiNetworks[i].net_mask.fromString(preferences.getString(("wifi_mask" + String(i)).c_str(), wifiNetworks[i].net_mask.toString()));
    }

    for (int i = 0; i < MAX_SERVERS; i++)
    {
        servers[i] = preferences.getString(("srv" + String(i)).c_str(), servers[i]);
    }

    LORA_TXPOWER = preferences.getUChar("lora_txpwr", LORA_TXPOWER);
    LORA_FREQUENCY = preferences.getFloat("lora_freq", LORA_FREQUENCY);
    LORA_CODING_RATE4 = preferences.getUChar("lora_cr", LORA_CODING_RATE4);
    LORA_SIGNAL_BANDWIDTH = preferences.getUInt("lora_bw", LORA_SIGNAL_BANDWIDTH);
    LORA_SPREADING_FACTOR = preferences.getUChar("lora_sf", LORA_SPREADING_FACTOR);
    UTC_OFFSET = preferences.getInt("utc_offset", UTC_OFFSET);

    preferences.end();
}

void saveSettings()
{
    // Очистка NVS
    esp_err_t err = nvs_flash_erase();
    if (err != ESP_OK)
    {
        printf("Ошибка очистки NVS\n");
        return;
    }

    // Инициализация NVS
    err = nvs_flash_init();
    if (err != ESP_OK)
    {
        printf("Ошибка инициализации NVS\n");
        return;
    }

    preferences.begin("settings", false);

    preferences.putUChar("srv_addr", SERVER_ADDRESS);
    preferences.putUInt("gc_cd", GARBAGE_COLLECT_COOLDOWN);
    preferences.putUInt("ts_int", TIME_SYNC_INTERVAL);
    preferences.putUInt("dc_ts", DATACOL_TIMESTORE);
    preferences.putUInt("gw_ping_int", GATEWORKPING_INTERVAL);
    preferences.putUInt("disp_int", DISPLAY_INTERVAL);
    preferences.putUChar("ts_retry", TIME_SYNC_RETRY_COUNT);
    preferences.putUChar("wifi_retry", WIFI_CONNECT_RETRY_COUNT);
    preferences.putUInt("wifi_cd", WIFI_CONNECT_COOLDOWN);
    preferences.putUInt("ts_delay", TIME_SYNC_DELAY);
    preferences.putString("serial_dev", PARAM_SerialDevice);
    preferences.putString("akey", PARAM_Akey);
    preferences.putString("ver_dev", PARAM_VersionDevice);
    preferences.putString("ntp", net_ntp);

    for (int i = 0; i < MAX_WIFI_NETWORKS; i++)
    {
        preferences.putString(("wifi_ssid" + String(i)).c_str(), wifiNetworks[i].ssid);
        preferences.putString(("wifi_pwd" + String(i)).c_str(), wifiNetworks[i].password);
        preferences.putBool(("wifi_static" + String(i)).c_str(), wifiNetworks[i].useStaticSettings);
        preferences.putString(("wifi_ip" + String(i)).c_str(), wifiNetworks[i].net_ip.toString());
        preferences.putString(("wifi_gw" + String(i)).c_str(), wifiNetworks[i].net_gateway_ip.toString());
        preferences.putString(("wifi_mask" + String(i)).c_str(), wifiNetworks[i].net_mask.toString());
    }

    for (int i = 0; i < MAX_SERVERS; i++)
    {
        preferences.putString(("srv" + String(i)).c_str(), servers[i]);
    }

    preferences.putUChar("lora_txpwr", LORA_TXPOWER);
    preferences.putFloat("lora_freq", LORA_FREQUENCY);
    preferences.putUChar("lora_cr", LORA_CODING_RATE4);
    preferences.putUInt("lora_bw", LORA_SIGNAL_BANDWIDTH);
    preferences.putUChar("lora_sf", LORA_SPREADING_FACTOR);
    preferences.putInt("utc_offset", UTC_OFFSET);

    preferences.end();
}