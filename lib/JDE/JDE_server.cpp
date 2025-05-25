// Copyright [2025] <>

#include <Arduino.h>
#include "JDE_server.h"

// Глобальная карта для хранения информации об устройствах
std::unordered_map<uint8_t, deviceInfo> devicesInfo;

// Получение ключа по ID
String getKeyFromId(uint8_t idKey, uint8_t dev_id)
{
    auto it = devicesInfo.find(dev_id);
    if (it != devicesInfo.end())
    {
        auto res = it->second.maskKeys.find(idKey);
        if (res != it->second.maskKeys.end())
        {
            return String(res->second.c_str());
        }
    }
  return "NotRegisteredKey";
}

// Декодирование JSON из байтов
String decodeJsonFromBytes(uint8_t *recv_buf, uint8_t dev_id, int16_t lastRssi)
{
  uint8_t nestedObject[JSON_MAX_NESTEDOBJECT], nesObjCount = 0;
  String result = "{";
  bool isSystemObject = false; // Флаг для отслеживания объекта "system"

  for (int i = START_PAYLOAD; i < recv_buf[BYTE_COUNT];)
  {
    uint8_t key_id = recv_buf[i++];
    if (key_id == JSON_LEVEL_UP)
    {
      if (nesObjCount == 0)
      {
        printf("Ошибка вложенного объекта!\n");
        break;
      }
      nesObjCount--;
      if (nestedObject[nesObjCount] == JSON_OBJECT)
      {
        // Если "system", добавляем RSSI
        if (nesObjCount == 0 && isSystemObject)
        {
          result += ", \"RSSI\": \"" + String(lastRssi) + "\"";
          isSystemObject = false;
        }
        result += '}';
      }
      else if (nestedObject[nesObjCount] == JSON_ARRAY)
        result += ']';
      if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
        result += ", ";
      continue;
    }
    uint8_t tag;
    if (nesObjCount > 0 && nestedObject[nesObjCount - 1] == JSON_ARRAY)
      tag = key_id;
    else
    {
      result += '\"' + getKeyFromId(key_id, dev_id) + '\"' + ": ";

      // Обнаруживаем "system"
      if (getKeyFromId(key_id, dev_id) == "system")
        isSystemObject = true;

      tag = recv_buf[i++];
    }
    switch (tag & GENERAL_TYPE_MASK)
    {
    case SPECIFIC:
    {
      switch (tag & SPECIFIC_TYPE_MASK)
      {
      case JSON_OBJECT:
      {
        result += '{';
        if (nesObjCount == JSON_MAX_NESTEDOBJECT)
        {
          printf("Ошибка. Превышено число вложенных объектов\n");
          continue;
        }
        nestedObject[nesObjCount] = JSON_OBJECT;
        nesObjCount++;
        continue;
      }
      case JSON_ARRAY:
      {
        result += '[';
        if (nesObjCount == JSON_MAX_NESTEDOBJECT)
        {
          printf("Ошибка. Превышено число вложенных объектов\n");
          continue;
        }
        nestedObject[nesObjCount] = JSON_ARRAY;
        nesObjCount++;
        continue;
      }
      case IP_ADDR:
      {
        uint8_t ip[4];
        char ip_str[16];
        for (int j = 0; j < 4; j++)
          ip[j] = recv_buf[i++];
        snprintf(ip_str, sizeof(ip_str), "%hhu.%hhu.%hhu.%hhu", ip[0], ip[1], ip[2], ip[3]);
        result += '\"' + String(ip_str) + '\"';
        if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
          result += ", ";
        continue;
      }
      case MAC_ADDR:
      {
        uint8_t mac[6];
        char mac_str[18];
        for (int j = 0; j < 6; j++)
          mac[j] = recv_buf[i++];
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        result += '\"' + String(mac_str) + '\"';
        if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
          result += ", ";
        continue;
      }
      case DATE_YYYYMMDD:
      {
        uint16_t year = 0;
        year |= recv_buf[i++];
        year |= recv_buf[i++] << 8;
        uint8_t mon = recv_buf[i++];
        uint8_t day = recv_buf[i++];
        char date[11];
        snprintf(date, sizeof(date), "%d-%02hhu-%02hhu", year, mon, day);
        result += '\"' + String(date) + '\"';
        if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
          result += ", ";
        continue;
      }
      case TIME_HHMMSS:
      {
        uint8_t hour = recv_buf[i++];
        uint8_t min = recv_buf[i++];
        uint8_t sec = recv_buf[i++];
        char time[11];
        snprintf(time, sizeof(time), "%02hhu:%02hhu:%02hhu", hour, min, sec);

        result += '\"' + String(time) + '\"';
        if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
          result += ", ";
        continue;
      }
      }
    }
    case STRING:
    {
      char unpack_buf_cstr[JSON_MAX_LEN_FIELD];
      snprintf(unpack_buf_cstr, sizeof(unpack_buf_cstr), "%s", reinterpret_cast<char *>(&recv_buf[i]));
      i += strlen(reinterpret_cast<char *>(&recv_buf[i])) + 1;
      result += '\"' + String(unpack_buf_cstr) + '\"';
      if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
        result += ", ";
      continue;
    }
    case INTEGER:
    {
      switch (tag & NUMBER_SIZE_MASK)
      {
      case _8BIT:
      {
        int8_t num = (int8_t)recv_buf[i++];
        result += String(num);
        if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
          result += ", ";
        continue;
      }
      case _16BIT:
      {
        int16_t num = 0;
        num |= recv_buf[i++];
        num |= recv_buf[i++] << 8;
        result += String(num);
        if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
          result += ", ";
        continue;
      }
      case _32BIT:
      {
        int32_t num = 0;
        for (int j = 0; j < 32; j += 8)
          num |= recv_buf[i++] << j;
        result += String(num);
        if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
          result += ", ";
        continue;
      }
      }
    }
    case CUSTOM_FLOAT:
    {
      switch (tag & NUMBER_SIZE_MASK)
      {
      case _8BIT:
      {
        int8_t num = (int8_t)recv_buf[i++];
        uint8_t dec_places = tag & DECIMAL_POINT_MASK;
        int8_t integer = num / quick_pow10(dec_places);
        uint8_t fraction = abs(num) % quick_pow10(dec_places);
        char num_str[18];
        snprintf(num_str, sizeof(num_str), "%d.%0*d", integer, dec_places, fraction);
        if (integer == 0 && num < 0)
          result += '-';
        result += String(num_str);
        if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
          result += ", ";
        continue;
      }
      case _16BIT:
      {
        int16_t num = 0;
        num |= recv_buf[i++];
        num |= recv_buf[i++] << 8;
        uint8_t dec_places = tag & DECIMAL_POINT_MASK;
        int16_t integer = num / quick_pow10(dec_places);
        uint16_t fraction = abs(num) % quick_pow10(dec_places);
        char num_str[18];
        snprintf(num_str, sizeof(num_str), "%d.%0*d", integer, dec_places, fraction);
        if (integer == 0 && num < 0)
          result += '-';
        result += String(num_str);
        if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
          result += ", ";
        continue;
      }
      case _32BIT:
      {
        int32_t num = 0;
        for (int j = 0; j < 32; j += 8)
          num |= recv_buf[i++] << j;
        uint8_t dec_places = tag & DECIMAL_POINT_MASK;
        int32_t integer = num / quick_pow10(dec_places);
        uint32_t fraction = abs(num) % quick_pow10(dec_places);
        char num_str[18];
        snprintf(num_str, sizeof(num_str), "%i.%0*u", integer, dec_places, fraction);
        if (integer == 0 && num < 0)
          result += '-';
        result += String(num_str);
        if (i != recv_buf[BYTE_COUNT] && recv_buf[i] != JSON_LEVEL_UP)
          result += ", ";
        continue;
      }
      }
    }
    }
  }
  result += '}';
  return result;
}

// Быстрое возведение 10 в степень
int64_t quick_pow10(int n)
{
  static int64_t pow10[] = {
      1, 10, 100, 1000,
      10000, 100000, 1000000, 10000000,
      100000000, 1000000000, 10000000000, 100000000000,
      1000000000000, 10000000000000, 100000000000000, 1000000000000000};
  if (n < 0 || n >= (sizeof(pow10) / sizeof(pow10[0])))
    return 1;
  return pow10[n];
}
