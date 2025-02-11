// Copyright [2025] Мальцев Максим Дмитриевич <maksdm007@gmail.com>

#include "headers/pins.h"
#include "headers/main.h"
#include "headers/lora_d.h"
#include "headers/wifi_d.h"
#include "headers/dataprocess.h"
#include "headers/garbagecollector.h"
#include "headers/display.h"
#include "headers/settings.h"

// Глобальные переменные
std::unordered_map<uint8_t, deviceInfo> devicesInfo;
SemaphoreHandle_t devicesInfoMutex;
SemaphoreHandle_t wifiConnectMutex;

void setup()
{
    Serial.begin(9600);
    printf("\n");

    // Загрузка настроек из NVS
    loadSettings();

    // Установка временной зоны в системе
    std::string tz = "CST-" + std::to_string(UTC_OFFSET / 3600U);
    setenv("TZ", tz.c_str(), 1);
    tzset();

    // Инициализация LoRa
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
    if (!manager.init())
        printf("Успешная инициализация модуля LoRa\n");
    else
        printf("Ошибка инициализации модуля LoRa\n");

    // Настройка модуля LoRa
    driver.setTxPower(LORA_TXPOWER, false);
    driver.setFrequency(LORA_FREQUENCY);
    driver.setCodingRate4(LORA_CODING_RATE4);
    driver.setSignalBandwidth(LORA_SIGNAL_BANDWIDTH);
    driver.setSpreadingFactor(LORA_SPREADING_FACTOR);
    driver.setModeRx();

    // Настройка диода
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    // Инициализация очередей
    loraReciveQueue = xQueueCreate(16, sizeof(loraPacket));
    loraSendQueue = xQueueCreate(16, sizeof(loraPacket));
    processQueue = xQueueCreate(16, sizeof(uint8_t) * RH_RF95_MAX_MESSAGE_LEN);
    wifiSendQueue = xQueueCreate(16, sizeof(String));

    // Инициализация семафора на доступ к информации об устройствах
    devicesInfoMutex = xSemaphoreCreateMutex();
    // Инициализация семафора на подключение к WiFi
    wifiConnectMutex = xSemaphoreCreateMutex();

    // Создание задач
    xTaskCreatePinnedToCore(
        loraReceiveTask,     // Функция задачи
        "LoRa Receive Task", // Имя задачи
        4096,                // Размер стека
        NULL,                // Параметры задачи
        5,                   // Приоритет
        NULL,                // Дескриптор задачи
        0);                  // Ядро

    xTaskCreatePinnedToCore(
        loraSendTask,
        "Send Task",
        4096,
        NULL,
        6,
        NULL,
        0);

    xTaskCreate(
        processPackageTask,
        "Process Package Task",
        8192,
        NULL,
        4,
        NULL);

    xTaskCreate(
        sendToServerTask,
        "Send To Server Task",
        4096,
        NULL,
        3,
        NULL);

    xTaskCreate(
        garbageCollectorTask,
        "Garbage Collector Task",
        4096,
        NULL,
        2,
        NULL);

    xTaskCreate(
        gatePingTask,
        "Gate Ping Task",
        4096,
        NULL,
        1,
        NULL);

    xTaskCreate(
        displayTask,
        "Display Task",
        4096,
        NULL,
        1,
        NULL);

    xTaskCreate(
        timeSyncTask,
        "Time Sync Task",
        4096,
        NULL,
        2,
        NULL);

    xTaskCreate(
        webServerTask,
        "Web Server Task",
        8192,
        NULL,
        1,
        NULL);
}

void loop()
{
    // Пусто, все работает в задачах FreeRTOS
    vTaskDelay(portMAX_DELAY);
}
