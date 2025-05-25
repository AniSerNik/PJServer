// Copyright [2025] <>

#ifdef JDE_CLIENT_BUILD

#include "JDE_client.h"
#include <algorithm>
#include <string>
#include <vector>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Regexp.h>

// Статические переменные для управления логированием библиотеки
static bool jde_client_debug_print = false;
static bool jde_client_encode_print = false;

void JDESetLogging(bool debugPrint, bool encodePrint) {
    jde_client_debug_print = debugPrint;
    jde_client_encode_print = encodePrint;
}

// Макросы для использования внутри JDE_client.cpp
#define SerialP1_JDE if (jde_client_debug_print) Serial
#define SerialP2_JDE if (jde_client_encode_print) Serial

static JsonDocument jsonDoc;                //< JSON Document для хранения JSON
static std::vector<std::string> jsonKeysCl; //< Массив с ключами    

/* Получение ключей */
/**
 * @brief Получает ключи из JSON Object
 * @details Получает ключи из JSON Object и сохраняет их в массив с ключами
 * @param embjson JSON Object
 */
static void _getKeysEmbeddedObject(const JsonObject &embjson);

/**
 * @brief Получает ключи из JSON Array
 * @details Получает ключи из JSON Array и сохраняет их в массив с ключами
 * @param embjson JSON Array
 */
static void _getKeysEmbeddedArray(const JsonArray &embjson);

/* Кодирование JSON */
/**
 * @brief Кодирует JSON Object
 * @details Кодирует JSON Object и сохраняет его в буфер для отправки на сервер
 * @param embjson JSON Object
 * @param isTopObj Флаг, указывающий, является ли JSON Object верхним объектом
 */
static void _encodeJsonObject(const JsonObject &embjson, bool isTopObj = false);

/**
 * @brief Кодирует JSON Array
 * @details Кодирует JSON Array и сохраняет его в буфер для отправки на сервер
 * @param embjson JSON Array
 */
static void _encodeJsonArray(const JsonArray &embjson);

/**
 * @brief Кодирует значение JSON
 * @details Кодирует значение и сохраняет его в буфер для отправки на сервер
 * @param val Значение JSON
 */
static void _encodeJsonValue(const char *val);


bool jsonStringProcess(String str) {
  DeserializationError err = deserializeJson(jsonDoc, str, DeserializationOption::NestingLimit(JSON_MAX_NESTEDOBJECT + 1));
  if (err) {
    Serial.println("JDE_client: deserializeJson() failed with code " + String(err.f_str()));
    return false;
  }
  return true;
}

/* Получение ключей */

void parseJsonKeys() {
  jsonKeysCl.clear();
  _getKeysEmbeddedObject(jsonDoc.as<JsonObject>());
}

static void _getKeysEmbeddedObject(const JsonObject &embjson) {
  for (JsonPair kv : embjson) {
    if(std::find(jsonKeysCl.begin(), jsonKeysCl.end(), kv.key().c_str()) == jsonKeysCl.end())
      jsonKeysCl.push_back(kv.key().c_str());
    if (kv.value().is<JsonArray>())
      _getKeysEmbeddedArray(kv.value().as<JsonArray>());
    else if (kv.value().is<JsonObject>())
      _getKeysEmbeddedObject(kv.value().as<JsonObject>());
  }
}

static void _getKeysEmbeddedArray(const JsonArray &embjson) {
  for (int i = 0; i < embjson.size(); i++) {
    if (embjson[i].is<JsonArray>())
      _getKeysEmbeddedArray(embjson[i].as<JsonArray>());
    else if (embjson[i].is<JsonObject>())
      _getKeysEmbeddedObject(embjson[i].as<JsonObject>());
  }
}

void encodeJsonKeys() {
  for (int i = 0; i < jsonKeysCl.size(); i++) {
    const char* key = jsonKeysCl[i].c_str();
    if(strlen(key) + 1 > sizeof(send_buf)) {
      Serial.println("JDE_client: Buffer overflow during key encoding");
      return;
    }
    strcpy((char*)&send_buf[send_buf[BYTE_COUNT]], key);  // NOLINT
    send_buf[BYTE_COUNT] += strlen(key);
    strcpy((char*)&send_buf[send_buf[BYTE_COUNT]++], " ");  // NOLINT
    SerialP1_JDE.println(String(i) + "\t" + String(key));
  }
}

/* Кодирование JSON */
void encodeJsonForSending() {
  _encodeJsonObject(jsonDoc.as<JsonObject>(), true);
}

static void _encodeJsonObject(const JsonObject &embjson, bool isTopObj) {
  if(!isTopObj) {
    send_buf[send_buf[BYTE_COUNT]++] = SPECIFIC | JSON_OBJECT;
    SerialP2_JDE.println(F("object"));
  }
  for (JsonPair kv : embjson) {
    auto it = find(jsonKeysCl.begin(), jsonKeysCl.end(), kv.key().c_str()); 
    if (it == jsonKeysCl.end()) {
      Serial.print("JDE_client: error find key. Replace it before release version");
      return;
    }
    uint8_t idKey = it - jsonKeysCl.begin();
    SerialP2_JDE.print(kv.key().c_str());
    SerialP2_JDE.print(F(" : "));
    send_buf[send_buf[BYTE_COUNT]++] = idKey;
    if (kv.value().is<JsonArray>())
      _encodeJsonArray(kv.value().as<JsonArray>());
    else if (kv.value().is<JsonObject>())
      _encodeJsonObject(kv.value().as<JsonObject>());
    else
      _encodeJsonValue(kv.value().as<String>().c_str());
  }
  if(!isTopObj)
    send_buf[send_buf[BYTE_COUNT]++] = JSON_LEVEL_UP;
}

static void _encodeJsonArray(const JsonArray &embjson) {
  SerialP2_JDE.println(F("array"));
  send_buf[send_buf[BYTE_COUNT]++] = SPECIFIC | JSON_ARRAY;
  for (int i = 0; i < embjson.size(); i++) {
    if (embjson[i].is<JsonArray>())
      _encodeJsonArray(embjson[i].as<JsonArray>());
    else if (embjson[i].is<JsonObject>())
      _encodeJsonObject(embjson[i].as<JsonObject>());
    else
      _encodeJsonValue(embjson[i].as<String>().c_str());
  }
  send_buf[send_buf[BYTE_COUNT]++] = JSON_LEVEL_UP;
}

static void _encodeJsonValue(const char *val) {
  if(val == NULL)
    return;
  SerialP2_JDE.print(val);
  SerialP2_JDE.print(F(" is a "));
  uint8_t tag = 0x00;
  char value[JSON_MAX_LEN_FIELD];
  strlcpy(value, val, JSON_MAX_LEN_FIELD);
  MatchState ms;
  ms.Target(value);
  if(ms.Match("^%s*%-?%d+%.%d+%s*$") == REGEXP_MATCHED) {
    SerialP2_JDE.print(F("custom float "));
    tag |= CUSTOM_FLOAT;

    char integ[13], frac[16];
    int ret = sscanf(value, " %12[0-9-].%15[0-9]", integ, frac);
    if (ret != 2) {
      Serial.println(F("JDE_client: input error, sending as string"));
      goto send_as_str;      
    }
    uint8_t decimal_places = strlen(frac);
    tag |= decimal_places;
    
    char integfrac[28];
    strcpy(integfrac, integ); // NOLINT
    strcat(integfrac, frac);  // NOLINT

    errno = 0;
    int equiv_int = strtol(integfrac, NULL, 10);
    if (errno != 0) {
      SerialP2_JDE.println(F("; conversion error, sending as string"));
      goto send_as_str;
    }
    
    if (INT8_MIN <= equiv_int && equiv_int <= INT8_MAX) {
      SerialP2_JDE.println(F("8 bit"));
      tag |= _8BIT;
      if(send_buf[BYTE_COUNT]+2 <= sizeof(send_buf)) {
        send_buf[send_buf[BYTE_COUNT]++] = tag;
        send_buf[send_buf[BYTE_COUNT]++] = equiv_int;
      }
    } else if (INT16_MIN <= equiv_int && equiv_int <= INT16_MAX) {
      SerialP2_JDE.println(F("16 bit"));
      tag |= _16BIT;
      if(send_buf[BYTE_COUNT]+3 <= sizeof(send_buf)) {
        send_buf[send_buf[BYTE_COUNT]++] = tag;
        send_buf[send_buf[BYTE_COUNT]++] = equiv_int & 0x00FF;
        send_buf[send_buf[BYTE_COUNT]++] = (equiv_int & 0xFF00) >> 8;
      }
    } else if (INT32_MIN <= equiv_int && equiv_int <= INT32_MAX) {
      SerialP2_JDE.println(F("32 bit"));
      tag |= _32BIT;
      if(send_buf[BYTE_COUNT]+5 <= sizeof(send_buf)) {
        send_buf[send_buf[BYTE_COUNT]++] = tag;
        for(int j = 0; j < 4; j++) {
          send_buf[send_buf[BYTE_COUNT]++] = equiv_int & 0x00FF;
          equiv_int = equiv_int >> 8;
        }
      }
    }
  } else if (ms.Match("^%s*%-?%d+%s*$") == REGEXP_MATCHED) {
    SerialP2_JDE.print(F("integer "));
    tag |= INTEGER;
    errno = 0;
    int num = strtol(value, NULL, 10);
    if (errno != 0) {
      SerialP2_JDE.println(F("; conversion error, sending as string"));
      goto send_as_str;
    }

    if (INT8_MIN <= num && num <= INT8_MAX) {
      SerialP2_JDE.println(F("8 bit"));
      tag |= _8BIT;
      if(send_buf[BYTE_COUNT]+2 <= sizeof(send_buf)) {
        send_buf[send_buf[BYTE_COUNT]++] = tag;
        send_buf[send_buf[BYTE_COUNT]++] = num;
      }
    } else if (INT16_MIN <= num && num <= INT16_MAX) {
      SerialP2_JDE.println(F("16 bit"));
      tag |= _16BIT;
      if(send_buf[BYTE_COUNT]+3 <= sizeof(send_buf)) {
        send_buf[send_buf[BYTE_COUNT]++] = tag;
        send_buf[send_buf[BYTE_COUNT]++] = num & 0x00FF;
        send_buf[send_buf[BYTE_COUNT]++] = (num & 0xFF00) >> 8;
      }
    } else if (INT32_MIN <= num && num <= INT32_MAX) {
      SerialP2_JDE.println(F("32 bit"));
      tag |= _32BIT;
      if(send_buf[BYTE_COUNT]+5 <= sizeof(send_buf)) {
        send_buf[send_buf[BYTE_COUNT]++] = tag;
        for(int j = 0; j < 4; j++) {
          send_buf[send_buf[BYTE_COUNT]++] = num & 0x00FF;
          num = num >> 8;
        }
      }
    }
  } else if (ms.Match("^%s*%x%x:%x%x:%x%x:%x%x:%x%x:%x%x%s*$") == REGEXP_MATCHED) {
    SerialP2_JDE.println(F("MAC"));
    tag |= SPECIFIC | MAC_ADDR;
    unsigned char mac[6];
    sscanf(value, " %hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
    if(send_buf[BYTE_COUNT]+7 <= sizeof(send_buf)) {
      send_buf[send_buf[BYTE_COUNT]++] = tag;
      for(int i = 0; i < 6; i++)
        send_buf[send_buf[BYTE_COUNT]++] = mac[i];
    }   
  } else if(ms.Match("^%s*%d?%d?%d%.%d?%d?%d%.%d?%d?%d%.%d?%d?%d%s*$") == REGEXP_MATCHED) {
    SerialP2_JDE.println(F("IP"));
    tag |= SPECIFIC | IP_ADDR;
    unsigned char ip_val[4];
    sscanf(value, " %hhu.%hhu.%hhu.%hhu", &ip_val[0], &ip_val[1], &ip_val[2], &ip_val[3]);
    
    if(send_buf[BYTE_COUNT]+5 <= sizeof(send_buf)) {
      send_buf[send_buf[BYTE_COUNT]++] = tag;
      for(int i = 0; i < 4; i++)
        send_buf[send_buf[BYTE_COUNT]++] = ip_val[i];
    }   
  } else if(ms.Match("^%s*%d%d%d%d%-%d%d%-%d%d%s*$") == REGEXP_MATCHED) {
    SerialP2_JDE.println(F("date"));
    tag |= SPECIFIC | DATE_YYYYMMDD;
    unsigned int year;
    unsigned char month, day;
    sscanf(value, " %u-%hhu-%hhu", &year, &month, &day);
    if(send_buf[BYTE_COUNT]+5 <= sizeof(send_buf)) {
      send_buf[send_buf[BYTE_COUNT]++] = tag;
      send_buf[send_buf[BYTE_COUNT]++] = year & 0x00FF;     
      send_buf[send_buf[BYTE_COUNT]++] = (year & 0xFF00) >> 8;
      send_buf[send_buf[BYTE_COUNT]++] = month;
      send_buf[send_buf[BYTE_COUNT]++] = day;
    }
  } else if(ms.Match("^%s*%d%d:%d%d:%d%d%s*$") == REGEXP_MATCHED) {
    SerialP2_JDE.println(F("time"));
    tag |= SPECIFIC | TIME_HHMMSS;
    unsigned char hours, minutes, seconds;
    sscanf(value, " %hhu:%hhu:%hhu", &hours, &minutes, &seconds);
    if(send_buf[BYTE_COUNT]+4 <= sizeof(send_buf)) {
      send_buf[send_buf[BYTE_COUNT]++] = tag;
      send_buf[send_buf[BYTE_COUNT]++] = hours;
      send_buf[send_buf[BYTE_COUNT]++] = minutes;
      send_buf[send_buf[BYTE_COUNT]++] = seconds;
    }
  } else {
    send_as_str:
    SerialP2_JDE.println(F("string"));
    tag = STRING;
    uint8_t value_len = strlen(value) + 1;
    if (send_buf[BYTE_COUNT] + value_len < sizeof(send_buf)) {         
      send_buf[send_buf[BYTE_COUNT]++] = tag;
      strcpy((char*)&send_buf[send_buf[BYTE_COUNT]], value);  // NOLINT
      send_buf[BYTE_COUNT] += value_len;        
    } else {
        Serial.println("JDE_client: Buffer overflow when sending string value");
    }
  }
}

#endif // JDE_CLIENT_BUILD
