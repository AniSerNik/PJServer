// Copyright [2025] Мальцев Максим Дмитриевич <maksdm007@gmail.com>

#include "headers/pins.h"
#include "headers/display.h"
#include "headers/settings.h"
#include <GyverOLED.h>
#include <WiFi.h>

GyverOLED<SSD1306_128x64> oled;

void displayTask(void *pvParameters)
{
    oled.init(SDA, SCL);
    oled.clear();
    oled.update();

    while (1)
    {
        oled.clear();
        oled.home();
        oled.print("SSID: ");
        oled.print(WiFi.SSID());
        oled.setCursor(0, 1);
        oled.print("IP: ");
        oled.print(WiFi.localIP());
        oled.setCursor(0, 2);
        oled.print("Принято: ");
        oled.print(packetcntr);
        oled.update();
        vTaskDelay(DISPLAY_INTERVAL / portTICK_PERIOD_MS);
    }
}