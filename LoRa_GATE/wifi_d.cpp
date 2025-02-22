// Copyright [2025] Мальцев Максим Дмитриевич <maksdm007@gmail.com>

#include "headers/wifi_d.h"
#include "headers/settings.h"
#include "headers/main.h"
#include "headers/dataprocess.h"
#include <HTTPClient.h>
#include <WebServer.h>

// Очередь для отправки данных на сервер
QueueHandle_t wifiSendQueue;

// Мьютекс для защиты функции wifiConnect
extern SemaphoreHandle_t wifiConnectMutex;

// Структура для хранения информации о времени
struct tm timeinfo;

static WebServer server(80);

// Задача периодической отправки пинга шлюза
void gatePingTask(void *pvParameters)
{
  while (1)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      wifiConnect();
    }

    printf("\nОтправка информации о шлюзе\n");
    String json;
    json = "{\"system\":{";
    json += "\"Akey\":\"" + String(PARAM_AKEY) + "\",";
    json += "\"Serial\":\"" + String(PARAM_SerialDevice) + "\",";
    json += "\"Version\":\"" + String(PARAM_VersionDevice) + "\",";
    json += "\"Recived\":\"" + String(receivedPacketCounter) + "\",";
    json += "\"RSSI\":\"" + String(WiFi.RSSI()) + "\"}";
    json += "}";
    printf("JSON информация о шлюзе:\n%s\n", json.c_str());

    if (xQueueSend(wifiSendQueue, &json, portMAX_DELAY) != pdPASS)
    {
      printf("Ошибка постановки в wifiSendQueue информации о шлюзе\n");
    }

    // Задержка на интервал GATEWORKPING_INTERVAL
    vTaskDelay(GATEWORKPING_INTERVAL / portTICK_PERIOD_MS);
  }
}

// Задача отправки данных на сервер
void sendToServerTask(void *pvParameters)
{
  String json;
  while (1)
  {
    if (xQueueReceive(wifiSendQueue, &json, portMAX_DELAY))
    {
      if (WiFi.status() != WL_CONNECTED)
      {
        wifiConnect();
      }

      // Отправляем данные на все серверы
      for (const String &server : servers)
      {
        if (server.length() > 0)
        {
          WiFiClient client;
          if (WiFi.status() == WL_CONNECTED)
          {
            HTTPClient http;
            http.begin(client, server.c_str());
            http.addHeader("Content-Type", "application/json");
            printf("%s:\n   Ответ HTTP: %i\n", server.c_str(), http.POST(json));
            http.end();
          }
          else
          {
            printf("%s:\nНет Wi-Fi подключения. Данные не переданы.\n", server.c_str());
          }
        }
      }
    }
  }
}

// Задача периодической синхронизации времени
void timeSyncTask(void *pvParameters)
{
  while (1)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      wifiConnect();
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      uint8_t retry_count = 1U;
      while (retry_count <= TIME_SYNC_RETRY_COUNT)
      {
        configTime(UTC_OFFSET, 0U, NET_NTP.c_str());
        if (!getLocalTime(&timeinfo))
        {
          printf("Не удалось получить текущее время %u/%u\n", retry_count, TIME_SYNC_RETRY_COUNT);
          retry_count++;
          vTaskDelay(TIME_SYNC_DELAY / portTICK_PERIOD_MS);
          continue;
        }
        else
        {
          break;
        }
      }
      char timeStr[53];
      strftime(timeStr, sizeof(timeStr), "%d-%m-%Y %H:%M:%S", &timeinfo);
      printf("Синхронизировано задачей: %s\n", timeStr);
    }
    else
    {
      printf("Ошибка синхронизации времени, нет WiFi подключения\n");
    }
    vTaskDelay(TIME_SYNC_INTERVAL / portTICK_PERIOD_MS);
  }
}

// Подключение к Wi-Fi
static void wifiConnect()
{
  if (xSemaphoreTake(wifiConnectMutex, portMAX_DELAY) == pdTRUE)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      WiFi.mode(WIFI_STA);

      uint8_t bestNetworkIndex = -1;
      int16_t bestRSSI = -256; // минимальное значение RSSI

      for (int i = 0; i < MAX_WIFI_NETWORKS; i++)
      {
        if (wifiNetworks[i].ssid.length() == 0)
        {
          continue;
        }

        // Настройка статического IP, если указано
        if (wifiNetworks[i].useStaticSettings)
        {
          if (!WiFi.config(wifiNetworks[i].net_ip, wifiNetworks[i].net_gateway_ip, wifiNetworks[i].net_mask))
          {
            printf("Не удалось выставить настройки подключения к Wi-Fi для сети %s\n", wifiNetworks[i].ssid.c_str());
          }
        }

        WiFi.begin(wifiNetworks[i].ssid.c_str(), wifiNetworks[i].password.c_str());

        printf("Подключение к WiFi сети \"%s\"\n", wifiNetworks[i].ssid.c_str());
        uint8_t retry_count = 0;
        while (WiFi.status() != WL_CONNECTED && retry_count < WIFI_CONNECT_RETRY_COUNT)
        {
          vTaskDelay(WIFI_CONNECT_COOLDOWN / portTICK_PERIOD_MS);
          printf("Попытка подключения к Wi-Fi %i/%i\n", retry_count + 1, WIFI_CONNECT_RETRY_COUNT);
          retry_count++;
        }

        if (WiFi.status() == WL_CONNECTED)
        {
          int rssi = WiFi.RSSI();
          printf("Успешно подключение к WiFi сети \"%s\", RSSI: %d\n", wifiNetworks[i].ssid.c_str(), rssi);
          if (rssi > bestRSSI)
          {
            bestRSSI = rssi;
            bestNetworkIndex = i;
          }
          WiFi.disconnect();
        }
        else
        {
          printf("Ошибка подключения к WiFi сети \"%s\"\n", wifiNetworks[i].ssid.c_str());
          vTaskDelay(WIFI_CONNECT_COOLDOWN / portTICK_PERIOD_MS);
        }
      }

      if (bestNetworkIndex != -1 && wifiNetworks[bestNetworkIndex].ssid.length() != 0)
      {
        vTaskDelay(WIFI_CONNECT_COOLDOWN * 3 / portTICK_PERIOD_MS);
        // Настройка статического IP, если указано
        if (wifiNetworks[bestNetworkIndex].useStaticSettings)
        {
          if (!WiFi.config(wifiNetworks[bestNetworkIndex].net_ip, wifiNetworks[bestNetworkIndex].net_gateway_ip, wifiNetworks[bestNetworkIndex].net_mask))
          {
            printf("Не удалось выставить настройки подключения к Wi-Fi сети с лучшим RSSI \"%s\"\n", wifiNetworks[bestNetworkIndex].ssid.c_str());
          }
        }

        WiFi.begin(wifiNetworks[bestNetworkIndex].ssid.c_str(), wifiNetworks[bestNetworkIndex].password.c_str());

        printf("Подключение к WiFi сети с лучшим RSSI \"%s\"\n", wifiNetworks[bestNetworkIndex].ssid.c_str());
        uint8_t retry_count = 0;
        while (WiFi.status() != WL_CONNECTED && retry_count < WIFI_CONNECT_RETRY_COUNT)
        {
          vTaskDelay(WIFI_CONNECT_COOLDOWN / portTICK_PERIOD_MS);
          printf("Попытка подключения к Wi-Fi сети с лучшим RSSI %i/%i\n", retry_count + 1, WIFI_CONNECT_RETRY_COUNT);
          retry_count++;
        }

        if (WiFi.status() == WL_CONNECTED)
        {
          printf("\nПодключено к сети с лучшим RSSI \"%s\"\n", wifiNetworks[bestNetworkIndex].ssid.c_str());
          printf("IP адрес: %s\nRSSI: %i\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());

          // Синхронизация времени при подключении к Wi-Fi
          uint8_t time_retry_count = 1U;
          while (time_retry_count <= TIME_SYNC_RETRY_COUNT)
          {
            configTime(UTC_OFFSET, 0U, NET_NTP.c_str());
            if (!getLocalTime(&timeinfo))
            {
              printf("Не удалось получить текущее время %u/%u\n", time_retry_count, TIME_SYNC_RETRY_COUNT);
              time_retry_count++;
              vTaskDelay(TIME_SYNC_DELAY / portTICK_PERIOD_MS);
              continue;
            }
            else
            {
              char timeStr[53];
              strftime(timeStr, sizeof(timeStr), "%d-%m-%Y %H:%M:%S", &timeinfo);
              printf("Синхронизировано при подключении: %s\n", timeStr);
              break;
            }
          }
        }
        else
        {
          printf("Ошибка подключения к сети с лучшим RSSI \"%s\"\n", wifiNetworks[bestNetworkIndex].ssid.c_str());
        }
      }
      else
      {
        printf("Не удалось найти подходящую сеть для подключения\n");
        WiFi.mode(WIFI_AP);
        printf("Запуск точки доступа \nSSID: %s\npassword: %s\n", AP_NAME, AP_PASSWORD);
        if (WiFi.softAP(AP_NAME, AP_PASSWORD))
        {
          printf("Точка доступа запущена успешно\n");
          vTaskDelay(NOWIFI_AP_DELAY / portTICK_PERIOD_MS);
        }
        else if (WiFi.softAP("LORAGATE_DEBUG"))
        {
          printf("Точка доступа запущена успешно\n");
          vTaskDelay(NOWIFI_AP_DELAY / portTICK_PERIOD_MS);
        }
        else
        {
          printf("Ошибка запуска точки доступа\n");
        }
      }
    }
    xSemaphoreGive(wifiConnectMutex);
  }
}

void webServerTask(void *pvParameters)
{
  server.on("/", handleRoot);
  server.on("/save", handleSaveSettings);
  server.on("/clear_nvs", HTTP_POST, handleClearNVS);
  server.on("/restart", HTTP_POST, handleRestart);
  server.onNotFound(handleNotFound);
  server.begin();

  while (true)
  {
    server.handleClient();
    vTaskDelay(WEB_SERVER_INTERVAL / portTICK_PERIOD_MS);
  }
}

static void handleRestart()
{
  server.send(200, "text/html", "<html><head><meta charset='UTF-8'></head><body><h1>Перезагрузка...</h1><a href='/'>Ожидание перезагрузки</a></body></html>");
  ESP.restart();
}

static void handleNotFound()
{
  server.send(404, "text/html", "<html><head><meta charset='UTF-8'></head><body><h1>Страница не найдена</h1></body></html>");
}

static void handleClearNVS()
{
  if (clearNVS())
    server.send(200, "text/html", "<html><head><meta charset='UTF-8'></head><body><h1>Память очищена</h1><a href='/'>Назад</a></body></html>");
  else
    server.send(200, "text/html", "<html><head><meta charset='UTF-8'></head><body><h1>Ошибка очистки памяти</h1><a href='/'>Назад</a></body></html>");
}

static void handleRoot()
{
  String html = "<html><head><meta charset='UTF-8'></head><body>";
  html += "<h1>Настройки</h1>";
  html += "<form action='/save' method='POST'>";

  html += "LORA_TXPOWER: <input type='text' name='LORA_TXPOWER' value='" + String(LORA_TXPOWER) + "'><br>";
  html += "LORA_FREQUENCY: <input type='text' name='LORA_FREQUENCY' value='" + String(LORA_FREQUENCY) + "'><br>";
  html += "LORA_CODING_RATE4: <input type='text' name='LORA_CODING_RATE4' value='" + String(LORA_CODING_RATE4) + "'><br>";
  html += "LORA_SIGNAL_BANDWIDTH: <input type='text' name='LORA_SIGNAL_BANDWIDTH' value='" + String(LORA_SIGNAL_BANDWIDTH) + "'><br>";
  html += "LORA_SPREADING_FACTOR: <input type='text' name='LORA_SPREADING_FACTOR' value='" + String(LORA_SPREADING_FACTOR) + "'><br>";
  html += "LORA_INTERVAL: <input type='text' name='LORA_INTERVAL' value='" + String(LORA_INTERVAL) + "'><br>";

  html += "SERVER_ADDRESS: <input type='text' name='SERVER_ADDRESS' value='" + String(SERVER_ADDRESS) + "'><br>";
  html += "PARAM_AKEY: <input type='text' name='PARAM_AKEY' value='" + PARAM_AKEY + "'><br>";
  html += "PARAM_VersionDevice: <input type='text' name='PARAM_VersionDevice' value='" + PARAM_VersionDevice + "'><br>";

  html += "WEB_SERVER_INTERVAL: <input type='text' name='WEB_SERVER_INTERVAL' value='" + String(WEB_SERVER_INTERVAL) + "'><br>";

  html += "GARBAGE_COLLECT_COOLDOWN: <input type='text' name='GARBAGE_COLLECT_COOLDOWN' value='" + String(GARBAGE_COLLECT_COOLDOWN) + "'><br>";
  html += "TIME_SYNC_INTERVAL: <input type='text' name='TIME_SYNC_INTERVAL' value='" + String(TIME_SYNC_INTERVAL) + "'><br>";
  html += "DATACOL_TIMESTORE: <input type='text' name='DATACOL_TIMESTORE' value='" + String(DATACOL_TIMESTORE) + "'><br>";
  html += "GATEWORKPING_INTERVAL: <input type='text' name='GATEWORKPING_INTERVAL' value='" + String(GATEWORKPING_INTERVAL) + "'><br>";
  html += "DISPLAY_INTERVAL: <input type='text' name='DISPLAY_INTERVAL' value='" + String(DISPLAY_INTERVAL) + "'><br>";
  html += "TIME_SYNC_RETRY_COUNT: <input type='text' name='TIME_SYNC_RETRY_COUNT' value='" + String(TIME_SYNC_RETRY_COUNT) + "'><br>";
  html += "WIFI_CONNECT_RETRY_COUNT: <input type='text' name='WIFI_CONNECT_RETRY_COUNT' value='" + String(WIFI_CONNECT_RETRY_COUNT) + "'><br>";
  html += "WIFI_CONNECT_COOLDOWN: <input type='text' name='WIFI_CONNECT_COOLDOWN' value='" + String(WIFI_CONNECT_COOLDOWN) + "'><br>";
  html += "AP_NAME: <input type='text' name='AP_NAME' value='" + AP_NAME + "'><br>";
  html += "AP_PASSWORD: <input type='text' name='AP_PASSWORD' value='" + AP_PASSWORD + "'><br>";
  html += "NOWIFI_AP_DELAY: <input type='text' name='NOWIFI_AP_DELAY' value='" + String(NOWIFI_AP_DELAY) + "'><br>";
  html += "TIME_SYNC_DELAY: <input type='text' name='TIME_SYNC_DELAY' value='" + String(TIME_SYNC_DELAY) + "'><br>";
  html += "PARAM_SerialDevice: <input type='text' name='PARAM_SerialDevice' value='" + PARAM_SerialDevice + "'><br>";
  html += "NET_NTP: <input type='text' name='NET_NTP' value='" + NET_NTP + "'><br>";
  html += "UTC_OFFSET: <input type='text' name='UTC_OFFSET' value='" + String(UTC_OFFSET) + "'><br>";

  for (int i = 0; i < MAX_WIFI_NETWORKS; i++)
  {
    if (wifiNetworks[i].ssid.length() > 0)
    {
      html += "<fieldset><legend>WiFi Сеть " + String(i + 1) + "</legend>";
      html += "SSID: <input type='text' name='wifi_ssid" + String(i) + "' value='" + wifiNetworks[i].ssid + "'><br>";
      html += "Password: <input type='text' name='wifi_password" + String(i) + "' value='" + wifiNetworks[i].password + "'><br>";
      html += "Use Static Settings: <input type='checkbox' name='wifi_useStatic" + String(i) + "' " + (wifiNetworks[i].useStaticSettings ? "checked" : "") + "><br>";
      html += "IP: <input type='text' name='wifi_net_ip" + String(i) + "' value='" + wifiNetworks[i].net_ip.toString() + "'><br>";
      html += "Gateway: <input type='text' name='wifi_net_gateway_ip" + String(i) + "' value='" + wifiNetworks[i].net_gateway_ip.toString() + "'><br>";
      html += "Mask: <input type='text' name='wifi_net_mask" + String(i) + "' value='" + wifiNetworks[i].net_mask.toString() + "'><br>";
      // Флажок для удаления записи
      html += "Удалить: <input type='checkbox' name='delete_wifi" + String(i) + "'><br>";
      html += "</fieldset>";
    }
  }

  html += "<button type='button' onclick='addNetwork()'>Добавить сеть</button>";
  html += "<div id='newNetwork'></div>";
  html += "<script>";
  html += "function addNetwork() {";
  html += "var div = document.getElementById('newNetwork');";
  html += "div.innerHTML = \"<fieldset><legend>Новая WiFi сеть</legend>\" +";
  html += "\"SSID: <input type='text' name='new_wifi_ssid'><br>\" +";
  html += "\"Пароль: <input type='text' name='new_wifi_password'><br>\" +";
  html += "\"Использовать статические настройки: <input type='checkbox' name='new_wifi_useStatic'><br>\" +";
  html += "\"IP адрес: <input type='text' name='new_wifi_net_ip'><br>\" +";
  html += "\"Шлюз: <input type='text' name='new_wifi_net_gateway_ip'><br>\" +";
  html += "\"Маска: <input type='text' name='new_wifi_net_mask'><br>\" +";
  html += "\"</fieldset>\";";
  html += "}";
  html += "</script>";

  for (int i = 0; i < MAX_SERVERS; i++)
  {
    if (servers[i].length() > 0)
    {
      html += "<fieldset><legend>Сервер " + String(i + 1) + "</legend>";
      html += "<input type='text' name='server" + String(i) + "' value='" + servers[i] + "'><br>";
      html += "Удалить: <input type='checkbox' name='delete_server" + String(i) + "'><br>";
      html += "</fieldset>";
    }
  }

  html += "<button type='button' onclick='addServer()'>Добавить сервер</button>";
  html += "<div id='newServer'></div>";

  html += "<script>";
  html += "function addServer() {";
  html += "var div = document.getElementById('newServer');";
  html += "div.innerHTML = \"<fieldset><legend>Новый сервер</legend>\" +";
  html += "\"<input type='text' name='new_server'><br>\" +";
  html += "\"</fieldset>\";";
  html += "}";
  html += "</script>";

  html += "<input type='submit' value='Сохранить'>";
  html += "</form>";
  html += "</body></html>";

  html += "<form action='/clear_nvs' method='POST' style='margin-top:50px'>";
  html += "<input type='submit' value='Сброс настроек'>";
  html += "</form>";

  html += "<form action='/restart' method='POST'>";
  html += "<input type='submit' value='Перезагрузить'>";
  html += "</form>";

  server.send(200, "text/html", html);
}

static void handleSaveSettings()
{
  if (server.method() == HTTP_POST)
  {
    LORA_TXPOWER = server.arg("LORA_TXPOWER").toInt();
    LORA_FREQUENCY = server.arg("LORA_FREQUENCY").toFloat();
    LORA_CODING_RATE4 = server.arg("LORA_CODING_RATE4").toInt();
    LORA_SIGNAL_BANDWIDTH = server.arg("LORA_SIGNAL_BANDWIDTH").toInt();
    LORA_SPREADING_FACTOR = server.arg("LORA_SPREADING_FACTOR").toInt();
    LORA_INTERVAL = server.arg("LORA_INTERVAL").toInt();

    WEB_SERVER_INTERVAL = server.arg("WEB_SERVER_INTERVAL").toInt();

    SERVER_ADDRESS = server.arg("SERVER_ADDRESS").toInt();
    GARBAGE_COLLECT_COOLDOWN = server.arg("GARBAGE_COLLECT_COOLDOWN").toInt();
    TIME_SYNC_INTERVAL = server.arg("TIME_SYNC_INTERVAL").toInt();
    DATACOL_TIMESTORE = server.arg("DATACOL_TIMESTORE").toInt();
    GATEWORKPING_INTERVAL = server.arg("GATEWORKPING_INTERVAL").toInt();
    DISPLAY_INTERVAL = server.arg("DISPLAY_INTERVAL").toInt();
    TIME_SYNC_RETRY_COUNT = server.arg("TIME_SYNC_RETRY_COUNT").toInt();
    WIFI_CONNECT_RETRY_COUNT = server.arg("WIFI_CONNECT_RETRY_COUNT").toInt();
    WIFI_CONNECT_COOLDOWN = server.arg("WIFI_CONNECT_COOLDOWN").toInt();
    AP_NAME = server.arg("AP_NAME");
    AP_PASSWORD = server.arg("AP_PASSWORD");
    NOWIFI_AP_DELAY = server.arg("AP_DELAY").toInt();
    TIME_SYNC_DELAY = server.arg("TIME_SYNC_DELAY").toInt();
    PARAM_SerialDevice = server.arg("PARAM_SerialDevice");
    PARAM_AKEY = server.arg("PARAM_AKEY");
    PARAM_VersionDevice = server.arg("PARAM_VersionDevice");
    NET_NTP = server.arg("NET_NTP");
    UTC_OFFSET = server.arg("UTC_OFFSET").toInt();

    for (int i = 0; i < MAX_WIFI_NETWORKS; i++)
    {
      if (server.hasArg("delete_wifi" + String(i)))
      {
        // Запись удаляется – сбрасываем значения
        wifiNetworks[i].ssid = "";
        wifiNetworks[i].password = "";
        wifiNetworks[i].useStaticSettings = false;
        wifiNetworks[i].net_ip = IPAddress(0, 0, 0, 0);
        wifiNetworks[i].net_gateway_ip = IPAddress(0, 0, 0, 0);
        wifiNetworks[i].net_mask = IPAddress(0, 0, 0, 0);
      }
      else
      {
        // Если запись не удаляется – считываем данные из формы
        wifiNetworks[i].ssid = server.arg("wifi_ssid" + String(i));
        wifiNetworks[i].password = server.arg("wifi_password" + String(i));
        wifiNetworks[i].useStaticSettings = server.hasArg("wifi_useStatic" + String(i));
        wifiNetworks[i].net_ip.fromString(server.arg("wifi_net_ip" + String(i)));
        wifiNetworks[i].net_gateway_ip.fromString(server.arg("wifi_net_gateway_ip" + String(i)));
        wifiNetworks[i].net_mask.fromString(server.arg("wifi_net_mask" + String(i)));
      }
    }

    if (server.hasArg("new_wifi_ssid") && server.arg("new_wifi_ssid").length() > 0)
    {
      for (int i = 0; i < MAX_WIFI_NETWORKS; i++)
      {
        if (wifiNetworks[i].ssid.length() == 0)
        {
          wifiNetworks[i].ssid = server.arg("new_wifi_ssid");
          wifiNetworks[i].password = server.arg("new_wifi_password");
          wifiNetworks[i].useStaticSettings = server.hasArg("new_wifi_useStatic");
          wifiNetworks[i].net_ip.fromString(server.arg("new_wifi_net_ip"));
          wifiNetworks[i].net_gateway_ip.fromString(server.arg("new_wifi_net_gateway_ip"));
          wifiNetworks[i].net_mask.fromString(server.arg("new_wifi_net_mask"));
          break;
        }
      }
    }

    for (int i = 0; i < MAX_SERVERS; i++)
    {
      if (server.hasArg("delete_server" + String(i)))
      {
        servers[i] = "";
      }
      else
      {
        servers[i] = server.arg("server" + String(i));
      }
    }

    if (server.hasArg("new_server") && server.arg("new_server").length() > 0)
    {
      for (int i = 0; i < MAX_SERVERS; i++)
      {
        if (servers[i].isEmpty())
        {
          servers[i] = server.arg("new_server");
          break;
        }
      }
    }

    saveSettings();

    server.send(200, "text/html", "<html><head><meta charset='UTF-8'></head><body><h1>Настройки сохранены</h1><a href='/'>Назад</a></body></html>");
  }
  else
  {
    server.send(405, "text/html", "<html><head><meta charset='UTF-8'></head><body><h1>Метод не разрешен</h1></body></html>");
  }
}