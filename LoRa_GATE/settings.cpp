// Copyright [2025] Мальцев Максим Дмитриевич <maksdm007@gmail.com>

#include "headers/settings.h"
#include <Preferences.h>
#include <nvs_flash.h>

uint8_t SERVER_ADDRESS = 200U;

uint32_t WEB_SERVER_INTERVAL = 300U; // 300 миллисекунд
uint32_t LORA_INTERVAL = 10U;        // 10 миллисекунд

uint32_t GARBAGE_COLLECT_COOLDOWN = 21600000U; // 6 часов в миллисекундах (21600e3)
uint32_t TIME_SYNC_INTERVAL = 3600000U;        // 1 час в миллисекундах (3600e3)
uint32_t DATACOL_TIMESTORE = 7200000U;         // 2 часа в миллисекундах (7200e3)
uint32_t GATEWORKPING_INTERVAL = 300000U;      // 5 минут в миллисекундах (300e3)
uint32_t DISPLAY_INTERVAL = 10000U;            // 10 секунд в миллисекундах (10e3)
uint32_t WIFI_CONNECT_COOLDOWN = 1000U;        // 1 секунда в миллисекундах (1e3)
uint32_t TIME_SYNC_DELAY = 2000U;              // 2 секунды в миллисекундах (2e3)
uint32_t NOWIFI_AP_DELAY = 300000U;            // 5 минут в миллисекундах (300e3)

uint8_t TIME_SYNC_RETRY_COUNT = 5U;
uint8_t WIFI_CONNECT_RETRY_COUNT = 5U;

// Параметры для генерации JSON
String PARAM_SerialDevice = "0";
String PARAM_AKEY = "NeKKxx2";
String PARAM_VersionDevice = "TestSecond";

// Сети Wi-Fi
WiFiNetwork wifiNetworks[MAX_WIFI_NETWORKS] = {
    {"rt451-Ext", "apsI0gqvFn", false, IPAddress(192, 168, 0, 95), IPAddress(192, 168, 0, 1), IPAddress(255, 255, 255, 0)}};

// Настройки точки доступа
String AP_NAME = "LoRaGateAP";
String AP_PASSWORD = "12345678";

// Адрес NTP сервера
String NET_NTP = "pool.ntp.org";

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
    WEB_SERVER_INTERVAL = preferences.getUInt("ws_int", WEB_SERVER_INTERVAL);
    LORA_INTERVAL = preferences.getUInt("lora_int", LORA_INTERVAL);
    AP_NAME = preferences.getString("ap_name", AP_NAME);
    AP_PASSWORD = preferences.getString("ap_pwd", AP_PASSWORD);
    NOWIFI_AP_DELAY = preferences.getUInt("ap_delay", NOWIFI_AP_DELAY);

    PARAM_SerialDevice = preferences.getString("serial_dev", PARAM_SerialDevice);
    PARAM_AKEY = preferences.getString("akey", PARAM_AKEY);
    PARAM_VersionDevice = preferences.getString("ver_dev", PARAM_VersionDevice);
    NET_NTP = preferences.getString("ntp", NET_NTP);

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

bool clearNVS()
{
    esp_err_t err = nvs_flash_erase();
    if (err != ESP_OK)
    {
        printf("Ошибка очистки NVS, код ошибки: %d\n", err);
        return 0;
    }

    err = nvs_flash_init();
    if (err != ESP_OK)
    {
        printf("Ошибка инициализации NVS после очистки, код ошибки: %d\n", err);
        return 0;
    }

    printf("NVS успешно очищена\n");
    return 1;
}

void saveSettings()
{
    clearNVS();

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
    preferences.putUInt("ws_int", WEB_SERVER_INTERVAL);
    preferences.putUInt("lora_int", LORA_INTERVAL);
    preferences.putString("ap_name", AP_NAME);
    preferences.putString("ap_pwd", AP_PASSWORD);
    preferences.putUInt("ap_delay", NOWIFI_AP_DELAY);

    preferences.putString("serial_dev", PARAM_SerialDevice);
    preferences.putString("akey", PARAM_AKEY);
    preferences.putString("ver_dev", PARAM_VersionDevice);
    preferences.putString("ntp", NET_NTP);

    uint8_t wifiIndex = 0U;
    for (uint8_t i = 0U; i < MAX_WIFI_NETWORKS; i++)
    {
        if (wifiNetworks[i].ssid.length() > 0U)
        {
            preferences.putString(("wifi_ssid" + String(wifiIndex)).c_str(), wifiNetworks[i].ssid);
            preferences.putString(("wifi_pwd" + String(wifiIndex)).c_str(), wifiNetworks[i].password);
            preferences.putBool(("wifi_static" + String(wifiIndex)).c_str(), wifiNetworks[i].useStaticSettings);
            preferences.putString(("wifi_ip" + String(wifiIndex)).c_str(), wifiNetworks[i].net_ip.toString());
            preferences.putString(("wifi_gw" + String(wifiIndex)).c_str(), wifiNetworks[i].net_gateway_ip.toString());
            preferences.putString(("wifi_mask" + String(wifiIndex)).c_str(), wifiNetworks[i].net_mask.toString());
            wifiIndex++;
        }
    }
    for (uint8_t i = wifiIndex; i < MAX_WIFI_NETWORKS; i++)
    {
        preferences.putString(("wifi_ssid" + String(i)).c_str(), "");
        preferences.putString(("wifi_pwd" + String(i)).c_str(), "");
        preferences.putBool(("wifi_static" + String(i)).c_str(), 0U);
        preferences.putString(("wifi_ip" + String(i)).c_str(), "");
        preferences.putString(("wifi_gw" + String(i)).c_str(), "");
        preferences.putString(("wifi_mask" + String(i)).c_str(), "");
    }

    uint8_t serverIndex = 0U;
    for (uint8_t i = 0U; i < MAX_SERVERS; i++)
    {
        if (servers[i].length() > 0U)
        {
            preferences.putString(("srv" + String(serverIndex)).c_str(), servers[i]);
            serverIndex++;
        }
    }
    for (uint8_t i = serverIndex; i < MAX_WIFI_NETWORKS; i++)
    {
        preferences.putString(("srv" + String(i)).c_str(), "");
    }

    preferences.putUChar("lora_txpwr", LORA_TXPOWER);
    preferences.putFloat("lora_freq", LORA_FREQUENCY);
    preferences.putUChar("lora_cr", LORA_CODING_RATE4);
    preferences.putUInt("lora_bw", LORA_SIGNAL_BANDWIDTH);
    preferences.putUChar("lora_sf", LORA_SPREADING_FACTOR);
    preferences.putInt("utc_offset", UTC_OFFSET);

    preferences.end();
}