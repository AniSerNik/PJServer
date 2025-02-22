// Copyright [2025] Мальцев Максим Дмитриевич <maksdm007@gmail.com>

#include "headers/pins.h"
#include "headers/display.h"
#include "headers/settings.h"
#include "headers/dataprocess.h"
#include "headers/wifi_d.h"
#include <GyverOLED.h>
#include <WiFi.h>

GyverOLED<SSD1306_128x64> oled;

void displayTask(void *pvParameters)
{
    oled.init(SDA, SCL);
    oled.clear();
    oled.update();
    u8_t day = 0;

    while (1)
    {
        oled.clear();
        oled.home();

        char printStr[64];
        if (getLocalTime(&timeinfo))
        {
            strftime(printStr, sizeof(printStr), "%d-%m-%Y %H:%M:%S", &timeinfo);
            oled.print(printStr);
        }
        else
        {
            oled.print("Не синхронизировано");
        }

        if (day != timeinfo.tm_mday)
        {
            day = timeinfo.tm_mday;
            receivedPacketCounter = 0;
        }

        oled.setCursor(0, 1);
        oled.print("Принято: ");
        oled.print(receivedPacketCounter);

        oled.setCursor(0, 2);
        oled.print("Последний: id:0x");
        oled.print(lastPacketData.from, HEX);

        oled.setCursor(0, 3);
        oled.print("RSSI:");
        oled.print(lastPacketData.rssi);
        oled.print(" ");
        strftime(printStr, sizeof(printStr), "%H:%M:%S", &lastPacketData.date);
        oled.print(printStr);

        if ((WiFi.getMode() & WIFI_AP) == WIFI_AP)
        {
            oled.setCursor(0, 5);
            oled.print("SSID: ");
            oled.print(WiFi.softAPSSID());
            oled.setCursor(0, 6);
            oled.print("Пароль: ");
            oled.print(AP_PASSWORD);
            oled.setCursor(0, 7);
            oled.print("IP: ");
            oled.print(WiFi.softAPIP());
        }
        else
        {
            oled.setCursor(0, 5);
            oled.print("SSID: ");
            oled.print(WiFi.SSID());
            oled.setCursor(0, 6);
            oled.print("IP: ");
            oled.print(WiFi.localIP());
            oled.setCursor(0, 7);
            oled.print("RSSI: ");
            oled.print(WiFi.RSSI());
        }

        oled.update();
        vTaskDelay(DISPLAY_INTERVAL / portTICK_PERIOD_MS);
    }
}